#ifndef ATC_VIOLATION_DETECTOR_H
#define ATC_VIOLATION_DETECTOR_H

#include "common/periodic_task.h"
#include "core/aircraft.h"
#include "common/types.h"
#include <vector>
#include <memory>
#include <mutex>
#include <ctime>

namespace atc {

class ViolationDetector : public PeriodicTask {
public:
    struct ViolationPrediction {
        std::string aircraft1_id;
        std::string aircraft2_id;
        double time_to_violation;  // seconds until violation occurs
        double min_separation;     // minimum separation that will occur
        Position conflict_point;   // location where minimum separation occurs
        std::vector<std::string> resolution_options;
    };

    // Warning tracking
    struct WarningRecord {
        std::string aircraft1;
        std::string aircraft2;
        std::time_t last_warning;
    };

    ViolationDetector();
    ~ViolationDetector() = default;

    void addAircraft(const std::shared_ptr<Aircraft>& aircraft);
    void removeAircraft(const std::string& callsign);
    void setLookaheadTime(int seconds);
    std::vector<ViolationInfo> getCurrentViolations() const;
    std::vector<ViolationPrediction> getPredictedViolations() const;

protected:
    void execute() override;

private:
    static constexpr double EARLY_WARNING_THRESHOLD = 3.0;    // 300% of minimum separation
    static constexpr double MEDIUM_WARNING_THRESHOLD = 2.5;   // 250% of minimum separation
    static constexpr double CRITICAL_WARNING_THRESHOLD = 2.0; // 200% of minimum separation
    static constexpr int WARNING_COOLDOWN = 15;              // Seconds between warnings

    void checkViolations();

    bool checkPairViolation(
        const AircraftState& state1,
        const AircraftState& state2,
        ViolationInfo& violation) const;

    bool canIssueWarning(const std::string& ac1, const std::string& ac2);
    void updateWarning(const std::string& ac1, const std::string& ac2);
    void cleanupWarnings();

    Position predictPosition(
        const AircraftState& state,
        double time_seconds) const;

    ViolationPrediction predictViolation(
        const AircraftState& state1,
        const AircraftState& state2) const;

    double calculateTimeToMinimumSeparation(
        const AircraftState& state1,
        const AircraftState& state2) const;

    std::vector<std::string> generateResolutionOptions(
        const AircraftState& state1,
        const AircraftState& state2) const;

    void handleImmediateViolation(const ViolationInfo& violation);
    void handleCriticalWarning(const ViolationPrediction& prediction);
    void handleMediumWarning(const ViolationPrediction& prediction);
    void handleEarlyWarning(const ViolationPrediction& prediction);
    void logViolation(const ViolationInfo& violation) const;

    mutable std::mutex mutex_;
    std::vector<std::shared_ptr<Aircraft>> aircraft_;
    std::vector<WarningRecord> warnings_;
    int lookahead_time_seconds_;
};

}

#endif // ATC_VIOLATION_DETECTOR_H
