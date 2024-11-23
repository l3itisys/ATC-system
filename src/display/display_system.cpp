#include "display/display_system.h"
#include "common/constants.h"
#include "common/logger.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <limits>

namespace atc {

DisplaySystem::DisplaySystem(std::shared_ptr<ViolationDetector> violation_detector)
    : PeriodicTask(std::chrono::milliseconds(constants::DISPLAY_UPDATE_INTERVAL),
                   constants::DISPLAY_PRIORITY)
    , violation_detector_(violation_detector) {
    Logger::getInstance().log("Display system initialized with update interval: " +
                            std::to_string(constants::DISPLAY_UPDATE_INTERVAL) + "ms");
}

void DisplaySystem::execute() {
    std::lock_guard<std::mutex> lock(display_mutex_);
    update_count_++;

    clearScreen();
    displayHeader();
    displayLegend();
    displayAircraft();
    displayViolations();
    displayFooter();
    std::cout.flush();
}

void DisplaySystem::clearScreen() const {
    std::cout << "\033[2J\033[H";
}

void DisplaySystem::displayHeader() const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    std::cout << Colors::bold() << "=== Air Traffic Control System ===" << Colors::reset() << std::endl;
    std::cout << "Time: " << std::ctime(&time);
    std::cout << std::string(70, '-') << std::endl;
}

void DisplaySystem::displayLegend() const {
    std::cout << Colors::cyan()
              << "Flight Levels:\n"
              << "  " << Colors::bold() << "UPPERCASE" << Colors::reset() << Colors::cyan()
              << " = High (>21k ft)\n"
              << "  Normal    = Mid  (19k-21k ft)\n"
              << "  lowercase = Low  (<19k ft)\n\n"
              << "Direction Indicators:\n"
              << "  ^ = N, / = NE, > = E, \\ = SE, v = S, / = SW, < = W, \\ = NW\n\n"
              << "Warning Levels:\n"
              << "  " << Colors::yellow() << "●" << Colors::reset() << Colors::cyan()
              << " = Early Warning (200%)\n"
              << "  " << Colors::yellow() << "▲" << Colors::reset() << Colors::cyan()
              << " = Medium Warning (150%)\n"
              << "  " << Colors::yellow() << "■" << Colors::reset() << Colors::cyan()
              << " = Critical Warning (120%)\n"
              << "  " << Colors::red() << "█" << Colors::reset() << Colors::cyan()
              << " = Violation\n"
              << "  " << Colors::blue() << "•" << Colors::reset() << Colors::cyan()
              << " = Predicted Position"
              << Colors::reset() << std::endl;
    std::cout << std::string(70, '-') << std::endl;
}

char DisplaySystem::getDirectionSymbol(double heading) const {
    int index = static_cast<int>((heading + 22.5) / 45.0) % 8;
    return DIRECTION_SYMBOLS[index];
}

const char* DisplaySystem::getWarningColor(WarningLevel level) const {
    switch (level) {
        case WarningLevel::VIOLATION: return Colors::red();
        case WarningLevel::CRITICAL: return Colors::yellow();
        case WarningLevel::MEDIUM: return Colors::dim();
        case WarningLevel::EARLY: return Colors::dim();
        default: return Colors::reset();
    }
}

std::string DisplaySystem::getWarningString(WarningLevel level) const {
    switch (level) {
        case WarningLevel::VIOLATION: return "VIOLATION";
        case WarningLevel::CRITICAL: return "CRITICAL";
        case WarningLevel::MEDIUM: return "WARNING";
        case WarningLevel::EARLY: return "CAUTION";
        default: return "";
    }
}

double DisplaySystem::calculateClosureRate(
    const AircraftState& state1,
    const AircraftState& state2) const {

    double vx = state2.velocity.vx - state1.velocity.vx;
    double vy = state2.velocity.vy - state1.velocity.vy;
    double vz = state2.velocity.vz - state1.velocity.vz;

    return std::sqrt(vx*vx + vy*vy + vz*vz);
}

double DisplaySystem::calculateTimeToClosestApproach(
    const AircraftState& state1,
    const AircraftState& state2) const {

    double dx = state2.position.x - state1.position.x;
    double dy = state2.position.y - state1.position.y;
    double vx = state2.velocity.vx - state1.velocity.vx;
    double vy = state2.velocity.vy - state1.velocity.vy;

    // Time to closest point of approach
    double dotProduct = dx * vx + dy * vy;
    double squareSpeed = vx * vx + vy * vy;

    if (squareSpeed < 1e-6) return 0.0;  // Parallel tracks

    return -dotProduct / squareSpeed;
}

std::pair<double, double> DisplaySystem::calculateSeparation(
    const AircraftState& state1,
    const AircraftState& state2) const {

    double dx = state1.position.x - state2.position.x;
    double dy = state1.position.y - state2.position.y;
    double dz = std::abs(state1.position.z - state2.position.z);

    double horizontal = std::sqrt(dx * dx + dy * dy);
    return {horizontal, dz};
}

std::string DisplaySystem::formatPosition(const Position& pos) const {
    std::ostringstream oss;
    oss << "(" << std::setw(2) << static_cast<int>(pos.x/1000) << ","
        << std::setw(2) << static_cast<int>(pos.y/1000) << ")";
    return oss.str();
}

std::string DisplaySystem::formatSeparation(double horizontal, double vertical) const {
    std::ostringstream oss;
    oss << std::setw(5) << static_cast<int>(horizontal) << "H"
        << std::setw(5) << static_cast<int>(vertical) << "V";
    return oss.str();
}

std::string DisplaySystem::formatAltitude(double altitude) const {
    std::ostringstream oss;
    oss << "FL" << std::setw(3) << std::setfill('0')
        << static_cast<int>(altitude/100);
    return oss.str();
}

void DisplaySystem::displayAircraft() const {
    std::vector<std::vector<AircraftDisplayInfo>> grid(DISPLAY_HEIGHT,
        std::vector<AircraftDisplayInfo>(DISPLAY_WIDTH));

    // First, calculate warning levels for all aircraft pairs
    for (const auto& aircraft : aircraft_) {
        const auto& state = aircraft->getState();

        int x = static_cast<int>((state.position.x / constants::AIRSPACE_X_MAX) * (DISPLAY_WIDTH - 1));
        int y = DISPLAY_HEIGHT - 1 - static_cast<int>((state.position.y / constants::AIRSPACE_Y_MAX) * (DISPLAY_HEIGHT - 1));

        if (x >= 0 && x < DISPLAY_WIDTH && y >= 0 && y < DISPLAY_HEIGHT) {
            AircraftDisplayInfo& cell = grid[y][x];
            cell.marker = state.callsign[0];
            cell.direction = getDirectionSymbol(state.heading);
            cell.occupied = true;
            cell.callsign = state.callsign;
            cell.altitude = state.position.z;
            cell.status = state.status;

            // Calculate warning level
            WarningLevel max_warning = WarningLevel::NONE;
            for (const auto& other : aircraft_) {
                if (other->getState().callsign == state.callsign) continue;

                auto [horiz, vert] = calculateSeparation(state, other->getState());
                double h_ratio = horiz / constants::MIN_HORIZONTAL_SEPARATION;
                double v_ratio = vert / constants::MIN_VERTICAL_SEPARATION;

                if (h_ratio < 1.0 || v_ratio < 1.0) {
                    max_warning = WarningLevel::VIOLATION;
                    break;
                } else if (h_ratio < WARNING_CRITICAL && v_ratio < WARNING_CRITICAL) {
                    max_warning = WarningLevel::CRITICAL;
                } else if (h_ratio < WARNING_MEDIUM && v_ratio < WARNING_MEDIUM &&
                         max_warning < WarningLevel::MEDIUM) {
                    max_warning = WarningLevel::MEDIUM;
                } else if (h_ratio < WARNING_EARLY && v_ratio < WARNING_EARLY &&
                         max_warning < WarningLevel::EARLY) {
                    max_warning = WarningLevel::EARLY;
                }
            }
            cell.warning_level = max_warning;

            // Add predicted position if warning level is Critical or higher
            if (cell.warning_level >= WarningLevel::CRITICAL) {
                Position future = {
                    state.position.x + state.velocity.vx * PREDICTION_TIME,
                    state.position.y + state.velocity.vy * PREDICTION_TIME,
                    state.position.z + state.velocity.vz * PREDICTION_TIME
                };

                int pred_x = static_cast<int>((future.x / constants::AIRSPACE_X_MAX) * (DISPLAY_WIDTH - 1));
                int pred_y = DISPLAY_HEIGHT - 1 - static_cast<int>((future.y / constants::AIRSPACE_Y_MAX) * (DISPLAY_HEIGHT - 1));

                if (pred_x >= 0 && pred_x < DISPLAY_WIDTH && pred_y >= 0 && pred_y < DISPLAY_HEIGHT &&
                    (pred_x != x || pred_y != y)) {
                    grid[pred_y][pred_x].occupied = true;
                    grid[pred_y][pred_x].marker = PREDICTED_POSITION_MARKER;
                    grid[pred_y][pred_x].is_predicted = true;
                }
            }
        }
    }

    // Display grid
    std::cout << "+" << std::string(DISPLAY_WIDTH * 2 + 2, '-') << "+" << std::endl;

    for (const auto& row : grid) {
        std::cout << "| ";
        for (const auto& cell : row) {
            if (cell.occupied) {
                if (cell.is_predicted) {
                    std::cout << Colors::blue() << cell.marker << " " << Colors::reset();
                } else {
                    const char* color = getWarningColor(cell.warning_level);

                    // Show altitude with case
                    char marker = cell.marker;
                    if (cell.altitude > 21000) marker = std::toupper(marker);
                    else if (cell.altitude < 19000) marker = std::tolower(marker);

                    std::cout << color << marker << cell.direction << Colors::reset();
                }
            } else {
                std::cout << "  ";
            }
        }
        std::cout << " |" << std::endl;
    }

    std::cout << "+" << std::string(DISPLAY_WIDTH * 2 + 2, '-') << "+" << std::endl;
    displayAircraftDetails();
}

void DisplaySystem::displayAircraftDetails() const {
    if (aircraft_.empty()) return;

    std::cout << "\nAircraft Details:" << std::endl;
    std::cout << std::string(96, '-') << std::endl;
    std::cout << std::setw(8) << "ID"
              << std::setw(10) << "Alt(FL)"
              << std::setw(8) << "Speed"
              << std::setw(8) << "Hdg"
              << std::setw(10) << "Status"
              << std::setw(16) << "Position"
              << std::setw(12) << "Nearest"
              << std::setw(12) << "Distance"
              << std::setw(12) << "Closure" << std::endl;
    std::cout << std::string(96, '-') << std::endl;

    for (const auto& aircraft : aircraft_) {
        const auto& state = aircraft->getState();

        // Find nearest aircraft and separation
        double min_horizontal = std::numeric_limits<double>::max();
        double min_vertical = std::numeric_limits<double>::max();
        std::string nearest_ac = "None";
        double closure_rate = 0;

        for (const auto& other : aircraft_) {
            if (other->getState().callsign == state.callsign) continue;

            const auto& other_state = other->getState();
            auto [horizontal, vertical] = calculateSeparation(state, other_state);

            if (horizontal < min_horizontal) {
                min_horizontal = horizontal;
                min_vertical = vertical;
                nearest_ac = other_state.callsign;
                closure_rate = calculateClosureRate(state, other_state);
            }
        }

        // Determine warning color based on separation
        const char* color = Colors::reset();
        std::string warning_indicator = "";

        if (min_horizontal < constants::MIN_HORIZONTAL_SEPARATION ||
            min_vertical < constants::MIN_VERTICAL_SEPARATION) {
            color = Colors::red();
            warning_indicator = " !!!";
        } else if (min_horizontal < constants::MIN_HORIZONTAL_SEPARATION * WARNING_CRITICAL) {
            color = Colors::yellow();
            warning_indicator = " !";
        } else if (min_horizontal < constants::MIN_HORIZONTAL_SEPARATION * WARNING_MEDIUM) {
            color = Colors::dim();
            warning_indicator = " ^";
        }

        std::cout << color
                  << std::setw(8) << state.callsign
                  << std::setw(10) << static_cast<int>(state.position.z/100)
                  << std::setw(8) << static_cast<int>(state.getSpeed())
                  << std::setw(8) << static_cast<int>(state.heading)
                  << std::setw(10) << Aircraft::getStatusString(state.status)
                  << std::setw(16) << formatPosition(state.position)
                  << std::setw(12) << nearest_ac;

        if (min_horizontal < std::numeric_limits<double>::max()) {
            std::cout << std::setw(6) << static_cast<int>(min_horizontal)
                     << "/"
                     << std::setw(5) << static_cast<int>(min_vertical);
        } else {
            std::cout << std::setw(12) << "-";
        }

        std::cout << std::setw(12) << static_cast<int>(closure_rate)
                  << warning_indicator
                  << Colors::reset() << std::endl;
    }
}

void DisplaySystem::displayViolations() const {
    auto violations = violation_detector_->getCurrentViolations();
    if (!violations.empty()) {
        std::cout << "\n" << Colors::red() << Colors::bold()
                  << "!!! SEPARATION VIOLATIONS DETECTED !!!"
                  << Colors::reset() << std::endl;
        std::cout << std::string(70, '!') << std::endl;

        for (const auto& violation : violations) {
            // Find states
            AircraftState state1, state2;
            for (const auto& aircraft : aircraft_) {
                auto state = aircraft->getState();
                if (state.callsign == violation.aircraft1_id) state1 = state;
                if (state.callsign == violation.aircraft2_id) state2 = state;
            }

            double closure_rate = calculateClosureRate(state1, state2);
            double time_to_closest = calculateTimeToClosestApproach(state1, state2);
            auto [horiz_sep, vert_sep] = calculateSeparation(state1, state2);

            // Display conflict details
            std::cout << Colors::yellow() << "\nCONFLICT ANALYSIS:" << Colors::reset() << std::endl;
            std::cout << "Aircraft Pair: " << violation.aircraft1_id
                     << " and " << violation.aircraft2_id << std::endl;

            std::cout << "\nCurrent Situation:"
                     << "\n  " << violation.aircraft1_id << ": "
                     << formatPosition(state1.position) << " FL"
                     << static_cast<int>(state1.position.z/100)
                     << " HDG " << static_cast<int>(state1.heading)
                     << "\n  " << violation.aircraft2_id << ": "
                     << formatPosition(state2.position) << " FL"
                     << static_cast<int>(state2.position.z/100)
                     << " HDG " << static_cast<int>(state2.heading)
                     << "\n\nSeparation Analysis:"
                     << "\n  Horizontal: " << std::fixed << std::setprecision(1)
                     << horiz_sep << " units ("
                     << (horiz_sep / constants::MIN_HORIZONTAL_SEPARATION * 100)
                     << "% of minimum)"
                     << "\n  Vertical: " << vert_sep << " units ("
                     << (vert_sep / constants::MIN_VERTICAL_SEPARATION * 100)
                     << "% of minimum)"
                     << "\n  Closure Rate: " << closure_rate << " units/s"
                     << "\n  Time to CPA: " << time_to_closest << "s";

            if (violation.is_predicted) {
                double time_until = (violation.prediction_time - violation.timestamp) / 1000.0;
                std::cout << Colors::yellow()
                         << "\n\nPREDICTED VIOLATION:"
                         << "\n  Time until: " << time_until << "s"
                         << Colors::reset();

                // Show predicted positions
                Position pos1_future = {
                    state1.position.x + state1.velocity.vx * time_until,
                    state1.position.y + state1.velocity.vy * time_until,
                    state1.position.z + state1.velocity.vz * time_until
                };
                Position pos2_future = {
                    state2.position.x + state2.velocity.vx * time_until,
                    state2.position.y + state2.velocity.vy * time_until,
                    state2.position.z + state2.velocity.vz * time_until
                };

                std::cout << "\n  Predicted Positions:"
                         << "\n    " << violation.aircraft1_id << ": "
                         << formatPosition(pos1_future) << " FL"
                         << static_cast<int>(pos1_future.z/100)
                         << "\n    " << violation.aircraft2_id << ": "
                         << formatPosition(pos2_future) << " FL"
                         << static_cast<int>(pos2_future.z/100);

                // Resolution suggestions
                int alt_diff = static_cast<int>(std::abs(state1.position.z - state2.position.z));
                std::cout << "\n\nResolution Options:";
                if (alt_diff < constants::MIN_VERTICAL_SEPARATION) {
                    std::cout << "\n  - Immediate altitude change of "
                             << (constants::MIN_VERTICAL_SEPARATION - alt_diff)
                             << " feet required";
                }
                std::cout << "\n  - Vector " << (state1.heading < state2.heading ? state1.callsign : state2.callsign)
                         << " right for lateral separation";
            } else {
                std::cout << Colors::red() << Colors::bold()
                         << "\n\n!!! IMMEDIATE VIOLATION - TAKE ACTION NOW !!!"
                         << Colors::reset()
                         << "\nRequired Actions:"
                         << "\n  1. Immediate vertical separation required"
                         << "\n  2. Turn " << violation.aircraft1_id << " right"
                         << "\n  3. Turn " << violation.aircraft2_id << " left"
                         << "\n  4. Increase speed of leading aircraft";
            }
            std::cout << std::endl << std::string(70, '-') << std::endl;
        }
    }
}

void DisplaySystem::displayFooter() const {
    std::cout << "\n" << std::string(70, '-') << std::endl;
    std::cout << "Aircraft Count: " << aircraft_.size()
              << " | Update Count: " << update_count_
              << " | Update Rate: " << constants::DISPLAY_UPDATE_INTERVAL << "ms"
              << " | Press Ctrl+C to exit" << std::endl;
    std::cout << std::string(70, '-') << std::endl;
}

void DisplaySystem::addAircraft(const std::vector<std::shared_ptr<Aircraft>>& new_aircraft) {
    std::lock_guard<std::mutex> lock(display_mutex_);
    for (const auto& aircraft : new_aircraft) {
        auto it = std::find_if(aircraft_.begin(), aircraft_.end(),
            [&](const auto& existing) {
                return existing->getState().callsign == aircraft->getState().callsign;
            });

        if (it == aircraft_.end()) {
            aircraft_.push_back(aircraft);
        }
    }
}

void DisplaySystem::displayAlert(const std::string& alert_message) {
    std::lock_guard<std::mutex> lock(display_mutex_);
    current_alert_message_ = alert_message;
    std::cout << Colors::red() << Colors::bold()
              << "ALERT: " << alert_message
              << Colors::reset() << std::endl;
}

void DisplaySystem::updateDisplay(const std::vector<std::shared_ptr<Aircraft>>& current_aircraft) {
    std::lock_guard<std::mutex> lock(display_mutex_);
    aircraft_ = current_aircraft;  // Update the entire aircraft list
    execute();  // Refresh the display
}

void DisplaySystem::removeAircraft(const std::string& callsign) {
    std::lock_guard<std::mutex> lock(display_mutex_);
    auto it = std::remove_if(aircraft_.begin(), aircraft_.end(),
        [&](const auto& aircraft) {
            return aircraft->getState().callsign == callsign;
        });
    aircraft_.erase(it, aircraft_.end());
}

}
