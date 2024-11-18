#include "display/display_system.h"
#include "core/aircraft.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <algorithm>

namespace atc {

DisplaySystem::DisplaySystem(std::shared_ptr<ViolationDetector> violation_detector)
    : PeriodicTask(std::chrono::milliseconds(constants::DISPLAY_UPDATE_INTERVAL),
                   constants::DISPLAY_PRIORITY)
    , violation_detector_(violation_detector) {
    std::cout << "Display system initialized with update interval: "
              << constants::DISPLAY_UPDATE_INTERVAL << "ms" << std::endl;
}

void DisplaySystem::execute() {
    std::lock_guard<std::mutex> lock(display_mutex_);
    update_count_++;

    clearScreen();
    displayHeader();
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

    std::cout << "=== Air Traffic Control System ===" << std::endl;
    std::cout << "Time: " << std::ctime(&time);
    std::cout << std::string(50, '-') << std::endl;

    std::cout << std::setw(10) << "Callsign"
              << std::setw(30) << "Position (X, Y, Z)"
              << std::setw(15) << "Speed"
              << std::setw(15) << "Heading"
              << std::setw(15) << "Status" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
}

void DisplaySystem::displayAircraft() const {
    for (const auto& aircraft : aircraft_) {
        auto state = aircraft->getState();

        std::cout << std::setw(10) << state.callsign
                  << std::fixed << std::setprecision(1)
                  << std::setw(30) << "(" << state.position.x << ", "
                                            << state.position.y << ", "
                                            << state.position.z << ")"
                  << std::setw(15) << state.getSpeed()
                  << std::setw(15) << state.heading
                  << std::setw(15) << Aircraft::getStatusString(state.status)  // Use method here
                  << std::endl;
    }
}

void DisplaySystem::displayViolations() const {
    auto violations = violation_detector_->getCurrentViolations();
    if (!violations.empty()) {
        std::cout << "\n!!! VIOLATIONS DETECTED !!!" << std::endl;
        std::cout << std::string(50, '-') << std::endl;

        for (const auto& violation : violations) {
            std::cout << "Violation between " << violation.aircraft1_id
                      << " and " << violation.aircraft2_id << std::endl
                      << "  Separation: H=" << violation.horizontal_separation
                      << " units, V=" << violation.vertical_separation << " units" << std::endl;

            if (violation.is_predicted) {
                double time_until = (violation.prediction_time - violation.timestamp) / 1000.0;
                std::cout << "  PREDICTED VIOLATION in "
                          << time_until << " seconds" << std::endl;
            } else {
                std::cout << "  *** IMMEDIATE VIOLATION ***" << std::endl;
            }
        }
    }
}

void DisplaySystem::displayFooter() const {
    std::cout << "\n" << std::string(50, '-') << std::endl;
    std::cout << "Aircraft Count: " << aircraft_.size()
              << " | Update Count: " << update_count_
              << " | Update Rate: " << constants::DISPLAY_UPDATE_INTERVAL << "ms" << std::endl;
}

void DisplaySystem::addAircraft(const std::shared_ptr<Aircraft>& aircraft) {
    std::lock_guard<std::mutex> lock(display_mutex_);
    aircraft_.push_back(aircraft);
}

void DisplaySystem::removeAircraft(const std::string& callsign) {
    std::lock_guard<std::mutex> lock(display_mutex_);
    auto it = std::remove_if(aircraft_.begin(), aircraft_.end(),
        [&](const auto& aircraft) {
            return aircraft->getState().callsign == callsign;
        });
    aircraft_.erase(it, aircraft_.end());
}

} // namespace atc

