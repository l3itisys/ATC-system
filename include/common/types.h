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

    double distanceTo(const Position& other) const {
        double dx = x - other.x;
        double dy = y - other.y;
        return std::sqrt(dx*dx + dy*dy);
    }
};

struct Velocity {
    double vx{0}, vy{0}, vz{0};

    double getSpeed() const {
        return std::sqrt(vx*vx + vy*vy + vz*vz);
    }

    void setFromSpeedAndHeading(double speed, double heading_deg) {
        double heading_rad = heading_deg * M_PI / 180.0;
        vx = speed * std::cos(90.0 - heading_rad);
        vy = speed * std::sin(90.0 - heading_rad);
    }
};

enum class AircraftStatus : uint8_t {
    ENTERING,
    CRUISING,
    HOLDING,
    EXITING,
    EMERGENCY
};

struct AircraftState {
    std::string callsign;
    Position position;
    Velocity velocity;
    double heading{0};    // degrees
    AircraftStatus status{AircraftStatus::ENTERING};
    uint64_t timestamp{0};

    void updateHeading() {
        heading = std::atan2(velocity.vy, velocity.vx) * 180.0 / M_PI;
        if (heading < 0) heading += 360.0;
    }

    void updateTimestamp() {
        timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    double getSpeed() const {
        return velocity.getSpeed();
    }
};

}
#endif // ATC_TYPES_H
