#include "core/violation_detector.h"
#include "common/constants.h"
#include "common/logger.h"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

namespace atc {

ViolationDetector::ViolationDetector(std::shared_ptr<comm::QnxChannel> channel)
    : PeriodicTask(std::chrono::milliseconds(constants::VIOLATION_CHECK_INTERVAL),
                   constants::VIOLATION_CHECK_PRIORITY)
    , channel_(channel)
    , lookahead_time_seconds_(constants::DEFAULT_LOOKAHEAD_TIME)
    , last_resolution_time_(std::chrono::steady_clock::now()) {

    Logger::getInstance().log("Violation detector initialized with lookahead time: " +
                            std::to_string(lookahead_time_seconds_) + " seconds");
}

void ViolationDetector::addAircraft(const std::shared_ptr<Aircraft>& aircraft) {
    std::lock_guard<std::mutex> lock(mutex_);
    aircraft_.push_back(aircraft);
    Logger::getInstance().log("Added aircraft to violation detector: " +
                            aircraft->getState().callsign);
}

void ViolationDetector::removeAircraft(const std::string& callsign) {
    std::lock_guard<std::mutex> lock(mutex_);
    aircraft_.erase(
        std::remove_if(aircraft_.begin(), aircraft_.end(),
            [&callsign](const auto& aircraft) {
                return aircraft->getState().callsign == callsign;
            }),
        aircraft_.end());
    Logger::getInstance().log("Removed aircraft from violation detector: " + callsign);
}

void ViolationDetector::setLookaheadTime(int seconds) {
    if (seconds > 0 && seconds <= constants::MAX_LOOKAHEAD_TIME) {
        lookahead_time_seconds_ = seconds;
        Logger::getInstance().log("Updated lookahead time to: " +
                                std::to_string(seconds) + " seconds");
    }
}

void ViolationDetector::execute() {
    checkViolations();

    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();

    while (!conflict_queue_.empty()) {
        auto& conflict = conflict_queue_.front();

        if (!conflict.resolution_attempted &&
            std::chrono::duration_cast<std::chrono::seconds>(
                now - conflict.detection_time).count() >= WARNING_COOLDOWN) {

            handlePredictedConflict(conflict.prediction);
            conflict.resolution_attempted = true;
        }

        if (std::chrono::duration_cast<std::chrono::seconds>(
            now - conflict.detection_time).count() > lookahead_time_seconds_) {
            conflict_queue_.pop();
        } else {
            break;
        }
    }
}

void ViolationDetector::handlePredictedConflict(const ViolationPrediction& prediction) {
    std::ostringstream oss;

    if (prediction.time_to_violation < 30) {
        oss << "\nCRITICAL WARNING - Imminent Conflict\n";
    } else if (prediction.time_to_violation < 60) {
        oss << "\nMEDIUM WARNING - Potential Conflict\n";
    } else {
        oss << "\nEARLY WARNING - Monitor Situation\n";
    }

    oss << "Aircraft: " << prediction.aircraft1_id << " and "
        << prediction.aircraft2_id << "\n"
        << "Time to violation: " << std::fixed << std::setprecision(1)
        << prediction.time_to_violation << " seconds\n"
        << "Minimum separation: " << prediction.min_separation << " units\n";

    Logger::getInstance().log(oss.str());
}

void ViolationDetector::checkViolations() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (size_t i = 0; i < aircraft_.size(); ++i) {
        for (size_t j = i + 1; j < aircraft_.size(); ++j) {
            try {
                auto state1 = aircraft_[i]->getState();
                auto state2 = aircraft_[j]->getState();

                ViolationInfo current_violation;
                if (checkPairViolation(state1, state2, current_violation)) {
                    handleImmediateViolation(current_violation);
                    continue;
                }

                auto prediction = predictViolation(state1, state2);
                if (prediction.time_to_violation < lookahead_time_seconds_ &&
                    prediction.min_separation < constants::MIN_HORIZONTAL_SEPARATION * EARLY_WARNING_THRESHOLD) {

                    auto actions = calculateResolutionActions(state1, state2, prediction);

                    conflict_queue_.push({
                        prediction,
                        actions,
                        std::chrono::steady_clock::now(),
                        false
                    });

                    if (prediction.requires_immediate_action) {
                        executeResolutionActions(actions);
                    }
                }
            } catch (const std::exception& e) {
                Logger::getInstance().log("Error checking violations: " + std::string(e.what()));
            }
        }
    }
}

bool ViolationDetector::checkPairViolation(
    const AircraftState& state1,
    const AircraftState& state2,
    ViolationInfo& violation) const {

    double dx = state1.position.x - state2.position.x;
    double dy = state1.position.y - state2.position.y;
    double dz = std::abs(state1.position.z - state2.position.z);

    double horizontal_separation = std::sqrt(dx * dx + dy * dy);

    if (horizontal_separation < constants::MIN_HORIZONTAL_SEPARATION &&
        dz < constants::MIN_VERTICAL_SEPARATION) {

        violation.aircraft1_id = state1.callsign;
        violation.aircraft2_id = state2.callsign;
        violation.horizontal_separation = horizontal_separation;
        violation.vertical_separation = dz;
        violation.is_predicted = false;
        violation.timestamp = state1.timestamp;
        violation.prediction_time = 0;

        return true;
    }

    return false;
}

ViolationDetector::ViolationPrediction ViolationDetector::predictViolation(
    const AircraftState& state1,
    const AircraftState& state2) const {

    ViolationPrediction prediction;
    prediction.aircraft1_id = state1.callsign;
    prediction.aircraft2_id = state2.callsign;

    double time_to_min = calculateTimeToMinimumSeparation(state1, state2);
    prediction.time_to_violation = time_to_min;

    Position pos1_future = predictPosition(state1, time_to_min);
    Position pos2_future = predictPosition(state2, time_to_min);

    double dx = pos1_future.x - pos2_future.x;
    double dy = pos1_future.y - pos2_future.y;
    prediction.min_separation = std::sqrt(dx*dx + dy*dy);

    prediction.conflict_point = {
        (pos1_future.x + pos2_future.x) / 2,
        (pos1_future.y + pos2_future.y) / 2,
        (pos1_future.z + pos2_future.z) / 2
    };

    prediction.requires_immediate_action =
        (prediction.time_to_violation < 30) ||
        (prediction.min_separation < constants::MIN_HORIZONTAL_SEPARATION * IMMEDIATE_ACTION_THRESHOLD);

    return prediction;
}

Position ViolationDetector::predictPosition(
    const AircraftState& state,
    double time_seconds) const {

    return Position{
        state.position.x + state.velocity.vx * time_seconds,
        state.position.y + state.velocity.vy * time_seconds,
        state.position.z + state.velocity.vz * time_seconds
    };
}

double ViolationDetector::calculateTimeToMinimumSeparation(
    const AircraftState& state1,
    const AircraftState& state2) const {

    double dx = state2.position.x - state1.position.x;
    double dy = state2.position.y - state1.position.y;
    double dvx = state2.velocity.vx - state1.velocity.vx;
    double dvy = state2.velocity.vy - state1.velocity.vy;

    double a = dvx * dvx + dvy * dvy;
    if (std::abs(a) < 1e-6) return 0.0;  // Parallel tracks

    double b = 2.0 * (dx * dvx + dy * dvy);
    double time = -b / (2.0 * a);

    return (time < 0.0) ? 0.0 : time;
}

void ViolationDetector::handleImmediateViolation(const ViolationInfo& violation) {
    std::ostringstream oss;
    oss << "\nIMMEDIATE VIOLATION - TAKE ACTION NOW!\n"
        << "Aircraft: " << violation.aircraft1_id << " and " << violation.aircraft2_id << "\n"
        << "Current separation: \n"
        << "  Horizontal: " << std::fixed << std::setprecision(1)
        << violation.horizontal_separation << " units\n"
        << "  Vertical: " << violation.vertical_separation << " units\n";

    Logger::getInstance().log(oss.str());

    // Attempt immediate resolution
    auto state1 = std::find_if(aircraft_.begin(), aircraft_.end(),
        [&](const auto& ac) { return ac->getState().callsign == violation.aircraft1_id; });
    auto state2 = std::find_if(aircraft_.begin(), aircraft_.end(),
        [&](const auto& ac) { return ac->getState().callsign == violation.aircraft2_id; });

    if (state1 != aircraft_.end() && state2 != aircraft_.end()) {
        auto actions = calculateResolutionActions(
            (*state1)->getState(),
            (*state2)->getState(),
            ViolationPrediction{violation.aircraft1_id, violation.aircraft2_id, 0,
                              violation.horizontal_separation, {}, true});
        executeResolutionActions(actions);
    }
}

void ViolationDetector::logViolation(const ViolationInfo& violation) const {
    std::ostringstream oss;
    oss << "\n=== VIOLATION REPORT ===\n"
        << "Time: " << violation.timestamp << "\n"
        << "Aircraft pair: " << violation.aircraft1_id
        << " - " << violation.aircraft2_id << "\n"
        << "Separation:\n"
        << "  Horizontal: " << violation.horizontal_separation << " units\n"
        << "  Vertical: " << violation.vertical_separation << " units\n"
        << "Status: " << (violation.is_predicted ? "PREDICTED" : "CURRENT") << "\n"
        << "======================\n";

    Logger::getInstance().log(oss.str());
}

std::vector<ViolationInfo> ViolationDetector::getCurrentViolations() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ViolationInfo> violations;

    for (size_t i = 0; i < aircraft_.size(); ++i) {
        for (size_t j = i + 1; j < aircraft_.size(); ++j) {
            ViolationInfo violation;
            if (checkPairViolation(aircraft_[i]->getState(),
                                 aircraft_[j]->getState(),
                                 violation)) {
                violations.push_back(violation);
            }
        }
    }
    return violations;
}

std::vector<ViolationDetector::ViolationPrediction>
ViolationDetector::getPredictedViolations() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ViolationPrediction> predictions;

    for (size_t i = 0; i < aircraft_.size(); ++i) {
        for (size_t j = i + 1; j < aircraft_.size(); ++j) {
            auto prediction = predictViolation(
                aircraft_[i]->getState(),
                aircraft_[j]->getState());

            if (prediction.time_to_violation < lookahead_time_seconds_ &&
                prediction.min_separation < constants::MIN_HORIZONTAL_SEPARATION * EARLY_WARNING_THRESHOLD) {
                predictions.push_back(prediction);
            }
        }
    }

    std::sort(predictions.begin(), predictions.end(),
              [](const ViolationPrediction& a, const ViolationPrediction& b) {
                  return a.time_to_violation < b.time_to_violation;
              });

    return predictions;
}

} // namespace atc
