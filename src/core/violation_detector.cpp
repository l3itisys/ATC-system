#include "core/violation_detector.h"
#include "common/constants.h"
#include "common/logger.h"
#include <sstream>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace atc {

ViolationDetector::ViolationDetector()
    : PeriodicTask(std::chrono::milliseconds(constants::VIOLATION_CHECK_INTERVAL),
                   constants::VIOLATION_CHECK_PRIORITY)
    , lookahead_time_seconds_(constants::DEFAULT_LOOKAHEAD_TIME) {
    Logger::getInstance().log("Violation detector initialized with lookahead time: " +
                            std::to_string(lookahead_time_seconds_) + " seconds");
}

void ViolationDetector::addAircraft(const std::shared_ptr<Aircraft>& aircraft) {
    std::lock_guard<std::mutex> lock(mutex_);
    aircraft_.push_back(aircraft);
}

void ViolationDetector::removeAircraft(const std::string& callsign) {
    std::lock_guard<std::mutex> lock(mutex_);
    aircraft_.erase(
        std::remove_if(aircraft_.begin(), aircraft_.end(),
            [&callsign](const auto& aircraft) {
                return aircraft->getState().callsign == callsign;
            }),
        aircraft_.end());
}

void ViolationDetector::setLookaheadTime(int seconds) {
    if (seconds > 0 && seconds <= constants::MAX_LOOKAHEAD_TIME) {
        lookahead_time_seconds_ = seconds;
        Logger::getInstance().log("Lookahead time set to: " + std::to_string(seconds) + " seconds");
    } else {
        Logger::getInstance().log("Invalid lookahead time: " + std::to_string(seconds));
    }
}

bool ViolationDetector::canIssueWarning(const std::string& ac1, const std::string& ac2) {
    std::time_t now = std::time(nullptr);

    // Always keep aircraft IDs in consistent order
    std::string first_ac = std::min(ac1, ac2);
    std::string second_ac = std::max(ac1, ac2);

    auto it = std::find_if(warnings_.begin(), warnings_.end(),
        [&first_ac, &second_ac](const WarningRecord& record) {
            return record.aircraft1 == first_ac && record.aircraft2 == second_ac;
        });

    if (it != warnings_.end()) {
        if (std::difftime(now, it->last_warning) < WARNING_COOLDOWN) {
            return false;
        }
        it->last_warning = now;
    } else {
        warnings_.push_back({first_ac, second_ac, now});
    }

    return true;
}

void ViolationDetector::cleanupWarnings() {
    std::time_t now = std::time(nullptr);
    warnings_.erase(
        std::remove_if(warnings_.begin(), warnings_.end(),
            [now](const WarningRecord& record) {
                return std::difftime(now, record.last_warning) > WARNING_COOLDOWN * 2;
            }),
        warnings_.end());
}

void ViolationDetector::execute() {
    checkViolations();
}

void ViolationDetector::checkViolations() {
    std::lock_guard<std::mutex> lock(mutex_);
    cleanupWarnings();
    bool critical_situation = false;

    for (size_t i = 0; i < aircraft_.size(); ++i) {
        for (size_t j = i + 1; j < aircraft_.size(); ++j) {
            auto state1 = aircraft_[i]->getState();
            auto state2 = aircraft_[j]->getState();

            // Calculate current separation
            double dx = state1.position.x - state2.position.x;
            double dy = state1.position.y - state2.position.y;
            double dz = std::abs(state1.position.z - state2.position.z);

            double horizontal_separation = std::sqrt(dx * dx + dy * dy);
            double vertical_separation = std::abs(dz);

            // Calculate separation ratios
            double h_ratio = horizontal_separation / constants::MIN_HORIZONTAL_SEPARATION;
            double v_ratio = vertical_separation / constants::MIN_VERTICAL_SEPARATION;
            double separation_ratio = std::min(h_ratio, v_ratio);

            if (separation_ratio < CRITICAL_WARNING_THRESHOLD &&
                canIssueWarning(state1.callsign, state2.callsign)) {

                if (separation_ratio < 1.0) {
                    // Immediate violation
                    ViolationInfo violation;
                    if (checkPairViolation(state1, state2, violation)) {
                        handleImmediateViolation(violation);
                        critical_situation = true;
                    }
                } else {
                    // Potential future violation
                    auto prediction = predictViolation(state1, state2);
                    if (prediction.time_to_violation < lookahead_time_seconds_) {
                        if (separation_ratio < CRITICAL_WARNING_THRESHOLD) {
                            handleCriticalWarning(prediction);
                            critical_situation = true;
                        } else if (separation_ratio < MEDIUM_WARNING_THRESHOLD) {
                            handleMediumWarning(prediction);
                        } else if (separation_ratio < EARLY_WARNING_THRESHOLD) {
                            handleEarlyWarning(prediction);
                        }
                    }
                }
            }
        }
    }

    // Adjust update frequency based on situation
    if (critical_situation) {
        setPeriod(std::chrono::milliseconds(500));
    } else {
        setPeriod(std::chrono::milliseconds(constants::VIOLATION_CHECK_INTERVAL));
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

    // Calculate positions at minimum separation time
    Position pos1_future = predictPosition(state1, time_to_min);
    Position pos2_future = predictPosition(state2, time_to_min);

    double dx = pos1_future.x - pos2_future.x;
    double dy = pos1_future.y - pos2_future.y;
    prediction.min_separation = std::sqrt(dx*dx + dy*dy);

    // Calculate conflict point
    prediction.conflict_point = {
        (pos1_future.x + pos2_future.x) / 2,
        (pos1_future.y + pos2_future.y) / 2,
        (pos1_future.z + pos2_future.z) / 2
    };

    prediction.resolution_options = generateResolutionOptions(state1, state2);

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

std::vector<std::string> ViolationDetector::generateResolutionOptions(
    const AircraftState& state1,
    const AircraftState& state2) const {

    std::vector<std::string> options;

    // Add vertical separation options
    double vertical_diff = state1.position.z - state2.position.z;
    if (std::abs(vertical_diff) < constants::MIN_VERTICAL_SEPARATION * 1.5) {
        if (vertical_diff > 0) {
            options.push_back(state1.callsign + ": Climb 1000 feet");
            options.push_back(state2.callsign + ": Descend 1000 feet");
        } else {
            options.push_back(state1.callsign + ": Descend 1000 feet");
            options.push_back(state2.callsign + ": Climb 1000 feet");
        }
    }

    // Add speed adjustment options
    double speed1 = state1.getSpeed();
    double speed2 = state2.getSpeed();
    if (std::abs(speed1 - speed2) < 50.0) {
        options.push_back(state1.callsign + ": Increase speed by 50 units");
        options.push_back(state2.callsign + ": Decrease speed by 50 units");
    }

    // Add heading change options
    double heading_diff = std::abs(state1.heading - state2.heading);
    if (heading_diff < 45.0) {
        options.push_back(state1.callsign + ": Turn right 30 degrees");
        options.push_back(state2.callsign + ": Turn left 30 degrees");
    }

    return options;
}

void ViolationDetector::handleImmediateViolation(const ViolationInfo& violation) {
    logViolation(violation);

    std::ostringstream oss;
    oss << "\nIMMEDIATE VIOLATION - TAKE ACTION NOW!\n"
        << "Aircraft: " << violation.aircraft1_id << " and " << violation.aircraft2_id << "\n"
        << "Current separation: \n"
        << "  Horizontal: " << std::fixed << std::setprecision(1)
        << violation.horizontal_separation << " units\n"
        << "  Vertical: " << violation.vertical_separation << " units\n"
        << "Required immediate actions:\n"
        << "1. Establish vertical separation\n"
        << "2. Turn " << violation.aircraft1_id << " right\n"
        << "3. Turn " << violation.aircraft2_id << " left\n"
        << "4. Increase speed differential";

    Logger::getInstance().log(oss.str());
}

void ViolationDetector::handleCriticalWarning(const ViolationPrediction& prediction) {
    std::ostringstream oss;
    oss << "\nCRITICAL WARNING - Imminent Conflict\n"
        << "Aircraft: " << prediction.aircraft1_id << " and " << prediction.aircraft2_id << "\n"
        << "Time to violation: " << prediction.time_to_violation << " seconds\n"
        << "Minimum separation: " << prediction.min_separation << " units\n"
        << "Recommended actions:";

    for (const auto& option : prediction.resolution_options) {
        oss << "\n- " << option;
    }

    Logger::getInstance().log(oss.str());
}

void ViolationDetector::handleMediumWarning(const ViolationPrediction& prediction) {
    std::ostringstream oss;
    oss << "\nMEDIUM WARNING - Potential Conflict\n"
        << "Aircraft: " << prediction.aircraft1_id << " and " << prediction.aircraft2_id << "\n"
        << "Time to closest approach: " << prediction.time_to_violation << " seconds\n"
        << "Expected minimum separation: " << prediction.min_separation << " units";

    Logger::getInstance().log(oss.str());
}

void ViolationDetector::handleEarlyWarning(const ViolationPrediction& prediction) {
    std::ostringstream oss;
    oss << "\nEARLY WARNING - Monitor Situation\n"
        << "Aircraft: " << prediction.aircraft1_id << " and " << prediction.aircraft2_id << "\n"
        << "Time to closest approach: " << prediction.time_to_violation << " seconds\n"
        << "Expected minimum separation: " << prediction.min_separation << " units";

    Logger::getInstance().log(oss.str());
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
            auto pred = predictViolation(aircraft_[i]->getState(), aircraft_[j]->getState());
            if (pred.time_to_violation < lookahead_time_seconds_ &&
                pred.min_separation < constants::MIN_HORIZONTAL_SEPARATION * CRITICAL_WARNING_THRESHOLD) {
                predictions.push_back(pred);
            }
        }
    }

    // Sort predictions by time to violation
    std::sort(predictions.begin(), predictions.end(),
              [](const ViolationPrediction& a, const ViolationPrediction& b) {
                  return a.time_to_violation < b.time_to_violation;
              });

    return predictions;
}

}
