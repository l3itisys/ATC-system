#include "core/aircraft.h"
#include <iostream>

namespace atc {

Aircraft::Aircraft(const std::string& callsign,
                 const Position& initial_pos,
                 const Velocity& initial_vel)
    : PeriodicTask(std::chrono::milliseconds(constants::POSITION_UPDATE_INTERVAL),
                   constants::AIRCRAFT_UPDATE_PRIORITY) {

    if (!initial_pos.isValid()) {
        throw std::invalid_argument("Initial position outside valid airspace");
    }

    state_.callsign = callsign;
    state_.position = initial_pos;
    state_.velocity = initial_vel;
    state_.updateHeading();
    state_.updateTimestamp();
}

void Aircraft::execute() {
    try {
        updatePosition();
    } catch (const std::exception& e) {
        std::cerr << "Error updating aircraft " << state_.callsign
                  << " position: " << e.what() << std::endl;
        declareEmergency();
    }
}

bool Aircraft::updateSpeed(double new_speed) {
    if (!validateSpeed(new_speed)) {
        return false;
    }

    try {
        std::lock_guard<std::mutex> lock(state_mutex_);
        double heading = state_.heading;
        state_.velocity.setFromSpeedAndHeading(new_speed, heading);
        state_.updateTimestamp();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error updating speed: " << e.what() << std::endl;
        return false;
    }
}

bool Aircraft::updateHeading(double new_heading) {
    if (new_heading < 0 || new_heading >= 360) {
        return false;
    }

    try {
        std::lock_guard<std::mutex> lock(state_mutex_);
        double speed = state_.getSpeed();
        state_.velocity.setFromSpeedAndHeading(speed, new_heading);
        state_.heading = new_heading;
        state_.updateTimestamp();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error updating heading: " << e.what() << std::endl;
        return false;
    }
}

bool Aircraft::updateAltitude(double new_altitude) {
    if (!validateAltitude(new_altitude)) {
        return false;
    }

    try {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_.position.z = new_altitude;
        state_.updateTimestamp();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error updating altitude: " << e.what() << std::endl;
        return false;
    }
}

void Aircraft::declareEmergency() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    state_.status = AircraftStatus::EMERGENCY;
    std::cout << "Aircraft " << state_.callsign << " declaring emergency!" << std::endl;
}

void Aircraft::cancelEmergency() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    state_.status = AircraftStatus::CRUISING;
    std::cout << "Aircraft " << state_.callsign << " emergency cancelled." << std::endl;
}

AircraftState Aircraft::getState() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return state_;
}

void Aircraft::updatePosition() {
    std::lock_guard<std::mutex> lock(state_mutex_);

    double dt = constants::POSITION_UPDATE_INTERVAL / 1000.0;  // Convert to seconds

    Position new_pos = {
        state_.position.x + state_.velocity.vx * dt,
        state_.position.y + state_.velocity.vy * dt,
        state_.position.z + state_.velocity.vz * dt
    };

    if (new_pos.isValid()) {
        state_.position = new_pos;
        state_.updateTimestamp();
    } else {
        state_.status = AircraftStatus::EXITING;
        stop();

        std::cout << "Aircraft " << state_.callsign
                  << " exiting airspace at position ("
                  << new_pos.x << ", " << new_pos.y << ", " << new_pos.z
                  << ")" << std::endl;
    }
}

bool Aircraft::validateSpeed(double speed) const {
    static constexpr double MIN_SPEED = 150.0;  // Minimum safe speed
    static constexpr double MAX_SPEED = 500.0;  // Maximum allowed speed
    return speed >= MIN_SPEED && speed <= MAX_SPEED;
}

bool Aircraft::validateAltitude(double altitude) const {
    return altitude >= constants::AIRSPACE_Z_MIN &&
           altitude <= constants::AIRSPACE_Z_MAX;
}

}
