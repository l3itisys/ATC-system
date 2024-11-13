#include "core/aircraft.h"
#include "common/constants.h"
#include <cmath>
#include <iostream>

namespace atc {

Aircraft::Aircraft(const std::string& callsign,
                 const Position& initial_pos,
                 const Velocity& initial_vel,
                 const FlightCharacteristics& characteristics)
    : PeriodicTask(std::chrono::milliseconds(constants::POSITION_UPDATE_INTERVAL),
                  constants::AIRCRAFT_UPDATE_PRIORITY)
    , characteristics_(characteristics) {
    state_.callsign = callsign;
    state_.position = initial_pos;
    state_.velocity = initial_vel;
    state_.speed = initial_vel.getSpeed();
    state_.altitude = initial_pos.z;
    state_.updateHeading();
    state_.updateTimestamp();

    // Validate initial position
    if (!initial_pos.isValid()) {
        throw std::invalid_argument("Initial position outside valid airspace");
    }
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
    if (new_speed < characteristics_.min_speed ||
        new_speed > characteristics_.max_speed) {
        return false;
    }

    try {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_.speed = new_speed;
        state_.velocity.setFromSpeedAndHeading(new_speed, state_.heading);
        state_.updateTimestamp();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error updating aircraft speed: " << e.what() << std::endl;
        return false;
    }
}

bool Aircraft::updateHeading(double new_heading) {
    if (new_heading < 0 || new_heading >= 360) {
        return false;
    }

    try {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_.heading = new_heading;
        state_.velocity.setFromSpeedAndHeading(state_.speed, new_heading);
        state_.updateTimestamp();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error updating aircraft heading: " << e.what() << std::endl;
        return false;
    }
}

bool Aircraft::updateAltitude(double new_altitude) {
    if (new_altitude < characteristics_.min_altitude ||
        new_altitude > characteristics_.max_altitude) {
        return false;
    }

    try {
        std::lock_guard<std::mutex> lock(state_mutex_);
        double max_altitude_change = (constants::POSITION_UPDATE_INTERVAL / 1000.0) *
            (new_altitude > state_.altitude ?
             characteristics_.max_climb_rate : characteristics_.max_descent_rate);

        double altitude_change = std::min(std::abs(new_altitude - state_.altitude),
                                        max_altitude_change);

        if (new_altitude > state_.altitude) {
            state_.position.z += altitude_change;
        } else {
            state_.position.z -= altitude_change;
        }

        state_.altitude = state_.position.z;
        state_.updateTimestamp();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error updating aircraft altitude: " << e.what() << std::endl;
        return false;
    }
}

void Aircraft::declareEmergency() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    state_.status = AircraftStatus::EMERGENCY;
    state_.alert_level = 2;
    std::cout << "Aircraft " << state_.callsign << " declaring emergency!" << std::endl;
}

void Aircraft::cancelEmergency() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    state_.status = AircraftStatus::CRUISING;
    state_.alert_level = 0;
    std::cout << "Aircraft " << state_.callsign << " emergency cancelled." << std::endl;
}

AircraftState Aircraft::getState() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return state_;
}

void Aircraft::updatePosition() {
    std::lock_guard<std::mutex> lock(state_mutex_);

    double dt = constants::POSITION_UPDATE_INTERVAL / 1000.0;  // Convert to seconds

    // Update position based on velocity
    Position new_pos = {
        state_.position.x + state_.velocity.vx * dt,
        state_.position.y + state_.velocity.vy * dt,
        state_.position.z + state_.velocity.vz * dt
    };

    // Check if new position is valid
    if (new_pos.isValid()) {
        state_.position = new_pos;
        state_.altitude = new_pos.z;
        state_.updateTimestamp();
    } else {
        // Aircraft is leaving the airspace
        state_.status = AircraftStatus::EXITING;
        stop();  // Stop the periodic task

        // Log the exit
        std::cout << "Aircraft " << state_.callsign
                  << " is exiting airspace at position ("
                  << new_pos.x << ", " << new_pos.y << ", " << new_pos.z
                  << ")" << std::endl;
    }
}

}
