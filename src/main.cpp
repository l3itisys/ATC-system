#include "core/aircraft.h"
#include "core/violation_detector.h"
#include "display/display_system.h"
#include "common/types.h"
#include "common/constants.h"
#include "communication/qnx_channel.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <memory>
#include <vector>
#include <fstream>
#include <sstream>
#include <csignal>
#include <cstdlib>
#include <atomic>
#include <algorithm>

namespace {
    std::atomic<bool> g_running{true};

    void signal_handler(int) {
        g_running = false;
    }
}

namespace atc {

class ATCSystem {
public:
    ATCSystem()
        : violation_detector_(std::make_shared<ViolationDetector>())
        , display_system_(std::make_shared<DisplaySystem>(violation_detector_)) {

        // Set up signal handling for graceful shutdown
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);

        // Initialize communication channel
        channel_ = std::make_shared<comm::QnxChannel>("ATC_CHANNEL");
        if (!channel_->initialize()) {
            throw std::runtime_error("Failed to initialize communication channel");
        }

        std::cout << "ATC System initialized successfully" << std::endl;
    }

    ~ATCSystem() {
        cleanup();
    }

    bool isRunning() const {
        return g_running;
    }

    void cleanup() {
        // Stop all components first
        if (display_system_) {
            std::cout << "Stopping display system..." << std::endl;
            display_system_->stop();
        }

        if (violation_detector_) {
            std::cout << "Stopping violation detector..." << std::endl;
            violation_detector_->stop();
        }

        for (const auto& aircraft : aircraft_) {
            if (aircraft) {
                aircraft->stop();
            }
        }
        aircraft_.clear();

        // Clean up channel last
        channel_.reset();

        std::cout << "Cleanup complete." << std::endl;
    }

    bool loadAircraftData(const std::string& filename) {
        std::cout << "\nAttempting to load aircraft data from: " << filename << std::endl;

        std::ifstream file(filename);
        if (!file) {
            std::cerr << "ERROR: Cannot open file: " << filename << std::endl;
            return false;
        }

        std::string line;
        if (!std::getline(file, line)) {
            std::cerr << "ERROR: Empty file or cannot read header" << std::endl;
            return false;
        }

        if (line != "Time,ID,X,Y,Z,SpeedX,SpeedY,SpeedZ") {
            std::cerr << "ERROR: Invalid header format" << std::endl;
            return false;
        }

        int success_count = 0;
        while (std::getline(file, line)) {
            if (line.empty()) {
                continue;
            }

            std::istringstream iss(line);
            std::string token;
            std::vector<std::string> tokens;

            while (std::getline(iss, token, ',')) {
                tokens.push_back(token);
            }

            if (tokens.size() != 8) {
                std::cerr << "ERROR: Invalid number of fields in line" << std::endl;
                continue;
            }

            try {
                double time = std::stod(tokens[0]);
                std::string id = tokens[1];
                double x = std::stod(tokens[2]);
                double y = std::stod(tokens[3]);
                double z = std::stod(tokens[4]);
                double speedX = std::stod(tokens[5]);
                double speedY = std::stod(tokens[6]);
                double speedZ = std::stod(tokens[7]);

                // Validate position
                if (x < constants::AIRSPACE_X_MIN || x > constants::AIRSPACE_X_MAX ||
                    y < constants::AIRSPACE_Y_MIN || y > constants::AIRSPACE_Y_MAX ||
                    z < constants::AIRSPACE_Z_MIN || z > constants::AIRSPACE_Z_MAX) {
                    std::cerr << "ERROR: Position out of bounds for aircraft " << id << std::endl;
                    continue;
                }

                Position pos{x, y, z};
                Velocity vel{speedX, speedY, speedZ};

                auto aircraft = std::make_shared<Aircraft>(id, pos, vel);
                aircraft_.push_back(aircraft);
                violation_detector_->addAircraft(aircraft);
                display_system_->addAircraft(aircraft);

                success_count++;

            } catch (const std::exception& e) {
                std::cerr << "ERROR: Failed to parse aircraft data: " << e.what() << std::endl;
                continue;
            }
        }

        std::cout << "Successfully loaded " << success_count << " aircraft" << std::endl;
        return success_count > 0;
    }

    void run() {
        std::cout << "\nStarting ATC System components..." << std::endl;

        for (const auto& aircraft : aircraft_) {
            aircraft->start();
        }

        violation_detector_->start();
        display_system_->start();

        std::cout << "System running. Press Ctrl+C to exit." << std::endl;

        while (isRunning()) {
            processMessages();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        cleanup();
    }

private:
    void processMessages() {
        comm::Message msg;
        if (channel_->receiveMessage(msg, 0)) {
            handleMessage(msg);
        }
    }

    void handleMessage(const comm::Message& msg) {
        switch (msg.type) {
            case comm::MessageType::COMMAND:
                if (auto* cmd_data = std::get_if<comm::CommandData>(&msg.payload)) {
                    handleCommand(*cmd_data);
                }
                break;

            case comm::MessageType::ALERT:
                if (auto* alert_data = std::get_if<comm::AlertData>(&msg.payload)) {
                    handleAlert(*alert_data);
                }
                break;

            default:
                break;
        }
    }

    void handleCommand(const comm::CommandData& cmd) {
        auto it = std::find_if(aircraft_.begin(), aircraft_.end(),
            [&](const auto& aircraft) {
                return aircraft->getState().callsign == cmd.target_id;
            });

        if (it == aircraft_.end()) {
            std::cerr << "Aircraft not found: " << cmd.target_id << std::endl;
            return;
        }
    }

    void handleAlert(const comm::AlertData& alert) {
        std::cout << "ALERT [Level " << static_cast<int>(alert.level)
                  << "]: " << alert.description << std::endl;
    }

    std::vector<std::shared_ptr<Aircraft>> aircraft_;
    std::shared_ptr<ViolationDetector> violation_detector_;
    std::shared_ptr<DisplaySystem> display_system_;
    std::shared_ptr<comm::QnxChannel> channel_;
};

} // namespace atc

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            std::cerr << "Usage: " << argv[0] << " <aircraft_data_file>" << std::endl;
            return 1;
        }

        atc::ATCSystem system;

        if (!system.loadAircraftData(argv[1])) {
            std::cerr << "Failed to load aircraft data" << std::endl;
            return 1;
        }

        system.run();
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
