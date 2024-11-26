#include "core/main_system.h"
#include "common/logger.h"
#include "common/constants.h"
#include <fstream>
#include <sstream>
#include <csignal>
#include <thread>

namespace {
    std::atomic<bool> g_shutdown_requested(false);

    void signal_handler(int signal) {
        g_shutdown_requested = true;
    }
}

namespace atc {

MainSystem::MainSystem()
    : running_(false)
    , start_time_(std::chrono::steady_clock::now()) {

    // Setup signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
}

MainSystem::~MainSystem() {
    shutdown();
}

bool MainSystem::initialize() {
    Logger::getInstance().log("Initializing ATC System...");

    try {
        // Initialize communication channel
        if (!initializeCommunication()) {
            Logger::getInstance().log("Failed to initialize communication");
            return false;
        }

        // Initialize system components
        if (!initializeComponents()) {
            Logger::getInstance().log("Failed to initialize components");
            return false;
        }

        // Validate system configuration
        if (!validateComponents()) {
            Logger::getInstance().log("System validation failed");
            return false;
        }

        running_ = true;
        Logger::getInstance().log("System initialization complete");
        return true;

    } catch (const std::exception& e) {
        Logger::getInstance().log("Initialization error: " + std::string(e.what()));
        return false;
    }
}

bool MainSystem::initializeCommunication() {
    try {
        // Don't use make_shared for QNX compatibility
        channel_ = std::shared_ptr<comm::QnxChannel>(
            new comm::QnxChannel("ATC_SYSTEM"));

        if (!channel_->initialize(true)) {  // Initialize as server
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        Logger::getInstance().log("Communication initialization error: " +
                                std::string(e.what()));
        return false;
    }
}

bool MainSystem::initializeComponents() {
    try {
        // Create components with appropriate dependencies
        violation_detector_ = std::shared_ptr<ViolationDetector>(
            new ViolationDetector(channel_));

        radar_system_ = std::shared_ptr<RadarSystem>(
            new RadarSystem(channel_));

        display_system_ = std::shared_ptr<DisplaySystem>(
            new DisplaySystem(violation_detector_));

        operator_console_ = std::shared_ptr<OperatorConsole>(
            new OperatorConsole(channel_));

        history_logger_ = std::shared_ptr<HistoryLogger>(
            new HistoryLogger("atc_history.log"));

        return true;
    } catch (const std::exception& e) {
        Logger::getInstance().log("Component initialization error: " +
                                std::string(e.what()));
        return false;
    }
}

bool MainSystem::validateComponents() const {
    return channel_ && violation_detector_ && radar_system_ &&
           display_system_ && operator_console_ && history_logger_;
}

void MainSystem::run() {
    if (!running_) {
        Logger::getInstance().log("System not properly initialized");
        return;
    }

    Logger::getInstance().log("Starting ATC System...");

    // Start all components
    radar_system_->start();
    violation_detector_->start();
    display_system_->start();
    operator_console_->start();
    history_logger_->start();

    auto last_metrics_update = std::chrono::steady_clock::now();

    // Main system loop
    while (running_ && !g_shutdown_requested) {
        try {
            handleSystemEvents();
            processSystemMessages();

            // Update metrics periodically
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(
                now - last_metrics_update).count() >= METRICS_UPDATE_INTERVAL) {
                updateSystemMetrics();
                logSystemStatus();
                last_metrics_update = now;
            }

            // Small sleep to prevent CPU overload
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

        } catch (const std::exception& e) {
            Logger::getInstance().log("Error in main loop: " + std::string(e.what()));
        }
    }

    shutdown();
}

bool MainSystem::loadAircraftData(const std::string& filename) {
    Logger::getInstance().log("Loading aircraft data from: " + filename);

    try {
        std::ifstream file(filename);
        if (!file) {
            Logger::getInstance().log("Failed to open aircraft data file");
            return false;
        }

        std::string line;
        std::getline(file, line);  // Skip header

        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string token;
            std::vector<std::string> tokens;

            while (std::getline(iss, token, ',')) {
                tokens.push_back(token);
            }

            if (tokens.size() != 8) {
                Logger::getInstance().log("Invalid data format in line: " + line);
                continue;
            }

            try {
                // Parse aircraft data
                double time = std::stod(tokens[0]);
                std::string id = tokens[1];
                Position pos{std::stod(tokens[2]),
                           std::stod(tokens[3]),
                           std::stod(tokens[4])};
                Velocity vel{std::stod(tokens[5]),
                           std::stod(tokens[6]),
                           std::stod(tokens[7])};

                // Create aircraft using raw pointer
                auto aircraft = std::shared_ptr<Aircraft>(
                    new Aircraft(id, pos, vel));

                aircraft_.push_back(aircraft);

                // Add to system components
                violation_detector_->addAircraft(aircraft);
                radar_system_->addAircraft(aircraft);
                display_system_->addAircraft(aircraft);

                Logger::getInstance().log("Added aircraft: " + id);

            } catch (const std::exception& e) {
                Logger::getInstance().log("Error parsing aircraft data: " +
                                        std::string(e.what()));
            }
        }

        Logger::getInstance().log("Successfully loaded " +
                                std::to_string(aircraft_.size()) +
                                " aircraft");
        return !aircraft_.empty();

    } catch (const std::exception& e) {
        Logger::getInstance().log("Error loading aircraft data: " +
                                std::string(e.what()));
        return false;
    }
}

void MainSystem::updateSystemMetrics() {
    auto now = std::chrono::steady_clock::now();
    metrics_.uptime = std::chrono::duration_cast<std::chrono::seconds>(
        now - start_time_).count();
    metrics_.active_aircraft = aircraft_.size();

    if (violation_detector_) {
        auto detector_metrics = violation_detector_->getMetrics();
        metrics_.violation_checks = detector_metrics.violation_checks_count;
        metrics_.violations_detected = detector_metrics.violations_detected;
    }
}

void MainSystem::handleSystemEvents() {
    if (g_shutdown_requested) {
        Logger::getInstance().log("Shutdown signal received");
        running_ = false;
    }
}

void MainSystem::processSystemMessages() {
    if (!channel_) return;

    comm::Message msg;
    while (channel_->receiveMessage(msg, 0)) {
        switch (msg.type) {
            case comm::MessageType::ALERT:
                Logger::getInstance().log("System Alert: " +
                                        msg.payload.alert_data.description);
                break;
            case comm::MessageType::STATUS_REQUEST:
                // Handle status requests
                break;
            default:
                break;
        }
    }
}

void MainSystem::shutdown() {
    if (!running_) return;

    Logger::getInstance().log("Initiating system shutdown...");

    // Stop all components in reverse order
    if (history_logger_) history_logger_->stop();
    if (operator_console_) operator_console_->stop();
    if (display_system_) display_system_->stop();
    if (violation_detector_) violation_detector_->stop();
    if (radar_system_) radar_system_->stop();

    // Stop all aircraft
    for (auto& aircraft : aircraft_) {
        if (aircraft) {
            aircraft->stop();
        }
    }

    aircraft_.clear();
    channel_.reset();
    running_ = false;

    Logger::getInstance().log("System shutdown complete");
}

void MainSystem::logSystemStatus() const {
    std::ostringstream oss;
    oss << "\n=== System Status Report ===\n"
        << "Uptime: " << metrics_.uptime << " seconds\n"
        << "Active Aircraft: " << metrics_.active_aircraft << "\n"
        << "Violation Checks: " << metrics_.violation_checks << "\n"
        << "Violations Detected: " << metrics_.violations_detected << "\n"
        << "Component Status:\n"
        << "  Radar: " << (radar_system_ ? "Active" : "Inactive") << "\n"
        << "  Violation Detector: " << (violation_detector_ ? "Active" : "Inactive") << "\n"
        << "  Display: " << (display_system_ ? "Active" : "Inactive") << "\n"
        << "  Console: " << (operator_console_ ? "Active" : "Inactive") << "\n"
        << "  History Logger: " << (history_logger_ ? "Active" : "Inactive") << "\n";

    Logger::getInstance().log(oss.str());
}

SystemMetrics MainSystem::getMetrics() const {
    return metrics_;
}

} // namespace atc
