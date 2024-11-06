#include "sim/aircraft.h"
#include <cmath>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>
#include <sys/neutrino.h>

static std::atomic<uint32_t> next_aircraft_id(1);

Aircraft::Aircraft(const AircraftInput& input, const FlightData& data)
    : aircraft_id(next_aircraft_id++),
      flight_data(data),
      running(false) {
    initializeState(input);
    channel = std::make_unique<ChannelManager>("RADAR_CHANNEL");
}

Aircraft::~Aircraft() {
    stop();
}

void Aircraft::initializeState(const AircraftInput& input) {
    current_state.id = aircraft_id;
    current_state.callsign = input.callsign;
    current_state.x = input.initial_x;
    current_state.y = input.initial_y;
    current_state.z = input.initial_z;
    current_state.heading = input.initial_heading;
    current_state.speed = input.initial_speed;
    current_state.altitude = input.initial_z;
    current_state.status = AircraftStatus::ENTERING;
    current_state.alert_level = 0;
    current_state.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    double heading_rad = input.initial_heading * M_PI / 180.0;
    current_state.vx = input.initial_speed * cos(heading_rad);
    current_state.vy = input.initial_speed * sin(heading_rad);
    current_state.vz = 0;
}

bool Aircraft::initialize() {
    return channel->initialize();
}

bool Aircraft::start() {
    if (running) {
        return false;
    }

    running = true;
    sim_thread = std::thread(&Aircraft::simLoop, this);
    return true;
}

void Aircraft::stop() {
    running = false;
    if (sim_thread.joinable()) {
        sim_thread.join();
    }
}

AircraftState Aircraft::getState() const {
    return current_state;
}

bool Aircraft::updateSpeed(double new_speed) {
    current_state.speed = new_speed;

    double heading_rad = current_state.heading * M_PI / 180.0;
    current_state.vx = new_speed * cos(heading_rad);
    current_state.vy = new_speed * sin(heading_rad);
    return true;
}

bool Aircraft::updateHeading(double new_heading) {
    current_state.heading = new_heading;

    double heading_rad = new_heading * M_PI / 180.0;
    current_state.vx = current_state.speed * cos(heading_rad);
    current_state.vy = current_state.speed * sin(heading_rad);
    return true;
}

bool Aircraft::updateAltitude(double new_altitude) {
    current_state.altitude = new_altitude;
    current_state.z = new_altitude;
    return true;
}

void Aircraft::declareEmergency() {
    current_state.status = AircraftStatus::EMERGENCY;
}

void Aircraft::cancelEmergency() {
    current_state.status = AircraftStatus::CRUISING;
}

void Aircraft::simLoop() {
    const int UPDATE_INTERVAL_MS = 1000; // 1 second

    while (running) {
        auto start_time = std::chrono::steady_clock::now();

        updatePosition(UPDATE_INTERVAL_MS / 1000.0);

        // Send state update to Radar System
        sendStateUpdate();

        // Sleep for remaining time
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time
        );
        if (elapsed.count() < UPDATE_INTERVAL_MS) {
            std::this_thread::sleep_for(std::chrono::milliseconds(UPDATE_INTERVAL_MS - elapsed.count()));
        }
    }
}

void Aircraft::updatePosition(double delta_time) {
    // New position
    double new_x = calculateNewPosition(current_state.x, current_state.vx, delta_time);
    double new_y = calculateNewPosition(current_state.y, current_state.vy, delta_time);
    double new_z = calculateNewPosition(current_state.z, current_state.vz, delta_time);

    // Validate and update position
    if (isValidMovement(new_x, new_y, new_z)) {
        current_state.x = new_x;
        current_state.y = new_y;
        current_state.z = new_z;
        current_state.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    } else {
        handleBoundaryCondition();
    }
}

double Aircraft::calculateNewPosition(double current, double velocity, double delta_time) {
    return current + velocity * delta_time;
}

bool Aircraft::isValidMovement(double new_x, double new_y, double new_z) {
    // Airspace bounderies
    const double X_MIN = 0.0;
    const double X_MAX = 100000.0;
    const double Y_MIN = 0.0;
    const double Y_MAX = 100000.0;
    const double Z_MIN = 0.0;
    const double Z_MAX = 25000.0;

    return (new_x >= X_MIN && new_x <= X_MAX) &&
           (new_y >= Y_MIN && new_y <= Y_MAX) &&
           (new_z >= Z_MIN && new_z <= Z_MAX);
}

void Aircraft::handleBoundaryCondition() {
    // For simplicity, we'll stop the aircraft when it reaches the boundary
    running = false;
    std::cout << "Aircraft " << current_state.callsign << " has exited the airspace." << std::endl;
}

void Aircraft::sendStateUpdate() {
    ATCMessage msg;
    msg.type = MessageType::MSG_POSITION_UPDATE;
    msg.sender_id = aircraft_id;
    msg.timestamp = current_state.timestamp;
    msg.state = current_state; // Copy the current state

    channel->sendMessage(msg);
}

