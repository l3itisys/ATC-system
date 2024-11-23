#include "common/constants.h"

namespace atc {
namespace constants {

// Airspace boundaries
const double AIRSPACE_X_MIN = 0.0;
const double AIRSPACE_X_MAX = 100000.0;
const double AIRSPACE_Y_MIN = 0.0;
const double AIRSPACE_Y_MAX = 100000.0;
const double AIRSPACE_Z_MIN = 15000.0;  // 15,000ft
const double AIRSPACE_Z_MAX = 25000.0;  // 25,000ft

// Separation minimums
const double MIN_HORIZONTAL_SEPARATION = 3000.0;
const double MIN_VERTICAL_SEPARATION = 1000.0;

// Update intervals (in milliseconds)
const int POSITION_UPDATE_INTERVAL = 1000;       // 1s
const int DISPLAY_UPDATE_INTERVAL = 5000;        // 5s
const int HISTORY_LOGGING_INTERVAL = 30000;      // 30s
const int VIOLATION_CHECK_INTERVAL = 1000;       // 1s

// Thread priorities
const int RADAR_PRIORITY = 20;
const int VIOLATION_CHECK_PRIORITY = 18;
const int AIRCRAFT_UPDATE_PRIORITY = 16;
const int DISPLAY_PRIORITY = 14;
const int LOGGING_PRIORITY = 12;
const int OPERATOR_PRIORITY = 10;

// Violation prediction
const int DEFAULT_LOOKAHEAD_TIME = 180;         // 3 minutes in seconds
const int MAX_LOOKAHEAD_TIME = 300;             // 5 minutes max

// Aircraft performance limits
const double MIN_SPEED = 150.0;
const double MAX_SPEED = 500.0;

// Display settings
const int DISPLAY_GRID_WIDTH = 50;
const int DISPLAY_GRID_HEIGHT = 25;
const int DISPLAY_UPDATE_MIN_INTERVAL = 1000;
const int DISPLAY_UPDATE_MAX_INTERVAL = 10000;

} // namespace constants
} // namespace atc
