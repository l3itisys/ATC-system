#include "core/aircraft.h"
#include "common/constants.h"
#include "common/logger.h"
#include <sstream>
#include <iomanip>
#include <cmath>

namespace atc {

Aircraft::Aircraft(const std::string& callsign,
                   const Position& initial_pos,
                   const Velocity& initial_vel)
    : PeriodicTask(std::chrono::milliseconds(constants::POSITION_UPDATE_INTERVAL),
                   constants::AIRCRAFT_UPDATE_PRIORITY) {
    try {
        // Validate initial position
        if (!initial_pos.isValid()) {
            throw std::invalid_argument("Initial position outside valid airspace");
        }

        // Initialize state with thread safety
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            state_.callsign = callsign;
            state_.position = initial_pos;
            state_.velocity = initial_vel;
            state_.status = AircraftStatus::ENTERING;
            state_.updateHeading();
            state_.updateTimestamp();
        }

        // Log initialization
        std::ostringstream oss;
        oss << "\n=== Aircraft initialized ===\n"
            << "Aircraft: " << callsign << "\n"
            << std::fixed << std::setprecision(2)
            << "Position: (" << initial_pos.x << ", "
            << initial_pos.y << ", " << initial_pos.z << ")\n"
            << "Speed: " << std::sqrt(initial_vel.vx * initial_vel.vx +
                                    initial_vel.vy * initial_vel.vy +
                                    initial_vel.vz * initial_vel.vz) << " units/s\n"
            << "Heading: " << state_.heading << " degrees\n"
            << "Status: " << getStatusString(state_.status) << "\n"
            << "Timestamp: " << state_.timestamp;
        Logger::getInstance().log(oss.str());

    } catch (const std::exception& e) {
        Logger::getInstance().log("Failed to initialize aircraft " + callsign +
                                ": " + std::string(e.what()));
        throw;
    }
}

void Aircraft::execute() {
    try {
        auto start_time = std::chrono::steady_clock::now();

        // Update position
        updatePosition();

        // Log periodic updates (every 5th update to avoid excessive logging)
        static int update_count = 0;
        if (++update_count % 5 == 0) {
            std::lock_guard<std::mutex> lock(state_mutex_);
            logState("Periodic Update", state_);
        }

        // Update execution time statistics
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time).count();
        updateExecutionStats(duration);

    } catch (const std::exception& e) {
        Logger::getInstance().log("Error updating aircraft " + state_.callsign +
                                ": " + std::string(e.what()));
        declareEmergency();
    }
}

void Aircraft::updatePosition() {
    std::lock_guard<std::mutex> lock(state_mutex_);

    // Calculate time delta in seconds
    double dt = constants::POSITION_UPDATE_INTERVAL / 1000.0;

    // Calculate new position
    Position new_pos{
        state_.position.x + state_.velocity.vx * dt,
        state_.position.y + state_.velocity.vy * dt,
        state_.position.z + state_.velocity.vz * dt
    };

    // Update state if new position is valid
    if (new_pos.isValid()) {
        state_.position = new_pos;
        state_.updateTimestamp();

        // Update status if needed
        if (state_.status == AircraftStatus::ENTERING) {
            state_.status = AircraftStatus::CRUISING;
            logState("Status Change", state_);
        }
    } else {
        // Aircraft is leaving airspace
        state_.status = AircraftStatus::EXITING;
        logState("Aircraft Exiting Airspace", state_);
        stop();  // Stop periodic updates
    }
}

void Aircraft::declareEmergency() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    state_.status = AircraftStatus::EMERGENCY;
    logState("Emergency Declared", state_);
    Logger::getInstance().log("Aircraft " + state_.callsign + " declaring emergency!");
}

void Aircraft::cancelEmergency() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (state_.status == AircraftStatus::EMERGENCY) {
        state_.status = AircraftStatus::CRUISING;
        logState("Emergency Cancelled", state_);
    }
}

bool Aircraft::updateSpeed(double new_speed) {
    if (!validateSpeed(new_speed)) {
        Logger::getInstance().log("Invalid speed value for " + state_.callsign +
                                ": " + std::to_string(new_speed));
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
        Logger::getInstance().log("Error updating speed: " + std::string(e.what()));
        return false;
    }
}

bool Aircraft::updateHeading(double new_heading) {
    if (new_heading < 0 || new_heading >= 360) {
        Logger::getInstance().log("Invalid heading value for " + state_.callsign +
                                ": " + std::to_string(new_heading));
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
        Logger::getInstance().log("Error updating heading: " + std::string(e.what()));
        return false;
    }
}

bool Aircraft::updateAltitude(double new_altitude) {
    if (!validateAltitude(new_altitude)) {
        Logger::getInstance().log("Invalid altitude value for " + state_.callsign +
                                ": " + std::to_string(new_altitude));
        return false;
    }

    try {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_.position.z = new_altitude;
        state_.updateTimestamp();
        logState("Altitude Updated", state_);
        return true;
    } catch (const std::exception& e) {
        Logger::getInstance().log("Error updating altitude: " + std::string(e.what()));
        return false;
    }
}

AircraftState Aircraft::getState() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return state_;
}

std::string Aircraft::getStatusString(AircraftStatus status) {
    switch (status) {
        case AircraftStatus::ENTERING:   return "ENTERING";
        case AircraftStatus::CRUISING:   return "CRUISING";
        case AircraftStatus::HOLDING:    return "HOLDING";
        case AircraftStatus::EXITING:    return "EXITING";
        case AircraftStatus::EMERGENCY:  return "EMERGENCY";
        default:                         return "UNKNOWN";
    }
}

void Aircraft::logState(const std::string& event, const AircraftState& state) {
    std::ostringstream oss;
    oss << "\n=== " << event << " ===\n"
        << "Aircraft: " << state.callsign << "\n"
        << std::fixed << std::setprecision(2)
        << "Position: (" << state.position.x << ", "
        << state.position.y << ", "
        << state.position.z << ")\n"
        << "Speed: " << state.getSpeed() << " units/s\n"
        << "Heading: " << state.heading << " degrees\n"
        << "Status: " << getStatusString(state.status) << "\n"
        << "Timestamp: " << state.timestamp;
    Logger::getInstance().log(oss.str());
}

bool Aircraft::validateSpeed(double speed) const {
    return speed >= constants::MIN_SPEED && speed <= constants::MAX_SPEED;
}

bool Aircraft::validateAltitude(double altitude) const {
    return altitude >= constants::AIRSPACE_Z_MIN &&
           altitude <= constants::AIRSPACE_Z_MAX;
}

} // namespace atc
