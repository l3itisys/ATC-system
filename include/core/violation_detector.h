#ifndef ATC_VIOLATION_DETECTOR_H
#define ATC_VIOLATION_DETECTOR_H

#include "common/periodic_task.h"
#include "common/types.h"
#include "core/aircraft.h"
#include "communication/qnx_channel.h"
#include <vector>
#include <memory>
#include <mutex>
#include <chrono>
#include <queue>

namespace atc {

class ViolationDetector : public PeriodicTask {
public:
    struct ViolationPrediction {
        std::string aircraft1_id;
        std::string aircraft2_id;
        double time_to_violation;  // seconds until violation occurs
        double min_separation;     // minimum separation that will occur
        Position conflict_point;   // location where minimum separation occurs
        bool requires_immediate_action; // true if violation is imminent
    };

    struct ResolutionAction {
        std::string aircraft_id;
        enum class Type {
            ALTITUDE_CHANGE,
            SPEED_CHANGE,
            HEADING_CHANGE
        } action_type;
        double value;
        bool is_mandatory;
    };

    explicit ViolationDetector(std::shared_ptr<comm::QnxChannel> channel);
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
    static constexpr double CRITICAL_WARNING_THRESHOLD = 1.5; // 150% of minimum separation
    static constexpr double IMMEDIATE_ACTION_THRESHOLD = 1.2; // 120% of minimum separation
    static constexpr int WARNING_COOLDOWN = 15;              // Seconds between warnings

    struct PredictedConflict {
        ViolationPrediction prediction;
        std::vector<ResolutionAction> proposed_actions;
        std::chrono::steady_clock::time_point detection_time;
        bool resolution_attempted;
    };

    void checkViolations();
    void handlePredictedConflict(const ViolationPrediction& prediction);
    std::vector<ResolutionAction> calculateResolutionActions(
        const AircraftState& state1,
        const AircraftState& state2,
        const ViolationPrediction& prediction);
    void executeResolutionActions(const std::vector<ResolutionAction>& actions);

    bool checkPairViolation(
        const AircraftState& state1,
        const AircraftState& state2,
        ViolationInfo& violation) const;

    ViolationPrediction predictViolation(
        const AircraftState& state1,
        const AircraftState& state2) const;

    Position predictPosition(
        const AircraftState& state,
        double time_seconds) const;

    double calculateTimeToMinimumSeparation(
        const AircraftState& state1,
        const AircraftState& state2) const;

    void handleImmediateViolation(const ViolationInfo& violation);
    void logViolation(const ViolationInfo& violation) const;
    void sendResolutionCommand(const ResolutionAction& action);

    std::mutex mutex_;
    std::vector<std::shared_ptr<Aircraft>> aircraft_;
    std::shared_ptr<comm::QnxChannel> channel_;
    std::queue<PredictedConflict> conflict_queue_;
    int lookahead_time_seconds_;
    std::chrono::steady_clock::time_point last_resolution_time_;
};

} // namespace atc

#endif // ATC_VIOLATION_DETECTOR_H
