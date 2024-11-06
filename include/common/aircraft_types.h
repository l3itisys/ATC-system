#ifndef AIRCRAFT_TYPES_H
#define AIRCRAFT_TYPES_H

#include <stdint.h>
#include <string>
#include <chrono>

enum class AircraftStatus {
  ENTERING,
  CRUISING,
  HOLDING,
  EXITING,
  EMERGENCY

};

enum class AircraftType {
  COMMERCIAL,
  PRIVATE
};

struct AircraftState {
  uint32_t id;
  std::string callsign;
  double x, y, z;
  double vx, vy, vz;
  double heading;
  double speed;
  double altitude;
  AircraftStatus status;
  uint64_t timestamp;
  uint8_t alert_level;

};

struct AircraftInput {
  std::string callsign;
  std::string model;
  AircraftType type;
  uint64_t entry_time;
  double initial_x;
  double initial_y;
  double initial_z;
  double initial_heading;
  double initial_speed;

};

// Flight data structure
struct FlightData {
    std::string callsign;
    std::string aircraft_model;
    AircraftType type;
    double cruise_speed;
    double max_speed;
    double min_speed;
    double max_altitude;
    double min_altitude;
};

#endif // AIRCRAFT_TYPES_H
