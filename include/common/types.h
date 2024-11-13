#ifndef ATC_TYPES_H
#define ATC_TYPES_H

#include <string>
#include <chrono>
#include <cstdint>
#include <cmath>
#include "constants.h"

namespace atc {

struct Position {
    double x{0}, y{0}, z{0};

    bool isValid() const {
        return x >= constants::AIRSPACE_X_MIN && x <= constants::AIRSPACE_X_MAX &&
               y >= constants::AIRSPACE_Y_MIN && y <= constants::AIRSPACE_Y_MAX &&
               z >= constants::AIRSPACE_Z_MIN && z <= constants::AIRSPACE_Z_MAX;
    }
};

struct Velocity {
    double vx{0}, vy{0}, vz{0};

    // Calculate speed
    double getSpeed() const {
        return ::sqrt(vx*vx + vy*vy + vz*vz);
    }

    // Set velocity from speed and heading
    void setFromSpeedAndHeading(double speed, double heading_deg) {
        double heading_rad = heading_deg * M_PI / 180.0;
        vx = speed * ::cos(heading_rad);
        vy = speed * ::sin(heading_rad);
        // vz remains unchanged
    }
};

enum class AircraftStatus {
    ENTERING,   // Just entered the airspace
    CRUISING,   // Normal flight
    HOLDING,    // Maintaining position
    EXITING,    // About to exit airspace
    EMERGENCY   // Emergency situation
};

enum class AircraftType {
    COMMERCIAL,
    PRIVATE
};

struct AircraftState {
    std::string callsign;
    Position position;
    Velocity velocity;
    double heading{0};    // in degrees
    double speed{0};      // current speed
    double altitude{0};   // current altitude (same as position.z)
    AircraftStatus status{AircraftStatus::ENTERING};
    uint64_t timestamp{0};
    uint8_t alert_level{0};

    // Calculate heading from velocity
    void updateHeading() {
        heading = ::atan2(velocity.vy, velocity.vx) * 180.0 / M_PI;
        if (heading < 0) heading += 360.0;
    }

    // Update timestamp to current time
    void updateTimestamp() {
        timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
};

// Flight characteristics for different aircraft types
struct FlightCharacteristics {
    std::string model;
    AircraftType type;
    double cruise_speed;
    double max_speed;
    double min_speed;
    double max_altitude;
    double min_altitude;
    double max_climb_rate;    // feet per minute
    double max_descent_rate;  // feet per minute
};

// Violation detection types
struct SeparationViolation {
    std::string aircraft1_callsign;
    std::string aircraft2_callsign;
    double horizontal_separation;
    double vertical_separation;
    uint64_t timestamp;
    bool is_predicted{false};  // true if this is a predicted future violation
};

}

#endif // ATC_TYPES_H
