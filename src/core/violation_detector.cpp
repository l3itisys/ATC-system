#include "core/violation_detector.h"
#include "common/logger.h"
#include <sstream>
#include <cmath>

namespace atc {

ViolationDetector::ViolationDetector(std::shared_ptr<comm::QnxChannel> channel)
    : PeriodicTask(std::chrono::milliseconds(1000), 20)  // 1 second period, high priority
    , channel_(channel)
    , violation_checks_count_(0)
    , violations_detected_(0) {
    Logger::getInstance().log("Violation detector initialized");
}

void ViolationDetector::execute() {
    try {
        checkViolations();
    } catch (const std::exception& e) {
        Logger::getInstance().log("Error in violation detection: " + std::string(e.what()));
    }
}

void ViolationDetector::checkViolations() {
    std::lock_guard<std::mutex> lock(mutex_);
    violation_checks_count_++;

    for (size_t i = 0; i < aircraft_.size(); ++i) {
        if (!aircraft_[i]) continue;

        auto state1 = aircraft_[i]->getState();

        for (size_t j = i + 1; j < aircraft_.size(); ++j) {
            if (!aircraft_[j]) continue;

            auto state2 = aircraft_[j]->getState();

            // Check current violations
            ViolationInfo violation;
            if (checkPairViolation(state1, state2, violation)) {
                violations_detected_++;
                handleImmediateViolation(violation);
                continue;
            }

            // Check predicted violations
            auto prediction = predictViolation(state1, state2);
            if (validatePrediction(prediction)) {
                handlePredictedConflict(prediction);
            }
        }
    }
}

ViolationDetector::Metrics ViolationDetector::getMetrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return Metrics{
        violation_checks_count_,
        violations_detected_
    };
}

void ViolationDetector::handleImmediateViolation(const ViolationInfo& violation) {
    // Log violation
    std::ostringstream oss;
    oss << "\nIMMEDIATE VIOLATION DETECTED!\n"
        << "Aircraft: " << violation.aircraft1_id << " and "
        << violation.aircraft2_id << "\n"
        << "Separation: H=" << violation.horizontal_separation
        << "m, V=" << violation.vertical_separation << "m";
    Logger::getInstance().log(oss.str());

    // Calculate and execute resolution actions
    auto actions = calculateResolutionActions(
        ViolationPrediction{
            violation.aircraft1_id,
            violation.aircraft2_id,
            0.0,  // immediate violation
            violation.horizontal_separation,
            Position{0,0,0},  // not used for immediate violations
            true  // requires immediate action
        }
    );

    for (const auto& action : actions) {
        executeResolutionAction(action);
    }

    // Send alert
    if (channel_) {
        comm::AlertData alert(comm::alerts::LEVEL_EMERGENCY, oss.str());
        channel_->sendMessage(comm::Message::createAlert("VIOLATION_DETECTOR", alert));
    }
}

void ViolationDetector::handlePredictedConflict(const ViolationPrediction& prediction) {
    auto now = std::chrono::steady_clock::now();

    // Check warning cooldown
    auto it = last_warning_times_.find(prediction.aircraft1_id + prediction.aircraft2_id);
    if (it != last_warning_times_.end()) {
        auto time_since_last = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second).count();
        if (time_since_last < WARNING_COOLDOWN) {
            return;
        }
    }

    last_warning_times_[prediction.aircraft1_id + prediction.aircraft2_id] = now;

    // Calculate and execute resolutions
    auto actions = calculateResolutionActions(prediction);
    for (const auto& action : actions) {
        executeResolutionAction(action);
    }

    // Log and alert
    std::ostringstream oss;
    oss << "\nPREDICTED VIOLATION in " << prediction.time_to_violation << "s\n"
        << "Aircraft: " << prediction.aircraft1_id << " and "
        << prediction.aircraft2_id << "\n"
        << "Minimum separation: " << prediction.min_separation << "m";
    Logger::getInstance().log(oss.str());

    if (channel_ && prediction.requires_immediate_action) {
        comm::AlertData alert(comm::alerts::LEVEL_CRITICAL, oss.str());
        channel_->sendMessage(comm::Message::createAlert("VIOLATION_DETECTOR", alert));
    }
}

std::vector<ViolationDetector::ResolutionAction>
ViolationDetector::calculateResolutionActions(const ViolationPrediction& prediction) {
    std::vector<ResolutionAction> actions;

    // Find aircraft states
    AircraftState state1, state2;
    bool found1 = false, found2 = false;

    for (const auto& aircraft : aircraft_) {
        if (!aircraft) continue;

        auto state = aircraft->getState();
        if (state.callsign == prediction.aircraft1_id) {
            state1 = state;
            found1 = true;
        } else if (state.callsign == prediction.aircraft2_id) {
            state2 = state;
            found2 = true;
        }
    }

    if (!found1 || !found2) return actions;

    // Calculate vertical separation
    if (state1.position.z <= state2.position.z) {
        actions.push_back(ResolutionAction{
            state1.callsign,
            ResolutionAction::Type::ALTITUDE_CHANGE,
            state1.position.z - MIN_VERTICAL_SEPARATION,
            prediction.requires_immediate_action,
            0.9,
            "Altitude change required"
        });
    } else {
        actions.push_back(ResolutionAction{
            state2.callsign,
            ResolutionAction::Type::ALTITUDE_CHANGE,
            state2.position.z - MIN_VERTICAL_SEPARATION,
            prediction.requires_immediate_action,
            0.9,
            "Altitude change required"
        });
    }

    return actions;
}

void ViolationDetector::executeResolutionAction(const ResolutionAction& action) {
    if (!validateResolutionAction(action) || !channel_) return;

    comm::CommandData cmd_data(action.aircraft_id, "");

    switch (action.action_type) {
        case ResolutionAction::Type::ALTITUDE_CHANGE:
            cmd_data.command = comm::commands::CMD_ALTITUDE;
            break;
        case ResolutionAction::Type::SPEED_CHANGE:
            cmd_data.command = comm::commands::CMD_SPEED;
            break;
        case ResolutionAction::Type::HEADING_CHANGE:
            cmd_data.command = comm::commands::CMD_HEADING;
            break;
        case ResolutionAction::Type::EMERGENCY_STOP:
            cmd_data.command = comm::commands::CMD_EMERGENCY;
            cmd_data.params.push_back(comm::commands::EMERGENCY_ON);
            break;
    }

    if (cmd_data.command != comm::commands::CMD_EMERGENCY) {
        cmd_data.params.push_back(std::to_string(action.value));
    }

    if (cmd_data.isValid()) {
        channel_->sendMessage(comm::Message::createCommand("VIOLATION_DETECTOR", cmd_data));
        Logger::getInstance().log("Resolution action sent: " + action.description);
    }
}

ViolationDetector::ViolationPrediction
ViolationDetector::predictViolation(const AircraftState& state1,
                                  const AircraftState& state2) const {
    ViolationPrediction prediction;
    prediction.aircraft1_id = state1.callsign;
    prediction.aircraft2_id = state2.callsign;

    prediction.time_to_violation = calculateTimeToMinimumSeparation(state1, state2);

    if (prediction.time_to_violation >= 0) {
        auto pos1 = predictPosition(state1, prediction.time_to_violation);
        auto pos2 = predictPosition(state2, prediction.time_to_violation);

        double dx = pos1.x - pos2.x;
        double dy = pos1.y - pos2.y;
        prediction.min_separation = std::sqrt(dx * dx + dy * dy);
        prediction.conflict_point = {
            (pos1.x + pos2.x) / 2,
            (pos1.y + pos2.y) / 2,
            (pos1.z + pos2.z) / 2
        };

        prediction.requires_immediate_action =
            prediction.min_separation < MIN_HORIZONTAL_SEPARATION * 1.2 &&
            prediction.time_to_violation < 60.0;
    }

    return prediction;
}

Position ViolationDetector::predictPosition(const AircraftState& state,
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
    if (std::abs(a) < 1e-6) return 0.0;

    double b = 2.0 * (dx * dvx + dy * dvy);
    double c = dx * dx + dy * dy - MIN_HORIZONTAL_SEPARATION * MIN_HORIZONTAL_SEPARATION;

    double discriminant = b * b - 4 * a * c;
    if (discriminant < 0) return -1.0;

    double t1 = (-b + std::sqrt(discriminant)) / (2.0 * a);
    double t2 = (-b - std::sqrt(discriminant)) / (2.0 * a);

    if (t1 < 0 && t2 < 0) return -1.0;
    if (t1 < 0) return t2;
    if (t2 < 0) return t1;
    return std::min(t1, t2);
}

bool ViolationDetector::checkPairViolation(const AircraftState& state1,
                                        const AircraftState& state2,
                                        ViolationInfo& violation) const {
    double dx = state1.position.x - state2.position.x;
    double dy = state1.position.y - state2.position.y;
    double horizontal_separation = std::sqrt(dx * dx + dy * dy);
    double vertical_separation = std::abs(state1.position.z - state2.position.z);

    if (horizontal_separation < MIN_HORIZONTAL_SEPARATION &&
        vertical_separation < MIN_VERTICAL_SEPARATION) {

        violation.aircraft1_id = state1.callsign;
        violation.aircraft2_id = state2.callsign;
        violation.horizontal_separation = horizontal_separation;
        violation.vertical_separation = vertical_separation;
        violation.is_predicted = false;
        violation.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return true;
    }
    return false;
}

bool ViolationDetector::validatePrediction(const ViolationPrediction& prediction) const {
    if (!prediction.isValid()) return false;

    // Check time bounds
    if (prediction.time_to_violation < 0 ||
        prediction.time_to_violation > PREDICTION_WINDOW) {
        return false;
    }

    // Check if conflict point is within airspace
    if (!prediction.conflict_point.isValid()) {
        return false;
    }

    return true;
}

bool ViolationDetector::validateResolutionAction(const ResolutionAction& action) const {
    if (!action.isValid()) return false;

    switch (action.action_type) {
        case ResolutionAction::Type::ALTITUDE_CHANGE:
            return action.value >= constants::AIRSPACE_Z_MIN &&
                   action.value <= constants::AIRSPACE_Z_MAX;

        case ResolutionAction::Type::SPEED_CHANGE:
            return action.value >= constants::MIN_SPEED &&
                   action.value <= constants::MAX_SPEED;

        case ResolutionAction::Type::HEADING_CHANGE:
            return action.value >= 0.0 && action.value < 360.0;

        case ResolutionAction::Type::EMERGENCY_STOP:
            return true;
    }

    return false;
}

void ViolationDetector::addAircraft(const std::shared_ptr<Aircraft>& aircraft) {
    if (!aircraft) return;

    std::lock_guard<std::mutex> lock(mutex_);
    aircraft_.push_back(aircraft);
    Logger::getInstance().log("Added aircraft to violation detector: " +
                            aircraft->getState().callsign);
}

void ViolationDetector::removeAircraft(const std::string& callsign) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Remove from aircraft vector
    aircraft_.erase(
        std::remove_if(aircraft_.begin(), aircraft_.end(),
            [&callsign](const auto& ac) {
                return ac && ac->getState().callsign == callsign;
            }),
        aircraft_.end());

    // Remove from warning times
    last_warning_times_.erase(callsign);

    Logger::getInstance().log("Removed aircraft from violation detector: " + callsign);
}

bool ViolationDetector::hasActiveViolations() const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (size_t i = 0; i < aircraft_.size(); ++i) {
        if (!aircraft_[i]) continue;
        auto state1 = aircraft_[i]->getState();

        for (size_t j = i + 1; j < aircraft_.size(); ++j) {
            if (!aircraft_[j]) continue;
            auto state2 = aircraft_[j]->getState();

            ViolationInfo violation;
            if (checkPairViolation(state1, state2, violation)) {
                return true;
            }
        }
    }
    return false;
}

int ViolationDetector::getActiveAircraftCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::count_if(aircraft_.begin(), aircraft_.end(),
        [](const auto& ac) { return ac != nullptr; });
}

} // namespace atc
