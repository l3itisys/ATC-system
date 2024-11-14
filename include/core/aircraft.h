#ifndef ATC_AIRCRAFT_H
#define ATC_AIRCRAFT_H

#include "common/types.h"
#include "common/constants.h"
#include "common/periodic_task.h"
#include <memory>
#include <mutex>
#include <stdexcept>

namespace atc {

class Aircraft : public PeriodicTask {
public:
    Aircraft(const std::string& callsign,
            const Position& initial_pos,
            const Velocity& initial_vel);

    // State access
    AircraftState getState() const;

    // Control commands
    bool updateSpeed(double new_speed);
    bool updateHeading(double new_heading);
    bool updateAltitude(double new_altitude);

    // Emergency handling
    void declareEmergency();
    void cancelEmergency();

protected:
    void execute() override;

private:
    void updatePosition();
    bool validateSpeed(double speed) const;
    bool validateAltitude(double altitude) const;

    AircraftState state_;
    mutable std::mutex state_mutex_;
};

}
#endif // ATC_AIRCRAFT_H
