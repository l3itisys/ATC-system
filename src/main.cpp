#include "core/aircraft.h"
#include "core/violation_detector.h"
#include "core/radar_system.h"
#include "display/display_system.h"
#include "operator/console.h"
#include "common/types.h"
#include "common/constants.h"
#include "common/logger.h"
#include "common/history_logger.h"
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
#include <chrono>
#include <ctime>

namespace {
    std::atomic<bool> g_running{true};
    void signal_handler(int) {
        g_running = false;
    }
}

namespace atc {

// System metrics structure for tracking performance and statistics
struct SystemMetrics {
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point last_update_time;
    uint64_t processed_updates;
    uint64_t violation_checks;
    uint64_t radar_updates;
    uint64_t display_updates;
    uint64_t operator_commands;

    SystemMetrics()
        : start_time(std::chrono::steady_clock::now())
        , last_update_time(std::chrono::steady_clock::now())
        , processed_updates(0)
        , violation_checks(0)
        , radar_updates(0)
        , display_updates(0)
        , operator_commands(0) {}
};

class ATCSystem {
public:
    ATCSystem()
        : channel_(std::make_shared<comm::QnxChannel>("ATC_CHANNEL"))
        , violation_detector_(std::make_shared<ViolationDetector>(channel_))
        , display_system_(std::make_shared<DisplaySystem>(violation_detector_))
        , history_logger_(std::make_shared<HistoryLogger>("atc_history.log"))
        , operator_console_(nullptr)
        , metrics_() {

        // Initialize signal handlers
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);

        if (!channel_->initialize()) {
            Logger::getInstance().log("Failed to initialize communication channel");
            throw std::runtime_error("Failed to initialize communication channel");
        }

        // Initialize operator console after channel is ready
        operator_console_ = std::make_shared<OperatorConsole>(channel_);
        if (!operator_console_ || !operator_console_->isOperational()) {
            Logger::getInstance().log("Failed to initialize operator console");
            throw std::runtime_error("Failed to initialize operator console");
        }

        // Initialize radar system
        radar_system_ = std::make_shared<RadarSystem>(channel_);
        if (!radar_system_) {
            Logger::getInstance().log("Failed to initialize radar system");
            throw std::runtime_error("Failed to initialize radar system");
        }

        // Check history logger
        if (!history_logger_->isOperational()) {
            Logger::getInstance().log("Failed to initialize history logger");
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
        Logger::getInstance().log("Starting system cleanup...");

        if (radar_system_) {
            Logger::getInstance().log("Stopping radar system...");
            radar_system_->stop();
        }

        if (history_logger_) {
            Logger::getInstance().log("Stopping history logger...");
            history_logger_->stop();
        }

        if (display_system_) {
            Logger::getInstance().log("Stopping display system...");
            display_system_->stop();
        }

        if (operator_console_) {
            Logger::getInstance().log("Stopping operator console...");
            operator_console_->stop();
        }

        if (violation_detector_) {
            Logger::getInstance().log("Stopping violation detector...");
            violation_detector_->stop();
        }

        for (const auto& aircraft : aircraft_) {
            if (aircraft) {
                Logger::getInstance().log("Stopping aircraft: " + aircraft->getState().callsign);
                aircraft->stop();
            }
        }

        aircraft_.clear();

        // Log final metrics
        logSystemMetrics();
        logFinalStatistics();

        channel_.reset();
        Logger::getInstance().log("System cleanup completed successfully.");
    }

    bool loadAircraftData(const std::string& filename) {
        Logger::getInstance().log("Loading aircraft data from: " + filename);
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

        // Verify header format
        if (line != "Time,ID,X,Y,Z,SpeedX,SpeedY,SpeedZ") {
            Logger::getInstance().log("ERROR: Invalid header format");
            return false;
        }

        int success_count = 0;
        int error_count = 0;
        std::vector<std::string> failed_entries;

        while (std::getline(file, line)) {
            if (line.empty()) continue;

            std::istringstream iss(line);
            std::string token;
            std::vector<std::string> tokens;

            while (std::getline(iss, token, ',')) {
                tokens.push_back(token);
            }

            if (tokens.size() != 8) {
                Logger::getInstance().log("ERROR: Invalid number of fields in line: " + line);
                error_count++;
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
                    Logger::getInstance().log("ERROR: Position out of bounds for aircraft " + id);
                    failed_entries.push_back(id + " (Invalid Position)");
                    error_count++;
                    continue;
                }

                // Validate speed
                double speed = std::sqrt(speedX*speedX + speedY*speedY + speedZ*speedZ);
                if (speed < constants::MIN_SPEED || speed > constants::MAX_SPEED) {
                    Logger::getInstance().log("ERROR: Invalid speed for aircraft " + id);
                    failed_entries.push_back(id + " (Invalid Speed)");
                    error_count++;
                    continue;
                }

                Position pos{x, y, z};
                Velocity vel{speedX, speedY, speedZ};
                auto aircraft = std::make_shared<Aircraft>(id, pos, vel);
                aircraft_.push_back(aircraft);
                violation_detector_->addAircraft(aircraft);
                radar_system_->addAircraft(aircraft);
                success_count++;
                Logger::getInstance().log("Successfully loaded aircraft: " + id);
            }
            catch (const std::exception& e) {
                Logger::getInstance().log("ERROR: Failed to parse aircraft data: " + std::string(e.what()));
                failed_entries.push_back(tokens[1] + " (" + e.what() + ")");
                error_count++;
                continue;
            }
        }

        // Log summary
        std::ostringstream summary;
        summary << "\nAircraft Data Loading Summary:\n"
                << "Successfully loaded: " << success_count << "\n"
                << "Failed entries: " << error_count << "\n";

        if (!failed_entries.empty()) {
            summary << "\nFailed entries detail:\n";
            for (const auto& entry : failed_entries) {
                summary << "- " << entry << "\n";
            }
        }

        Logger::getInstance().log(summary.str());
        return success_count > 0;
    }

    void run() {
        Logger::getInstance().log("Starting ATC System components...");

        radar_system_->start();
        Logger::getInstance().log("Radar system started");

        for (const auto& aircraft : aircraft_) {
            aircraft->start();
            Logger::getInstance().log("Started aircraft: " + aircraft->getState().callsign);
        }

        violation_detector_->start();
        display_system_->start();
        history_logger_->start();
        operator_console_->start();

        Logger::getInstance().log("All system components started");

        auto last_metrics_update = std::chrono::steady_clock::now();

        while (isRunning()) {
            auto cycle_start = std::chrono::steady_clock::now();

            // Process operator commands
            processOperatorCommands();

            // Get current aircraft states
            std::vector<std::shared_ptr<Aircraft>> current_aircraft = aircraft_;

            // Update display
            display_system_->updateDisplay(current_aircraft);
            metrics_.display_updates++;

            // Update history logger
            history_logger_->updateAircraftStates(current_aircraft);

            // Process violations and alerts
            processViolationsAndAlerts();

            // Process system tasks
            processSystemTasks();
            metrics_.processed_updates++;

            // Log metrics every 60 seconds
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(
                now - last_metrics_update).count() >= 60) {
                logSystemMetrics();
                last_metrics_update = now;
            }

            // Maintain update cycle timing
            auto cycle_end = std::chrono::steady_clock::now();
            auto cycle_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                cycle_end - cycle_start);
            if (cycle_duration.count() < 100) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100) - cycle_duration);
            }
        }

        cleanup();
    }

private:
    void processOperatorCommands() {
        comm::Message msg;
        while (channel_->receiveMessage(msg, 0)) {
            if (msg.type == comm::MessageType::COMMAND) {
                const auto& cmd_data = std::get<comm::CommandData>(msg.payload);
                executeOperatorCommand(cmd_data);
                metrics_.operator_commands++;
            }
        }
    }

    void processViolationsAndAlerts() {
        // Get violations from detector
        auto violations = violation_detector_->getCurrentViolations();
        for (const auto& violation : violations) {
            std::ostringstream alert;
            alert << "Separation violation between "
                  << violation.aircraft1_id << " and "
                  << violation.aircraft2_id
                  << " (H:" << std::fixed << std::setprecision(1)
                  << violation.horizontal_separation
                  << ", V:" << violation.vertical_separation << ")";
            display_system_->displayAlert(alert.str());
        }

        // Check predicted violations
        auto predictions = violation_detector_->getPredictedViolations();
        for (const auto& pred : predictions) {
            std::ostringstream alert;
            alert << "Predicted violation in "
                  << std::fixed << std::setprecision(1)
                  << pred.time_to_violation << "s between "
                  << pred.aircraft1_id << " and "
                  << pred.aircraft2_id;
            display_system_->displayAlert(alert.str());
        }
        metrics_.violation_checks++;
    }

    void executeOperatorCommand(const comm::CommandData& cmd) {
        auto it = std::find_if(aircraft_.begin(), aircraft_.end(),
            [&cmd](const auto& aircraft) {
                return aircraft->getState().callsign == cmd.target_id;
            });

        if (it != aircraft_.end()) {
            try {
                if (cmd.command == "ALTITUDE" && !cmd.params.empty()) {
                    double altitude = std::stod(cmd.params[0]);
                    (*it)->updateAltitude(altitude);
                    Logger::getInstance().log("Altitude updated for " + cmd.target_id);
                }
                else if (cmd.command == "SPEED" && !cmd.params.empty()) {
                    double speed = std::stod(cmd.params[0]);
                    (*it)->updateSpeed(speed);
                    Logger::getInstance().log("Speed updated for " + cmd.target_id);
                }
                else if (cmd.command == "HEADING" && !cmd.params.empty()) {
                    double heading = std::stod(cmd.params[0]);
                    (*it)->updateHeading(heading);
                    Logger::getInstance().log("Heading updated for " + cmd.target_id);
                }
                else if (cmd.command == "EMERGENCY") {
                    bool declare = !cmd.params.empty() && cmd.params[0] == "1";
                    if (declare) {
                        (*it)->declareEmergency();
                    } else {
                        (*it)->cancelEmergency();
                    }
                }
                else if (cmd.command == "STATUS") {
                    auto state = (*it)->getState();
                    logAircraftStatus(state);
                }
            }
            catch (const std::exception& e) {
                Logger::getInstance().log("Error executing command: " + std::string(e.what()));
            }
        } else {
            Logger::getInstance().log("Aircraft not found: " + cmd.target_id);
        }
    }

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
                case comm::MessageType::POSITION_UPDATE:
                    handlePositionUpdate(std::get<AircraftState>(msg.payload));
                    break;
                case comm::MessageType::STATUS_REQUEST:
                    handleStatusRequest(msg.sender_id);
                    break;
                default:
                    Logger::getInstance().log("Unknown message type received from " + msg.sender_id);
            }
        }
        catch (const std::exception& e) {
            Logger::getInstance().log("Error handling message: " + std::string(e.what()));
        }
    }

   void handleCommand(const comm::CommandData& cmd) {
        Logger::getInstance().log("Received command for " + cmd.target_id + ": " + cmd.command);
        auto aircraft_it = std::find_if(aircraft_.begin(), aircraft_.end(),
            [&cmd](const auto& aircraft) {
                return aircraft->getState().callsign == cmd.target_id;
            });

        if (aircraft_it != aircraft_.end()) {
            if (cmd.command == "SPEED" && !cmd.params.empty()) {
                try {
                    double new_speed = std::stod(cmd.params[0]);
                    if ((*aircraft_it)->updateSpeed(new_speed)) {
                        Logger::getInstance().log("Speed updated for " + cmd.target_id);
                    }
                } catch (const std::exception& e) {
                    Logger::getInstance().log("Error processing speed command: " + std::string(e.what()));
                }
            }
            else if (cmd.command == "ALTITUDE" && !cmd.params.empty()) {
                try {
                    double new_altitude = std::stod(cmd.params[0]);
                    if ((*aircraft_it)->updateAltitude(new_altitude)) {
                        Logger::getInstance().log("Altitude updated for " + cmd.target_id);
                    }
                } catch (const std::exception& e) {
                    Logger::getInstance().log("Error processing altitude command: " + std::string(e.what()));
                }
            }
            else if (cmd.command == "EMERGENCY") {
                (*aircraft_it)->declareEmergency();
                Logger::getInstance().log("Emergency declared for " + cmd.target_id);
            }
        } else {
            Logger::getInstance().log("Aircraft not found: " + cmd.target_id);
        }
   }

    void handleAlert(const comm::AlertData& alert) {
        std::ostringstream oss;
        oss << "ALERT [Level " << static_cast<int>(alert.level) << "]: "
            << alert.description;
        Logger::getInstance().log(oss.str());
        display_system_->displayAlert(oss.str());
    }

    void handlePositionUpdate(const AircraftState& state) {
        auto aircraft_it = std::find_if(aircraft_.begin(), aircraft_.end(),
            [&state](const auto& aircraft) {
                return aircraft->getState().callsign == state.callsign;
            });
        if (aircraft_it != aircraft_.end()) {
            std::vector<std::shared_ptr<Aircraft>> current_aircraft = {*aircraft_it};
            display_system_->updateDisplay(current_aircraft);
        }
    }

    void handleStatusRequest(const std::string& requestor) {
        std::ostringstream status;
        status << "System Status Report:\n"
               << "Active Aircraft: " << aircraft_.size() << "\n"
               << "Updates Processed: " << metrics_.processed_updates << "\n"
               << "Violation Checks: " << metrics_.violation_checks << "\n"
               << "System Uptime: " << getSystemUptime() << "s";
        comm::AlertData response{0, status.str()};
        comm::Message msg = comm::Message::createAlert("ATC_SYSTEM", response);
        channel_->sendMessage(msg);
    }

    void logSystemMetrics() {
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
            now - metrics_.start_time).count();

        std::ostringstream oss;
        oss << "\n=== System Metrics Report ===\n"
            << "Uptime: " << uptime << " seconds\n"
            << "Active Aircraft: " << aircraft_.size() << "\n"
            << "Processed Updates: " << metrics_.processed_updates << "\n"
            << "Violation Checks: " << metrics_.violation_checks << "\n"
            << "Radar Updates: " << metrics_.radar_updates << "\n"
            << "Display Updates: " << metrics_.display_updates << "\n"
            << "Operator Commands: " << metrics_.operator_commands << "\n"
            << "Updates/Second: " << (metrics_.processed_updates / std::max(1L, uptime)) << "\n"
            << "Last Update: " << formatTimestamp(metrics_.last_update_time) << "\n"
            << "=========================\n";

        Logger::getInstance().log(oss.str());
        metrics_.last_update_time = now;
    }

    void logFinalStatistics() {
        auto end_time = std::chrono::steady_clock::now();
        auto total_runtime = std::chrono::duration_cast<std::chrono::seconds>(
            end_time - metrics_.start_time).count();

        std::ostringstream oss;
        oss << "\n=== Final System Statistics ===\n"
            << "Total Runtime: " << total_runtime << " seconds\n"
            << "Total Aircraft Tracked: " << aircraft_.size() << "\n"
            << "Total Updates Processed: " << metrics_.processed_updates << "\n"
            << "Total Violation Checks: " << metrics_.violation_checks << "\n"
            << "Total Radar Updates: " << metrics_.radar_updates << "\n"
            << "Total Display Updates: " << metrics_.display_updates << "\n"
            << "Total Operator Commands: " << metrics_.operator_commands << "\n"
            << "Average Updates/Second: " << (metrics_.processed_updates / std::max(1L, total_runtime)) << "\n"
            << "============================\n";

        Logger::getInstance().log(oss.str());
    }

    void logAircraftStatus(const AircraftState& state) {
        std::ostringstream oss;
        oss << "\nAircraft Status: " << state.callsign
            << "\nPosition: (" << std::fixed << std::setprecision(2)
            << state.position.x << ", "
            << state.position.y << ", "
            << state.position.z << ")"
            << "\nSpeed: " << state.getSpeed()
            << "\nHeading: " << state.heading
            << "\nStatus: " << Aircraft::getStatusString(state.status)
            << "\nTimestamp: " << state.timestamp;
        Logger::getInstance().log(oss.str());
    }

    std::string formatTimestamp(const std::chrono::steady_clock::time_point& time_point) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    long getSystemUptime() {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(
            now - metrics_.start_time).count();
    }

private:
    // Member variables
    std::vector<std::shared_ptr<Aircraft>> aircraft_;
    std::shared_ptr<ViolationDetector> violation_detector_;
    std::shared_ptr<DisplaySystem> display_system_;
    std::shared_ptr<HistoryLogger> history_logger_;
    std::shared_ptr<RadarSystem> radar_system_;
    std::shared_ptr<OperatorConsole> operator_console_;
    std::shared_ptr<comm::QnxChannel> channel_;
    SystemMetrics metrics_;
};

} // namespace atc

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            std::cerr << "Usage: " << argv[0] << " <aircraft_data_file>" << std::endl;
            return 1;
        }

        atc::Logger::getInstance().log("Starting ATC System...");

        try {
            atc::ATCSystem system;

            // Load aircraft data
            if (!system.loadAircraftData(argv[1])) {
                atc::Logger::getInstance().log("Failed to load aircraft data from: " +
                                            std::string(argv[1]));
                return 1;
            }

            atc::Logger::getInstance().log("Successfully loaded aircraft data, starting system...");

            // Run the system
            system.run();

            atc::Logger::getInstance().log("System shutdown completed normally.");
            return 0;
        }
        catch (const std::runtime_error& e) {
            atc::Logger::getInstance().log("System initialization failed: " +
                                         std::string(e.what()));
            return 1;
        }
        catch (const std::exception& e) {
            atc::Logger::getInstance().log("Unexpected error during system operation: " +
                                         std::string(e.what()));
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        atc::Logger::getInstance().log("Fatal error: " + std::string(e.what()));
        return 1;
    }
    catch (...) {
        std::cerr << "Unknown fatal error occurred" << std::endl;
        atc::Logger::getInstance().log("Unknown fatal error occurred");
        return 1;
    }
}
