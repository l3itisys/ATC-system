#include "core/aircraft.h"
#include "common/types.h"
#include "common/constants.h"
#include "communication/qnx_channel.h"
#include <iostream>
#include <thread>
#include <memory>

int main() {
    try {
        std::cout << "Initializing ATC system..." << std::endl;

        // Create and initialize the communication channel first
        auto channel = std::make_shared<atc::comm::QnxChannel>("RADAR_CHANNEL");
        if (!channel->initialize()) {
            std::cerr << "Failed to initialize communication channel" << std::endl;
            return 1;
        }

        std::cout << "Communication channel initialized" << std::endl;

        // Initialize flight characteristics
        atc::FlightCharacteristics characteristics{
            .model = "Boeing 737",
            .type = atc::AircraftType::COMMERCIAL,
            .cruise_speed = 400,
            .max_speed = 500,
            .min_speed = 200,
            .max_altitude = 35000,
            .min_altitude = 15000,
            .max_climb_rate = 2000,
            .max_descent_rate = 2500
        };

        // Initialize position and velocity
        atc::Position initial_pos{50000, 50000, 20000};
        atc::Velocity initial_vel;
        initial_vel.setFromSpeedAndHeading(400, 90);  // 400 units/s, heading east

        std::cout << "Creating aircraft..." << std::endl;

        // Create aircraft
        atc::Aircraft aircraft("FLIGHT123", initial_pos, initial_vel, characteristics);

        // Start the simulation
        std::cout << "Starting simulation..." << std::endl;
        aircraft.start();

        // Let it run for a while
        std::cout << "Running simulation for 10 seconds..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(10));

        // Change some parameters
        std::cout << "Updating aircraft parameters..." << std::endl;
        aircraft.updateSpeed(450);
        aircraft.updateHeading(180);

        // Run for a few more seconds
        std::cout << "Running with new parameters for 5 seconds..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));

        // Stop simulation
        std::cout << "Stopping simulation..." << std::endl;
        aircraft.stop();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
