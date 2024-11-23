#include "core/aircraft.h"
#include "core/violation_detector.h"
#include "common/types.h"
#include "common/constants.h"
#include "communication/qnx_channel.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <memory>
#include <vector>
#include <cmath>

void printSimulationHeader(double initial_separation) {
    std::cout << "\n=========================================" << std::endl;
    std::cout << "SIMULATION PARAMETERS" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << "Initial separation:     " << initial_separation << " units" << std::endl;
    std::cout << "Combined closing speed: 800 units/s" << std::endl;
    std::cout << "Minimum separation:     " << atc::constants::MIN_HORIZONTAL_SEPARATION
              << " units" << std::endl;
    std::cout << "Expected violation in:  "
              << (initial_separation - atc::constants::MIN_HORIZONTAL_SEPARATION) / 800.0
              << " seconds" << std::endl;
    std::cout << "=========================================\n" << std::endl;
}

void printTimeStatus(int time, double separation) {
    std::cout << "\n----------------------------------------" << std::endl;
    std::cout << "TIME STATUS UPDATE" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Time:        " << std::setw(2) << time << " seconds" << std::endl;
    std::cout << "Separation:  " << std::fixed << std::setprecision(1)
              << std::setw(8) << separation << " units" << std::endl;

    if (separation > atc::constants::MIN_HORIZONTAL_SEPARATION) {
        double time_to_min = (separation - atc::constants::MIN_HORIZONTAL_SEPARATION) / 800.0;
        std::cout << "Status:      Safe - Minimum separation in "
                  << std::setprecision(1) << time_to_min << " seconds" << std::endl;
    } else {
        std::cout << "Status:      *** VIOLATION - IMMEDIATE ACTION REQUIRED ***" << std::endl;
    }
    std::cout << "----------------------------------------" << std::endl;
}

int main() {
    try {
        std::cout << "Initializing ATC system...\n";

        // Create and initialize the ControlComputer
        atc::ControlComputer control_computer;
        control_computer.initializeSystem();

        // Print simulation parameters
        double initial_separation = 20000.0;  // Initial separation between aircraft
        printSimulationHeader(initial_separation);

        // Start the ControlComputer
        control_computer.start();

        // Simulate for 30 seconds
        for (int i = 0; i < 30; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // Fetch all aircraft states from the ControlComputer
            auto aircraft_states = control_computer.getAircraftStates();

            // Calculate separation for the first two aircraft (if present)
            if (aircraft_states.size() >= 2) {
                double dx = aircraft_states[0].position.x - aircraft_states[1].position.x;
                double dy = aircraft_states[0].position.y - aircraft_states[1].position.y;
                double separation = std::sqrt(dx * dx + dy * dy);

                // Print time status
                printTimeStatus(i, separation);
            }
        }

        // Stop the ControlComputer
        control_computer.stop();

        std::cout << "ATC system simulation complete.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
