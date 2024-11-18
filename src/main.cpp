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
        std::cout << "Initializing ATC system..." << std::endl;

        // Create and initialize the communication channel
        auto channel = std::make_shared<atc::comm::QnxChannel>("RADAR_CHANNEL");
        if (!channel->initialize()) {
            std::cerr << "Failed to initialize communication channel" << std::endl;
            return 1;
        }
        std::cout << "Communication channel initialized" << std::endl;

        // Initialize aircraft on converging paths
        // First aircraft moving west
        atc::Position pos1{60000, 50000, 20000};
        atc::Velocity vel1;
        vel1.vx = -400;  // Moving west
        vel1.vy = 0;
        vel1.vz = 0;

        // Second aircraft moving east
        atc::Position pos2{40000, 50000, 20000};
        atc::Velocity vel2;
        vel2.vx = 400;   // Moving east
        vel2.vy = 0;
        vel2.vz = 0;

        // Create violation detector
        auto violation_detector = std::make_shared<atc::ViolationDetector>(30);

        std::cout << "\nCreating test aircraft..." << std::endl;
        std::cout << "FLIGHT1: Starting at (" << pos1.x << ", " << pos1.y << ", " << pos1.z
                  << "), moving west at 400 units/s" << std::endl;
        std::cout << "FLIGHT2: Starting at (" << pos2.x << ", " << pos2.y << ", " << pos2.z
                  << "), moving east at 400 units/s" << std::endl;

        auto aircraft1 = std::make_shared<atc::Aircraft>("FLIGHT1", pos1, vel1);
        auto aircraft2 = std::make_shared<atc::Aircraft>("FLIGHT2", pos2, vel2);

        // Add aircraft to violation detector
        violation_detector->addAircraft(aircraft1);
        violation_detector->addAircraft(aircraft2);

        // Start components
        std::cout << "\nStarting simulation..." << std::endl;
        aircraft1->start();
        aircraft2->start();
        violation_detector->start();

        // Print simulation parameters
        double initial_separation = 20000.0;  // Initial separation between aircraft
        printSimulationHeader(initial_separation);

        // Run for 30 seconds
        for (int i = 0; i < 30; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            auto state1 = aircraft1->getState();
            auto state2 = aircraft2->getState();

            // Calculate current separation
            double dx = state1.position.x - state2.position.x;
            double dy = state1.position.y - state2.position.y;
            double separation = std::sqrt(dx*dx + dy*dy);

            // Print time status
            printTimeStatus(i, separation);
        }

        // Stop simulation
        std::cout << "\nStopping simulation..." << std::endl;
        aircraft1->stop();
        aircraft2->stop();
        violation_detector->stop();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
