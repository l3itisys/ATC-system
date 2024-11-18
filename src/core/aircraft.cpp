#include "core/aircraft.h"
#include <iostream>
#include <iomanip>

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

    // Log initial state
    logState("Aircraft initialized", state_);
}

void Aircraft::logState(const std::string& event, const AircraftState& state) {
    std::cout << "\n=== " << event << " ===" << std::endl;
    std::cout << "Aircraft: " << state.callsign << "\n"
              << std::fixed << std::setprecision(2)
              << "Position: (" << state.position.x << ", "
              << state.position.y << ", "
              << state.position.z << ")\n"
              << "Speed: " << state.getSpeed() << " units/s\n"
              << "Heading: " << state.heading << " degrees\n"
              << "Status: " << getStatusString(state.status) << "\n"
              << "Timestamp: " << state.timestamp << std::endl;
}

std::string Aircraft::getStatusString(AircraftStatus status) {
    switch (status) {
        case AircraftStatus::ENTERING: return "ENTERING";
        case AircraftStatus::CRUISING: return "CRUISING";
        case AircraftStatus::HOLDING: return "HOLDING";
        case AircraftStatus::EXITING: return "EXITING";
        case AircraftStatus::EMERGENCY: return "EMERGENCY";
        default: return "UNKNOWN";
    }
}

void Aircraft::execute() {
    try {
        updatePosition();

        // Log periodic state update
        std::lock_guard<std::mutex> lock(state_mutex_);
        static int update_count = 0;
        if (++update_count % 5 == 0) {  // Log every 5th update to avoid spam
            logState("Periodic Update", state_);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error updating aircraft " << state_.callsign
                  << " position: " << e.what() << std::endl;
        declareEmergency();
    }
}

bool Aircraft::updateSpeed(double new_speed) {
    if (!validateSpeed(new_speed)) {
        std::cerr << "Invalid speed value: " << new_speed << std::endl;
        return false;
    }

    try {
        std::lock_guard<std::mutex> lock(state_mutex_);
        double heading = state_.heading;
        state_.velocity.setFromSpeedAndHeading(new_speed, heading);
        state_.updateTimestamp();
        logState("Speed Updated", state_);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error updating speed: " << e.what() << std::endl;
        return false;
    }
}

bool Aircraft::updateHeading(double new_heading) {
    if (new_heading < 0 || new_heading >= 360) {
        std::cerr << "Invalid heading value: " << new_heading << std::endl;
        return false;
    }

    try {
        std::lock_guard<std::mutex> lock(state_mutex_);
        double speed = state_.getSpeed();
        state_.velocity.setFromSpeedAndHeading(speed, new_heading);
        state_.heading = new_heading;
        state_.updateTimestamp();
        logState("Heading Updated", state_);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error updating heading: " << e.what() << std::endl;
        return false;
    }
}

bool Aircraft::updateAltitude(double new_altitude) {
    if (!validateAltitude(new_altitude)) {
        std::cerr << "Invalid altitude value: " << new_altitude << std::endl;
        return false;
    }

    try {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_.position.z = new_altitude;
        state_.updateTimestamp();
        logState("Altitude Updated", state_);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error updating altitude: " << e.what() << std::endl;
        return false;
    }
}

void Aircraft::declareEmergency() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    state_.status = AircraftStatus::EMERGENCY;
    logState("Emergency Declared", state_);
    std::cout << "Aircraft " << state_.callsign << " declaring emergency!" << std::endl;
}

void Aircraft::cancelEmergency() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    state_.status = AircraftStatus::CRUISING;
    logState("Emergency Cancelled", state_);
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

        // Update status if needed
        if (state_.status == AircraftStatus::ENTERING) {
            state_.status = AircraftStatus::CRUISING;
            logState("Status Change", state_);
        }
    } else {
        state_.status = AircraftStatus::EXITING;
        logState("Aircraft Exiting Airspace", state_);
        stop();
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
