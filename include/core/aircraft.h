#ifndef ATC_AIRCRAFT_H
#define ATC_AIRCRAFT_H

#include "common/periodic_task.h"
#include "common/types.h"
#include <mutex>
#include <string>

namespace atc {

class Aircraft : public PeriodicTask {
public:
    Aircraft(const std::string& callsign,
             const Position& initial_pos,
             const Velocity& initial_vel);
    ~Aircraft() = default;

    void declareEmergency();
    void cancelEmergency();

    // Methods to update aircraft parameters
    bool updateSpeed(double new_speed);
    bool updateHeading(double new_heading);
    bool updateAltitude(double new_altitude);

    // Method to get current state
    AircraftState getState() const;

    // Static method to get status string
    static std::string getStatusString(AircraftStatus status);

protected:
    void execute() override;

private:
    void updatePosition();
    bool validateSpeed(double speed) const;
    bool validateAltitude(double altitude) const;
    void logState(const std::string& event, const AircraftState& state);

    mutable std::mutex state_mutex_;
    AircraftState state_;
};

} // namespace atc

#endif // ATC_AIRCRAFT_H

