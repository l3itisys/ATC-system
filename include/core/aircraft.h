#ifndef ATC_AIRCRAFT_H
#define ATC_AIRCRAFT_H

#include "common/types.h"
#include "common/constants.h"
#include "common/periodic_task.h"
#include <memory>
#include <mutex>

namespace atc {

class Aircraft : public PeriodicTask {
public:
    Aircraft(const std::string& callsign,
            const Position& initial_pos,
            const Velocity& initial_vel,
            const FlightCharacteristics& characteristics);

    // State access
    AircraftState getState() const;

    // Control commands
    bool updateSpeed(double new_speed);
    bool updateHeading(double new_heading);
    bool updateAltitude(double new_altitude);
    void declareEmergency();
    void cancelEmergency();

protected:
    void execute() override;

private:
    void updatePosition();

    AircraftState state_;
    FlightCharacteristics characteristics_;
    mutable std::mutex state_mutex_;
};

}

#endif // ATC_AIRCRAFT_H
