#ifndef ATC_DISPLAY_SYSTEM_H
#define ATC_DISPLAY_SYSTEM_H

#include "common/periodic_task.h"
#include "core/violation_detector.h"
#include "core/aircraft.h"
#include <memory>
#include <mutex>
#include <vector>
#include <string>
#include <iomanip>
#include <cmath>

namespace atc {

class DisplaySystem : public PeriodicTask {
public:
    explicit DisplaySystem(std::shared_ptr<ViolationDetector> violation_detector);
    ~DisplaySystem() = default;

    void addAircraft(const std::shared_ptr<Aircraft>& aircraft);
    void addAircraft(const std::vector<std::shared_ptr<Aircraft>>& aircraft);
    void removeAircraft(const std::string& callsign);
    void displayAlert(const std::string& alert_message);
    void updateDisplay(const std::vector<std::shared_ptr<Aircraft>>& current_aircraft);

protected:
    void execute() override;

private:
    // ANSI color codes
    struct Colors {
        static const char* reset() { return "\033[0m"; }
        static const char* red() { return "\033[31m"; }
        static const char* green() { return "\033[32m"; }
        static const char* yellow() { return "\033[33m"; }
        static const char* blue() { return "\033[34m"; }
        static const char* magenta() { return "\033[35m"; }
        static const char* cyan() { return "\033[36m"; }
        static const char* bold() { return "\033[1m"; }
        static const char* dim() { return "\033[2m"; }
    };

    // Warning levels
    static constexpr double WARNING_EARLY = 2.0;     // 200% of minimum separation
    static constexpr double WARNING_MEDIUM = 1.5;    // 150% of minimum separation
    static constexpr double WARNING_CRITICAL = 1.2;  // 120% of minimum separation

    enum class WarningLevel {
        NONE,
        EARLY,
        MEDIUM,
        CRITICAL,
        VIOLATION
    };

    // Direction indicators
    const char DIRECTION_SYMBOLS[8] = {'^', '/', '>', '\\', 'v', '/', '<', '\\'};

    // Aircraft display information
    struct AircraftDisplayInfo {
        char marker;
        char direction;
        bool occupied;
        std::string callsign;
        double altitude;
        AircraftStatus status;
        WarningLevel warning_level;
        bool is_predicted;

        AircraftDisplayInfo()
            : marker(' ')
            , direction(' ')
            , occupied(false)
            , altitude(0.0)
            , status(AircraftStatus::CRUISING)
            , warning_level(WarningLevel::NONE)
            , is_predicted(false) {}
    };

    // Display methods
    void clearScreen() const;
    void displayHeader() const;
    void displayLegend() const;
    void displayAircraft() const;
    void displayViolations() const;
    void displayFooter() const;
    void displayAircraftDetails() const;

    // Helper methods
    char getDirectionSymbol(double heading) const;
    const char* getWarningColor(WarningLevel level) const;
    WarningLevel calculateWarningLevel(const AircraftState& state1, const AircraftState& state2) const;
    double calculateClosureRate(const AircraftState& state1, const AircraftState& state2) const;
    double calculateTimeToClosestApproach(const AircraftState& state1, const AircraftState& state2) const;
    std::pair<double, double> calculateSeparation(const AircraftState& state1, const AircraftState& state2) const;
    std::string formatPosition(const Position& pos) const;
    std::string formatSeparation(double horizontal, double vertical) const;
    std::string formatAltitude(double altitude) const;
    std::string getWarningString(WarningLevel level) const;
    std::string getRecommendedAction(const AircraftState& state1, const AircraftState& state2) const;

    // Constants
    static constexpr int DISPLAY_WIDTH = 50;
    static constexpr int DISPLAY_HEIGHT = 25;
    static constexpr int PREDICTION_TIME = 30;  // seconds
    static constexpr int MIN_DISPLAY_UPDATE = 1000;  // milliseconds
    static constexpr int MAX_DISPLAY_UPDATE = 10000; // milliseconds

    static constexpr char PREDICTED_POSITION_MARKER = '*';

    // Member variables
    mutable std::mutex display_mutex_;
    std::vector<std::shared_ptr<Aircraft>> aircraft_;
    std::shared_ptr<ViolationDetector> violation_detector_;
    int update_count_ = 0;
    std::string current_alert_message_;
};

}

#endif // ATC_DISPLAY_SYSTEM_H
