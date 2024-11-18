#include "core/aircraft.h"
#include "common/types.h"
#include "common/constants.h"
#include "communication/qnx_channel.h"
#include <iostream>
#include <thread>
#include <memory>
#include <vector>

// Test scenario function declarations
void runNormalFlightTest(atc::Aircraft& aircraft);
void runBoundaryTest(atc::Aircraft& aircraft);
void runEmergencyTest(atc::Aircraft& aircraft);

int main() {
    try {
        std::cout << "=== ATC System Test Scenarios ===" << std::endl;

        // Create and initialize the communication channel
        auto channel = std::make_shared<atc::comm::QnxChannel>("RADAR_CHANNEL");
        if (!channel->initialize()) {
            std::cerr << "Failed to initialize communication channel" << std::endl;
            return 1;
        }
        std::cout << "Communication channel initialized" << std::endl;

        // Test Scenario 1: Normal Flight
        std::cout << "\n=== Test Scenario 1: Normal Flight ===" << std::endl;
        {
            atc::Position pos{50000, 50000, 20000};
            atc::Velocity vel;
            vel.setFromSpeedAndHeading(400, 90);
            atc::Aircraft aircraft("TEST001", pos, vel);
            runNormalFlightTest(aircraft);
        }

        // Test Scenario 2: Boundary Test
        std::cout << "\n=== Test Scenario 2: Boundary Test ===" << std::endl;
        {
            atc::Position pos{95000, 50000, 20000};  // Near X boundary
            atc::Velocity vel;
            vel.setFromSpeedAndHeading(400, 90);
            atc::Aircraft aircraft("TEST002", pos, vel);
            runBoundaryTest(aircraft);
        }

        // Test Scenario 3: Emergency Handling
        std::cout << "\n=== Test Scenario 3: Emergency Test ===" << std::endl;
        {
            atc::Position pos{50000, 50000, 20000};
            atc::Velocity vel;
            vel.setFromSpeedAndHeading(400, 90);
            atc::Aircraft aircraft("TEST003", pos, vel);
            runEmergencyTest(aircraft);
        }

        std::cout << "\n=== All test scenarios completed ===" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}

void runNormalFlightTest(atc::Aircraft& aircraft) {
    // Start aircraft
    aircraft.start();
    std::cout << "Running normal flight test..." << std::endl;

    // Let it fly straight for 5 seconds
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Test speed change
    std::cout << "Testing speed change..." << std::endl;
    aircraft.updateSpeed(450);
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Test heading change
    std::cout << "Testing heading change..." << std::endl;
    aircraft.updateHeading(180);
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Stop test
    aircraft.stop();
    std::cout << "Normal flight test completed" << std::endl;
}

void runBoundaryTest(atc::Aircraft& aircraft) {
    // Start aircraft
    aircraft.start();
    std::cout << "Running boundary test..." << std::endl;

    // Aircraft should approach boundary and trigger exit
    std::this_thread::sleep_for(std::chrono::seconds(15));

    // Stop test
    aircraft.stop();
    std::cout << "Boundary test completed" << std::endl;
}

void runEmergencyTest(atc::Aircraft& aircraft) {
    // Start aircraft
    aircraft.start();
    std::cout << "Running emergency test..." << std::endl;

    // Let it fly normally for a bit
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Declare emergency
    std::cout << "Declaring emergency..." << std::endl;
    aircraft.declareEmergency();
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Cancel emergency
    std::cout << "Canceling emergency..." << std::endl;
    aircraft.cancelEmergency();
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Stop test
    aircraft.stop();
    std::cout << "Emergency test completed" << std::endl;
}
