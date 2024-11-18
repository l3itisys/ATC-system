#ifndef ATC_VIOLATION_DETECTOR_H
#define ATC_VIOLATION_DETECTOR_H

#include "common/types.h"
#include "common/periodic_task.h"
#include "core/aircraft.h"
#include <vector>
#include <memory>
#include <string>
#include <mutex>

namespace atc {

struct ViolationInfo {
    std::string aircraft1_id;
    std::string aircraft2_id;
    double horizontal_separation;
    double vertical_separation;
    uint64_t timestamp;
    bool is_predicted;
    Position predicted_position1;
    Position predicted_position2;
    uint64_t prediction_time;
};

class ViolationDetector : public PeriodicTask {
public:
    explicit ViolationDetector(int lookahead_seconds = 180);

    void addAircraft(const std::shared_ptr<Aircraft>& aircraft);
    void removeAircraft(const std::string& callsign);

    void setLookaheadTime(int seconds);
    int getLookaheadTime() const;

    std::vector<ViolationInfo> getCurrentViolations() const;

protected:
    void execute() override;

private:
    std::vector<ViolationInfo> checkViolations() const;
    bool checkPairViolation(const AircraftState& state1,
                           const AircraftState& state2,
                           ViolationInfo& violation) const;
    bool checkFutureViolation(const AircraftState& state1,
                             const AircraftState& state2,
                             ViolationInfo& violation) const;

    Position predictPosition(const AircraftState& state, double time_seconds) const;
    void logViolation(const ViolationInfo& violation) const;

    std::vector<std::shared_ptr<Aircraft>> aircraft_;
    int lookahead_time_seconds_;
    mutable std::mutex mutex_;
};

}
#endif // ATC_VIOLATION_DETECTOR_H
