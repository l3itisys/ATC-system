#ifndef ATC_MAIN_SYSTEM_H
#define ATC_MAIN_SYSTEM_H

#include "core/aircraft.h"
#include "core/radar_system.h"
#include "core/violation_detector.h"
#include "display/display_system.h"
#include "operator/console.h"
#include "common/logger.h"
#include "common/history_logger.h"
#include "communication/qnx_channel.h"
#include <memory>
#include <vector>
#include <atomic>
#include <chrono>

namespace atc {

struct SystemMetrics {
    long uptime;                      // System uptime in seconds
    size_t active_aircraft;           // Number of active aircraft
    size_t violation_checks;          // Number of violation checks performed
    size_t violations_detected;       // Number of violations detected
};

class MainSystem {
public:
    MainSystem();
    ~MainSystem();

    // System control
    bool initialize();
    void run();
    void shutdown();

    // Aircraft management
    bool loadAircraftData(const std::string& filename);

    // Status and metrics
    bool isRunning() const { return running_; }
    SystemMetrics getMetrics() const;

private:
    // Component initialization
    bool initializeCommunication();
    bool initializeComponents();
    bool validateComponents() const;

    // Event handling
    void handleSystemEvents();
    void handleSignals();
    void processSystemMessages();

    // Performance monitoring
    void updateSystemMetrics();
    void logSystemStatus() const;

    // System components
    std::shared_ptr<comm::QnxChannel> channel_;
    std::shared_ptr<ViolationDetector> violation_detector_;
    std::shared_ptr<RadarSystem> radar_system_;
    std::shared_ptr<DisplaySystem> display_system_;
    std::shared_ptr<OperatorConsole> operator_console_;
    std::shared_ptr<HistoryLogger> history_logger_;

    // Aircraft management
    std::vector<std::shared_ptr<Aircraft>> aircraft_;

    // System state
    std::atomic<bool> running_;
    SystemMetrics metrics_;
    std::chrono::steady_clock::time_point start_time_;

    // Constants
    static constexpr int METRICS_UPDATE_INTERVAL = 60;  // seconds
    static constexpr int SHUTDOWN_TIMEOUT = 5000;       // milliseconds
};

} // namespace atc

#endif // ATC_MAIN_SYSTEM_H

