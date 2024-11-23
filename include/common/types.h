#ifndef ATC_TYPES_H
#define ATC_TYPES_H
#include <string>
#include <cmath>
#include <chrono>

namespace atc {
namespace constants {
    extern const double AIRSPACE_X_MIN;
    extern const double AIRSPACE_X_MAX;
    extern const double AIRSPACE_Y_MIN;
    extern const double AIRSPACE_Y_MAX;
    extern const double AIRSPACE_Z_MIN;
    extern const double AIRSPACE_Z_MAX;
}

// Define AirspaceBoundary first
struct AirspaceBoundary {
    static bool isWithinLimits(double x, double y, double z) {
        return x >= constants::AIRSPACE_X_MIN && x <= constants::AIRSPACE_X_MAX &&
               y >= constants::AIRSPACE_Y_MIN && y <= constants::AIRSPACE_Y_MAX &&
               z >= constants::AIRSPACE_Z_MIN && z <= constants::AIRSPACE_Z_MAX;
    }
};

struct Position {
    double x;
    double y;
    double z;
    bool isValid() const {
        return AirspaceBoundary::isWithinLimits(x, y, z);  // Updated to use coordinate values
    }
};

struct Velocity {
    double vx;
    double vy;
    double vz;
    void setFromSpeedAndHeading(double speed, double heading_degrees) {
        double heading_radians = heading_degrees * M_PI / 180.0;
        vx = speed * cos(heading_radians);
        vy = speed * sin(heading_radians);
        // vz remains unchanged unless explicitly modified
    }
};

enum class AircraftStatus {
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
    double heading;         // in degrees
    AircraftStatus status;
    double timestamp;       // in milliseconds since epoch

    double getSpeed() const {
        return std::sqrt(velocity.vx * velocity.vx +
                        velocity.vy * velocity.vy +
                        velocity.vz * velocity.vz);
    }

    void updateHeading() {
        heading = atan2(velocity.vy, velocity.vx) * 180.0 / M_PI;
        if (heading < 0) heading += 360.0;
    }

    void updateTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }
};

struct ViolationInfo {
    std::string aircraft1_id;
    std::string aircraft2_id;
    double horizontal_separation;
    double vertical_separation;
    bool is_predicted;
    double prediction_time;  // in milliseconds since epoch
    double timestamp;        // time when the violation was detected or predicted
};

}
#endif // ATC_TYPES_H
