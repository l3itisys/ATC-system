#ifndef ATC_CONSTANTS_H
#define ATC_CONSTANTS_H

#include <string>

namespace atc {
namespace constants {

// Airspace boundaries
extern const double AIRSPACE_X_MIN;
extern const double AIRSPACE_X_MAX;
extern const double AIRSPACE_Y_MIN;
extern const double AIRSPACE_Y_MAX;
extern const double AIRSPACE_Z_MIN;
extern const double AIRSPACE_Z_MAX;

// Separation minimums
extern const double MIN_HORIZONTAL_SEPARATION;
extern const double MIN_VERTICAL_SEPARATION;

// Update intervals (in milliseconds)
extern const int POSITION_UPDATE_INTERVAL;    // 1s
extern const int DISPLAY_UPDATE_INTERVAL;     // 5s
extern const int HISTORY_LOGGING_INTERVAL;    // 30s
extern const int VIOLATION_CHECK_INTERVAL;    // 1s

// Thread priorities (higher number = higher priority)
extern const int RADAR_PRIORITY;              // Highest priority
extern const int VIOLATION_CHECK_PRIORITY;
extern const int AIRCRAFT_UPDATE_PRIORITY;
extern const int DISPLAY_PRIORITY;            // Lower than critical components
extern const int LOGGING_PRIORITY;            // Lowest priority
extern const int OPERATOR_PRIORITY;           // Operator console priority

// Violation prediction
extern const int DEFAULT_LOOKAHEAD_TIME;     // 3 minutes in seconds
extern const int MAX_LOOKAHEAD_TIME;         // 5 minutes max

// Aircraft performance limits
extern const double MIN_SPEED;               // Minimum safe speed
extern const double MAX_SPEED;               // Maximum allowed speed

// Display settings
extern const int DISPLAY_GRID_WIDTH;
extern const int DISPLAY_GRID_HEIGHT;
extern const int DISPLAY_UPDATE_MIN_INTERVAL;  // Minimum refresh interval (1s)
extern const int DISPLAY_UPDATE_MAX_INTERVAL;  // Maximum refresh interval (10s)

// System version
extern const std::string SYSTEM_VERSION;

} // namespace constants
} // namespace atc

#endif // ATC_CONSTANTS_H

