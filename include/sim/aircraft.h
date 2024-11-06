#ifndef AIRCRAFT_H
#define AIRCRAFT_H

#include <thread>
#include <atomic>
#include <memory>
#include "common/aircraft_types.h"
#include "sim/channel_manager.h"

class Aircraft {
public:
    Aircraft(const AircraftInput& input, const FlightData& data);
    ~Aircraft();

    bool initialize();
    bool start();
    void stop();

    AircraftState getState() const;

    bool updateSpeed(double new_speed);
    bool updateHeading(double new_heading);
    bool updateAltitude(double new_altitude);

    // Emergency
    void declareEmergency();
    void cancelEmergency();

private:
    // Aircraft data
    uint32_t aircraft_id;
    FlightData flight_data;
    AircraftState current_state;
    std::atomic<bool> running;

    std::unique_ptr<ChannelManager> channel;

    std::thread sim_thread;

    void simLoop();
    void updatePosition(double delta_time);
    bool isValidMovement(double new_x, double new_y, double new_z);
    void sendStateUpdate();
    void initializeState(const AircraftInput& input);
    double calculateNewPosition(double current, double velocity, double delta_time);
    void handleBoundaryCondition();
};

#endif // AIRCRAFT_H

