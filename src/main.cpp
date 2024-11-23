#include "core/aircraft.h"
#include "core/violation_detector.h"
#include "display/display_system.h"
#include "common/types.h"
#include "common/constants.h"
#include "communication/qnx_channel.h"
#include "common/logger.h"
#include "common/history_logger.h"
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
        , display_system_(std::make_shared<DisplaySystem>(violation_detector_))
        , history_logger_(std::make_shared<HistoryLogger>()) {

        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);

        channel_ = std::make_shared<comm::QnxChannel>("ATC_CHANNEL");
        if (!channel_->initialize()) {
            throw std::runtime_error("Failed to initialize communication channel");
        }

        if (!history_logger_->isOperational()) {
            throw std::runtime_error("Failed to initialize history logger");
        }

        Logger::getInstance().log("ATC System initialized successfully");
    }

    ~ATCSystem() {
        cleanup();
    }

    bool isRunning() const {
        return g_running;
    }

    void cleanup() {
        if (history_logger_) {
            Logger::getInstance().log("Stopping history logger...");
            history_logger_->stop();
        }

        if (display_system_) {
            Logger::getInstance().log("Stopping display system...");
            display_system_->stop();
        }

        if (violation_detector_) {
            Logger::getInstance().log("Stopping violation detector...");
            violation_detector_->stop();
        }

        for (const auto& aircraft : aircraft_) {
            if (aircraft) {
                aircraft->stop();
            }
        }
        aircraft_.clear();
        channel_.reset();

        Logger::getInstance().log("Cleanup complete.");
    }

    bool loadAircraftData(const std::string& filename) {
        Logger::getInstance().log("Attempting to load aircraft data from: " + filename);

        std::ifstream file(filename);
        if (!file) {
            Logger::getInstance().log("ERROR: Cannot open file: " + filename);
            return false;
        }

        std::string line;
        if (!std::getline(file, line)) {
            Logger::getInstance().log("ERROR: Empty file or cannot read header");
            return false;
        }

        if (line != "Time,ID,X,Y,Z,SpeedX,SpeedY,SpeedZ") {
            Logger::getInstance().log("ERROR: Invalid header format");
            return false;
        }

        int success_count = 0;
        while (std::getline(file, line)) {
            if (line.empty()) continue;

            std::istringstream iss(line);
            std::string token;
            std::vector<std::string> tokens;

            while (std::getline(iss, token, ',')) {
                tokens.push_back(token);
            }

            if (tokens.size() != 8) {
                Logger::getInstance().log("ERROR: Invalid number of fields in line");
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

                if (x < constants::AIRSPACE_X_MIN || x > constants::AIRSPACE_X_MAX ||
                    y < constants::AIRSPACE_Y_MIN || y > constants::AIRSPACE_Y_MAX ||
                    z < constants::AIRSPACE_Z_MIN || z > constants::AIRSPACE_Z_MAX) {
                    Logger::getInstance().log("ERROR: Position out of bounds for aircraft " + id);
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
                Logger::getInstance().log("ERROR: Failed to parse aircraft data: " + std::string(e.what()));
                continue;
            }
        }

        Logger::getInstance().log("Successfully loaded " + std::to_string(success_count) + " aircraft");
        return success_count > 0;
    }

    void run() {
        Logger::getInstance().log("Starting ATC System components...");

        for (const auto& aircraft : aircraft_) {
            aircraft->start();
        }

        violation_detector_->start();
        display_system_->start();
        history_logger_->start();

        std::chrono::steady_clock::time_point last_history_update =
            std::chrono::steady_clock::now();

        while (isRunning()) {
            auto now = std::chrono::steady_clock::now();
            history_logger_->updateAircraftStates(aircraft_);
            processSystemTasks();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        cleanup();
    }

private:
    void processSystemTasks() {
        comm::Message msg;
        while (channel_->receiveMessage(msg, 0)) {
            handleMessage(msg);
        }
    }

    void handleMessage(const comm::Message& msg) {
        try {
            switch (msg.type) {
                case comm::MessageType::COMMAND:
                    handleCommand(std::get<comm::CommandData>(msg.payload));
                    break;
                case comm::MessageType::ALERT:
                    handleAlert(std::get<comm::AlertData>(msg.payload));
                    break;
                default:
                    Logger::getInstance().log("Unknown message type received");
            }
        } catch (const std::exception& e) {
            Logger::getInstance().log("Error handling message: " + std::string(e.what()));
        }
    }

    void handleCommand(const comm::CommandData& cmd) {
        Logger::getInstance().log("Received command for " + cmd.target_id + ": " + cmd.command);
        // TODO: Implement command handling
    }

    void handleAlert(const comm::AlertData& alert) {
        Logger::getInstance().log("Received alert: " + alert.description);
        // TODO: Implement alert handling
    }

    std::vector<std::shared_ptr<Aircraft>> aircraft_;
    std::shared_ptr<ViolationDetector> violation_detector_;
    std::shared_ptr<DisplaySystem> display_system_;
    std::shared_ptr<HistoryLogger> history_logger_;
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
        atc::Logger::getInstance().log(std::string("Fatal error: ") + e.what());
        return 1;
    }
}
