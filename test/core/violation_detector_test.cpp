#ifndef ATC_VIOLATION_DETECTOR_H
#define ATC_VIOLATION_DETECTOR_H

#include "common/periodic_task.h"
#include "common/types.h"
#include "communication/qnx_channel.h"
#include <vector>
#include <memory>
#include <mutex>
#include <chrono>

namespace atc {

class ViolationDetector : public PeriodicTask {
public:
    // Constants based on requirements
    static constexpr double MIN_HORIZONTAL_SEPARATION = 3000.0;
    static constexpr double MIN_VERTICAL_SEPARATION = 1000.0;
    static constexpr int PREDICTION_WINDOW = 180;  // 3 minutes in seconds
    static constexpr int WARNING_COOLDOWN = 15;    // seconds between warnings

    struct ViolationPrediction {
        std::string aircraft1_id;
        std::string aircraft2_id;
        double time_to_violation;
        double min_separation;
        Position conflict_point;
        bool requires_immediate_action;
        std::chrono::steady_clock::time_point detection_time;

        bool isValid() const {
            return !aircraft1_id.empty() && !aircraft2_id.empty() &&
                   time_to_violation >= 0 && min_separation >= 0;
        }
    };

    struct ResolutionAction {
        std::string aircraft_id;
        enum class Type {
            ALTITUDE_CHANGE,
            SPEED_CHANGE,
            HEADING_CHANGE,
            EMERGENCY_STOP
        } action_type;
        double value;
        bool is_mandatory;
        double confidence;
        std::string description;

        bool isValid() const {
            return !aircraft_id.empty() && confidence >= 0 && confidence <= 1;
        }
    };

    explicit ViolationDetector(std::shared_ptr<comm::QnxChannel> channel);
    ~ViolationDetector() = default;

    void addAircraft(const std::shared_ptr<Aircraft>& aircraft);
    void removeAircraft(const std::string& callsign);
    bool hasActiveViolations() const;
    int getActiveAircraftCount() const;

protected:
    void execute() override;

private:
    void checkViolations();
    void handleImmediateViolation(const ViolationInfo& violation);
    void handlePredictedConflict(const ViolationPrediction& prediction);

    // Resolution calculation
    std::vector<ResolutionAction> calculateResolutionActions(const ViolationPrediction& prediction);
    void executeResolutionAction(const ResolutionAction& action);
    bool validateResolutionAction(const ResolutionAction& action) const;

    // Prediction helpers
    ViolationPrediction predictViolation(const AircraftState& state1,
                                       const AircraftState& state2) const;
    Position predictPosition(const AircraftState& state, double time_seconds) const;
    double calculateTimeToMinimumSeparation(const AircraftState& state1,
                                          const AircraftState& state2) const;
    bool checkPairViolation(const AircraftState& state1,
                           const AircraftState& state2,
                           ViolationInfo& violation) const;
    bool validatePrediction(const ViolationPrediction& prediction) const;

    // Data members
    std::vector<std::shared_ptr<Aircraft>> aircraft_;
    std::shared_ptr<comm::QnxChannel> channel_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_warning_times_;
    mutable std::mutex mutex_;
};

} // namespace atc

#endif // ATC_VIOLATION_DETECTOR_H
