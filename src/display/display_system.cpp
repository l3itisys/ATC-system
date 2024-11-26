#include "display/display_system.h"
#include "common/logger.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace atc {

// Define display constants if not in constants.h
namespace {
    constexpr int DISPLAY_UPDATE_MS = 5000;  // 5 seconds
    constexpr int DISPLAY_PRIORITY = 14;     // Lower than critical components
}

DisplaySystem::DisplaySystem(std::shared_ptr<ViolationDetector> violation_detector)
    : PeriodicTask(std::chrono::milliseconds(DISPLAY_UPDATE_MS),
                   DISPLAY_PRIORITY)
    , violation_detector_(violation_detector) {
    initializeGrid();
}

void DisplaySystem::execute() {
    updateDisplay();
}

void DisplaySystem::initializeGrid() {
    grid_.resize(GRID_HEIGHT);
    for (auto& row : grid_) {
        row.resize(GRID_WIDTH);
        for (auto& cell : row) {
            cell = GridCell{};
        }
    }
}

void DisplaySystem::updateDisplay() {
    updateGrid();
    displayGrid();
}

void DisplaySystem::updateDisplay(const std::vector<std::shared_ptr<Aircraft>>& aircraft) {
    // Update aircraft list and ensure it's copied properly
    {
        aircraft_.clear();  // Clear existing list
        aircraft_.insert(aircraft_.end(), aircraft.begin(), aircraft.end());  // Copy new aircraft
    }

    // Update and show the display
    updateGrid();
    displayGrid();
}

void DisplaySystem::updateGrid() {
    initializeGrid();  // Clear previous state

    // Get current violations
    std::vector<ViolationInfo> violations;
    if (violation_detector_) {
        violations = violation_detector_->getCurrentViolations();
    }

    // Update grid with aircraft positions
    for (const auto& aircraft : aircraft_) {
        if (!aircraft) continue;

        auto state = aircraft->getState();

        // Convert coordinates to grid position
        int x = static_cast<int>((state.position.x - constants::AIRSPACE_X_MIN) *
            (GRID_WIDTH - 1) / (constants::AIRSPACE_X_MAX - constants::AIRSPACE_X_MIN));
        int y = GRID_HEIGHT - 1 - static_cast<int>((state.position.y - constants::AIRSPACE_Y_MIN) *
            (GRID_HEIGHT - 1) / (constants::AIRSPACE_Y_MAX - constants::AIRSPACE_Y_MIN));

        if (x >= 0 && x < GRID_WIDTH && y >= 0 && y < GRID_HEIGHT) {
            auto& cell = grid_[y][x];
            cell.symbol = state.callsign[0];  // First letter of callsign
            cell.aircraft_id = state.callsign;

            // Check for violations
            for (const auto& violation : violations) {
                if (violation.aircraft1_id == state.callsign ||
                    violation.aircraft2_id == state.callsign) {
                    cell.warning_level = WarningLevel::VIOLATION;
                    cell.has_conflict = true;
                    break;
                }
            }

            // Add directional indicator based on heading
            char direction = getDirectionSymbol(state.heading);
            if (direction != cell.symbol) {
                cell.symbol = direction;
            }
        }
    }
}

void DisplaySystem::displayGrid() {
    // Clear screen
    std::cout << "\033[2J\033[H";

    // Display header
    displayHeader();

    // Display grid
    std::cout << "+";
    for (int i = 0; i < GRID_WIDTH * 2; i++) std::cout << "-";
    std::cout << "+\n";

    for (const auto& row : grid_) {
        std::cout << "|";
        for (const auto& cell : row) {
            if (cell.symbol != ' ') {
                if (cell.has_conflict) {
                    std::cout << "\033[31m" << "[" << cell.symbol << "]" << "\033[0m";
                } else {
                    const char* color = getWarningColor(cell.warning_level);
                    // Add altitude indicator: H(high), M(mid), L(low)
                    char alt_indicator = 'M';
                    // Get aircraft state to determine altitude
                    for (const auto& aircraft : aircraft_) {
                        if (aircraft && aircraft->getState().callsign == cell.aircraft_id) {
                            double altitude = aircraft->getState().position.z;
                            if (altitude > 21000) alt_indicator = 'H';
                            else if (altitude < 18000) alt_indicator = 'L';
                            break;
                        }
                    }
                    std::cout << color << alt_indicator << cell.symbol << "\033[0m";
                }
            } else {
                std::cout << "  ";
            }
        }
        std::cout << "|\n";
    }

    std::cout << "+";
    for (int i = 0; i < GRID_WIDTH * 2; i++) std::cout << "-";
    std::cout << "+\n";

    displayAircraftDetails();

    if (!current_alert_.empty()) {
        std::cout << "\n\033[31m" << current_alert_ << "\033[0m\n";
    }
}

char DisplaySystem::getDirectionSymbol(double heading) const {
    // Use more intuitive directional symbols
    const char symbols[] = {'N', 'E', 'S', 'W'};
    int index = static_cast<int>((heading + 45.0) / 90.0) % 4;
    return symbols[index];
}

void DisplaySystem::displayHeader() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    std::cout << "\033[1m=== Air Traffic Control Display ===\033[0m\n"
              << "Time: " << std::put_time(std::localtime(&time), "%c") << "\n"
              << "Active Aircraft: " << aircraft_.size() << " | "
              << "Separation Violations: " <<
                 (violation_detector_ ? violation_detector_->getCurrentViolations().size() : 0) << "\n"
              << "Legend: H/M/L=Alt | N/E/S/W=Dir | [\033[31mX\033[0m]=Conflict\n"
              << std::string(50, '-') << "\n";
}

void DisplaySystem::setTrackedAircraft(const std::string& callsign) {
    tracked_aircraft_ = callsign;
}

void DisplaySystem::clearTrackedAircraft() {
    tracked_aircraft_.clear();
}

void DisplaySystem::displayAircraftDetails() {
    if (aircraft_.empty()) return;

    // First display tracked aircraft if any
    if (!tracked_aircraft_.empty()) {
        auto tracked = std::find_if(aircraft_.begin(), aircraft_.end(),
            [this](const auto& ac) {
                return ac && ac->getState().callsign == tracked_aircraft_;
            });
        
        if (tracked != aircraft_.end()) {
            const auto& state = (*tracked)->getState();
            std::cout << "\nTracked Aircraft Details:\n"
                     << std::string(50, '-') << "\n"
                     << "ID: " << state.callsign << "\n"
                     << "Flight Level: " << static_cast<int>(state.position.z/100) << "\n"
                     << "Speed: " << static_cast<int>(state.getSpeed()) << " units/s\n"
                     << "Heading: " << static_cast<int>(state.heading) << "°\n"
                     << "Position: " << formatPosition(state.position) << "\n"
                     << "Status: " << Aircraft::getStatusString(state.status) << "\n"
                     << std::string(50, '-') << "\n";
        }
    }

    std::cout << "\nAircraft Details:\n" << std::string(70, '-') << "\n"
              << std::setw(8) << "ID"
              << std::setw(10) << "Alt(FL)"
              << std::setw(8) << "Speed"
              << std::setw(8) << "Hdg"
              << std::setw(15) << "Position"
              << std::setw(12) << "Status"
              << "\n" << std::string(70, '-') << "\n";

    for (const auto& aircraft : aircraft_) {
        if (!aircraft) continue;

        const auto& state = aircraft->getState();
        const char* color = "\033[0m";

        if (violation_detector_) {
            auto violations = violation_detector_->getCurrentViolations();
            for (const auto& v : violations) {
                if (v.aircraft1_id == state.callsign || v.aircraft2_id == state.callsign) {
                    color = "\033[31m";
                    break;
                }
            }
        }

        std::cout << color
                  << std::setw(8) << state.callsign
                  << std::setw(10) << static_cast<int>(state.position.z/100)
                  << std::setw(8) << static_cast<int>(state.getSpeed())
                  << std::setw(8) << static_cast<int>(state.heading)
                  << std::setw(15) << formatPosition(state.position)
                  << std::setw(12) << Aircraft::getStatusString(state.status)
                  << "\033[0m\n";
    }
}

std::string DisplaySystem::formatPosition(const Position& pos) const {
    std::ostringstream oss;
    oss << "(" << std::setw(3) << static_cast<int>(pos.x/1000) << ","
        << std::setw(3) << static_cast<int>(pos.y/1000) << ")";
    return oss.str();
}

const char* DisplaySystem::getWarningColor(WarningLevel level) const {
    switch (level) {
        case WarningLevel::VIOLATION: return "\033[31m";  // Red
        case WarningLevel::CRITICAL: return "\033[33m";   // Yellow
        case WarningLevel::WARNING: return "\033[36m";    // Cyan
        default: return "\033[0m";                        // Reset
    }
}

void DisplaySystem::addAircraft(const std::shared_ptr<Aircraft>& aircraft) {
    if (!aircraft) return;
    aircraft_.push_back(aircraft);
}

void DisplaySystem::removeAircraft(const std::string& callsign) {
    aircraft_.erase(
        std::remove_if(aircraft_.begin(), aircraft_.end(),
            [&callsign](const auto& ac) {
                return ac && ac->getState().callsign == callsign;
            }),
        aircraft_.end());
}

void DisplaySystem::displayAlert(const std::string& message) {
    current_alert_ = message;
}

} // namespace atc
