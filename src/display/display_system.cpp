#include "display/display_system.h"
#include "common/logger.h"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace atc {

DisplaySystem::DisplaySystem(std::shared_ptr<ViolationDetector> violation_detector)
    : PeriodicTask(std::chrono::milliseconds(1000), 10)  // 1 second base period, low priority
    , violation_detector_(violation_detector)
    , paused_(false)
    , show_grid_(true)
    , refresh_rate_(5)
    , last_update_(std::chrono::steady_clock::now()) {
    initializeGrid();
}

void DisplaySystem::execute() {
    if (paused_) return;

    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(
        now - last_update_).count() >= refresh_rate_) {
        updateDisplay();
        last_update_ = now;
    }
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
    std::lock_guard<std::mutex> lock(mutex_);
    updateGrid();
    displayGrid();
}

void DisplaySystem::updateGrid() {
    initializeGrid();  // Clear previous state

    for (const auto& aircraft : aircraft_) {
        if (!aircraft) continue;

        auto state = aircraft->getState();
        auto pos = state.position;

        // Convert position to grid coordinates
        int x = static_cast<int>((pos.x / constants::AIRSPACE_X_MAX) * GRID_WIDTH);
        int y = static_cast<int>((pos.y / constants::AIRSPACE_Y_MAX) * GRID_HEIGHT);

        if (isValidGridPosition(x, y)) {
            auto& cell = grid_[y][x];
            cell.aircraft_id = state.callsign;
            cell.is_tracked = (state.callsign == tracked_aircraft_);
            cell.warning_level = determineWarningLevel(state);
            cell.is_emergency = (state.status == AircraftStatus::EMERGENCY);
            cell.symbol = getAircraftSymbol(state);
        }
    }
}

void DisplaySystem::displayGrid() {
    clearScreen();
    displayHeader();

    // Display grid
    for (const auto& row : grid_) {
        for (const auto& cell : row) {
            displayCell(cell);
        }
        std::cout << '\n';
    }

    displayAircraftDetails();
}

void DisplaySystem::displayCell(const GridCell& cell) {
    const char* color = getWarningColor(cell.warning_level);

    std::cout << color;
    if (cell.is_conflict_point) {
        std::cout << 'X';
    } else if (cell.is_emergency) {
        std::cout << '!';
    } else {
        std::cout << cell.symbol;
    }
    std::cout << "\033[0m";  // Reset color
}

void DisplaySystem::displayHeader() {
    std::cout << "\033[2J\033[H";  // Clear screen and move to top
    std::cout << "=== Air Traffic Control Display ===\n";
    std::cout << "Aircraft: " << aircraft_.size();

    if (violation_detector_) {
        std::cout << "\nRefresh Rate: " << refresh_rate_ << "s"
                  << " | Display Mode: " << (show_grid_ ? "Grid" : "List")
                  << " | Status: " << (paused_ ? "PAUSED" : "ACTIVE") << "\n";
    }

    std::cout << std::string(GRID_WIDTH * 2, '-') << "\n";
}

void DisplaySystem::displayAircraftDetails() {
    std::cout << "\nAircraft Details:\n";
    std::cout << std::string(50, '-') << '\n';

    for (const auto& aircraft : aircraft_) {
        if (!aircraft) continue;

        auto state = aircraft->getState();
        const char* color = getWarningColor(determineWarningLevel(state));

        std::cout << color
                  << std::setw(10) << state.callsign
                  << std::setw(15) << formatPosition(state.position)
                  << std::setw(8) << static_cast<int>(state.getSpeed())
                  << "\033[0m\n";
    }
}

const char* DisplaySystem::getWarningColor(WarningLevel level) const {
    switch (level) {
        case WarningLevel::VIOLATION: return "\033[1;31m";  // Bright red
        case WarningLevel::CRITICAL:  return "\033[31m";    // Red
        case WarningLevel::WARNING:   return "\033[33m";    // Yellow
        default:                      return "\033[0m";     // Reset
    }
}

std::string DisplaySystem::formatPosition(const Position& pos) const {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1)
       << "(" << pos.x/1000 << "," << pos.y/1000 << "," << pos.z/1000 << ")";
    return ss.str();
}

char DisplaySystem::getAircraftSymbol(const AircraftState& state) const {
    double heading = state.heading;
    if (heading < 45 || heading >= 315) return '^';      // North
    if (heading >= 45 && heading < 135) return '>';      // East
    if (heading >= 135 && heading < 225) return 'v';     // South
    return '<';                                          // West
}

bool DisplaySystem::isValidGridPosition(int x, int y) const {
    return x >= 0 && x < GRID_WIDTH && y >= 0 && y < GRID_HEIGHT;
}

void DisplaySystem::markPredictedConflictPoint(const Position& point) {
    int x = static_cast<int>((point.x / constants::AIRSPACE_X_MAX) * GRID_WIDTH);
    int y = static_cast<int>((point.y / constants::AIRSPACE_Y_MAX) * GRID_HEIGHT);

    if (isValidGridPosition(x, y)) {
        grid_[y][x].is_conflict_point = true;
    }
}

WarningLevel DisplaySystem::determineWarningLevel(const AircraftState& state) const {
    if (state.status == AircraftStatus::EMERGENCY) {
        return WarningLevel::VIOLATION;
    }
    return WarningLevel::NONE;
}

void DisplaySystem::addAircraft(const std::shared_ptr<Aircraft>& aircraft) {
    if (!aircraft) return;
    std::lock_guard<std::mutex> lock(mutex_);
    aircraft_.push_back(aircraft);
}

void DisplaySystem::removeAircraft(const std::string& callsign) {
    std::lock_guard<std::mutex> lock(mutex_);
    aircraft_.erase(
        std::remove_if(aircraft_.begin(), aircraft_.end(),
            [&callsign](const auto& ac) {
                return ac && ac->getState().callsign == callsign;
            }),
        aircraft_.end());
}

void DisplaySystem::displayAlert(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    alerts_.push(message);
    if (alerts_.size() > MAX_ALERTS) {
        alerts_.pop();
    }
}

void DisplaySystem::setTrackedAircraft(const std::string& callsign) {
    std::lock_guard<std::mutex> lock(mutex_);
    tracked_aircraft_ = callsign;
}

void DisplaySystem::clearTrackedAircraft() {
    std::lock_guard<std::mutex> lock(mutex_);
    tracked_aircraft_.clear();
}

void DisplaySystem::clearScreen() const {
    std::cout << "\033[2J\033[H";  // ANSI escape codes to clear screen and move cursor to top
}

} // namespace atc
