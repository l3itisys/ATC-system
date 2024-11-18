#include "core/violation_detector.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <algorithm>

namespace atc {

ViolationDetector::ViolationDetector(int lookahead_seconds)
    : PeriodicTask(std::chrono::milliseconds(constants::VIOLATION_CHECK_INTERVAL),
                   constants::VIOLATION_CHECK_PRIORITY)
    , lookahead_time_seconds_(lookahead_seconds) {
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
    if (seconds >= 0 && seconds <= constants::MAX_LOOKAHEAD_TIME) {
        std::lock_guard<std::mutex> lock(mutex_);
        lookahead_time_seconds_ = seconds;
    }
}

int ViolationDetector::getLookaheadTime() const {
    return lookahead_time_seconds_;
}

void ViolationDetector::execute() {
    auto violations = checkViolations();
    for (const auto& violation : violations) {
        logViolation(violation);
    }
}

std::vector<ViolationInfo> ViolationDetector::checkViolations() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ViolationInfo> violations;

    for (size_t i = 0; i < aircraft_.size(); ++i) {
        for (size_t j = i + 1; j < aircraft_.size(); ++j) {
            ViolationInfo violation;
            auto state1 = aircraft_[i]->getState();
            auto state2 = aircraft_[j]->getState();

            if (state1.status == AircraftStatus::EXITING ||
                state2.status == AircraftStatus::EXITING) {
                continue;
            }

            if (checkPairViolation(state1, state2, violation)) {
                violations.push_back(violation);
            }
            else if (checkFutureViolation(state1, state2, violation)) {
                violations.push_back(violation);
            }
        }
    }
    return violations;
}

bool ViolationDetector::checkPairViolation(
    const AircraftState& state1,
    const AircraftState& state2,
    ViolationInfo& violation) const {

    double horiz_dist = std::sqrt(
        std::pow(state1.position.x - state2.position.x, 2) +
        std::pow(state1.position.y - state2.position.y, 2));
    double vert_dist = std::abs(state1.position.z - state2.position.z);

    if (horiz_dist < constants::MIN_HORIZONTAL_SEPARATION) {
        violation.aircraft1_id = state1.callsign;
        violation.aircraft2_id = state2.callsign;
        violation.horizontal_separation = horiz_dist;
        violation.vertical_separation = vert_dist;
        violation.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        violation.is_predicted = false;
        return true;
    }
    return false;
}

bool ViolationDetector::checkFutureViolation(
    const AircraftState& state1,
    const AircraftState& state2,
    ViolationInfo& violation) const {

    // Check at 10-second intervals
    for (int t = 10; t <= lookahead_time_seconds_; t += 10) {
        Position pos1 = predictPosition(state1, t);
        Position pos2 = predictPosition(state2, t);

        double horiz_dist = std::sqrt(
            std::pow(pos1.x - pos2.x, 2) +
            std::pow(pos1.y - pos2.y, 2));
        double vert_dist = std::abs(pos1.z - pos2.z);

        if (horiz_dist < constants::MIN_HORIZONTAL_SEPARATION) {
            violation.aircraft1_id = state1.callsign;
            violation.aircraft2_id = state2.callsign;
            violation.horizontal_separation = horiz_dist;
            violation.vertical_separation = vert_dist;
            violation.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            violation.is_predicted = true;
            violation.predicted_position1 = pos1;
            violation.predicted_position2 = pos2;
            violation.prediction_time = violation.timestamp + (t * 1000);
            return true;
        }
    }
    return false;
}

Position ViolationDetector::predictPosition(
    const AircraftState& state,
    double time_seconds) const {

    return Position{
        state.position.x + (state.velocity.vx * time_seconds),
        state.position.y + (state.velocity.vy * time_seconds),
        state.position.z + (state.velocity.vz * time_seconds)
    };
}

void ViolationDetector::logViolation(const ViolationInfo& violation) const {
    std::cout << "\n----------------------------------------" << std::endl;
    std::cout << "!!! SEPARATION VIOLATION ALERT !!!" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Aircraft:     " << violation.aircraft1_id
              << " and " << violation.aircraft2_id << std::endl;
    std::cout << "Horizontal:   " << std::fixed << std::setprecision(1)
              << violation.horizontal_separation << " units" << std::endl;
    std::cout << "Vertical:     " << violation.vertical_separation
              << " units" << std::endl;

    if (violation.is_predicted) {
        double time_until = (violation.prediction_time - violation.timestamp) / 1000.0;
        std::cout << "WARNING:      Predicted violation in " << time_until
                  << " seconds" << std::endl;
    } else {
        std::cout << "*** IMMEDIATE ACTION REQUIRED - CURRENT VIOLATION ***" << std::endl;
    }
    std::cout << "----------------------------------------\n" << std::endl;
}

std::vector<ViolationInfo> ViolationDetector::getCurrentViolations() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ViolationInfo> violations;
    for (size_t i = 0; i < aircraft_.size(); ++i) {
        for (size_t j = i + 1; j < aircraft_.size(); ++j) {
            ViolationInfo violation;
            auto state1 = aircraft_[i]->getState();
            auto state2 = aircraft_[j]->getState();
            if (checkPairViolation(state1, state2, violation)) {
                violations.push_back(violation);
            }
        }
    }
    return violations;
}

}
