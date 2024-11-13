#ifndef ATC_CONSTANTS_H
#define ATC_CONSTANTS_H

namespace atc {
namespace constants {

// Airspace boundaries
constexpr double AIRSPACE_X_MIN = 0.0;
constexpr double AIRSPACE_X_MAX = 100000.0;
constexpr double AIRSPACE_Y_MIN = 0.0;
constexpr double AIRSPACE_Y_MAX = 100000.0;
constexpr double AIRSPACE_Z_MIN = 15000.0;  // 15,000ft
constexpr double AIRSPACE_Z_MAX = 25000.0;  // 25,000ft

// Separation minimums
constexpr double MIN_HORIZONTAL_SEPARATION = 3000.0;
constexpr double MIN_VERTICAL_SEPARATION = 1000.0;

// Update intervals (in ms)
constexpr int POSITION_UPDATE_INTERVAL = 1000;    // 1s
constexpr int DISPLAY_UPDATE_INTERVAL = 5000;     // 5s
constexpr int HISTORY_LOGGING_INTERVAL = 30000;   // 30s
constexpr int VIOLATION_CHECK_INTERVAL = 1000;    // 1s

// Thread priorities (higher number = higher priority)
constexpr int RADAR_PRIORITY = 20;           // Highest priority
constexpr int VIOLATION_CHECK_PRIORITY = 18;
constexpr int AIRCRAFT_UPDATE_PRIORITY = 16;
constexpr int DISPLAY_PRIORITY = 14;
constexpr int LOGGING_PRIORITY = 12;         // Lowest priority

// Violation prediction
constexpr int DEFAULT_LOOKAHEAD_TIME = 180;  // 3 minutes in seconds
constexpr int MAX_LOOKAHEAD_TIME = 300;      // 5 minutes max

} // namespace constants
} // namespace atc

#endif // ATC_CONSTANTS_H
