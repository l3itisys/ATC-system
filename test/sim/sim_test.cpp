#include <gtest/gtest.h>
#include "sim/aircraft.h"

class AircraftTest : public ::testing::Test {
protected:
    AircraftInput input;
    FlightData flight_data;

    void SetUp() override {
        // Input data
        input.callsign = "TEST123";
        input.model = "A340";
        input.type = AircraftType::COMMERCIAL;
        input.entry_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        input.initial_x = 50000;
        input.initial_y = 50000;
        input.initial_z = 20000;
        input.initial_heading = 90;
        input.initial_speed = 400;

        // Flight data
        flight_data.callsign = "TEST123";
        flight_data.aircraft_model = "A340";
        flight_data.type = AircraftType::COMMERCIAL;
        flight_data.cruise_speed = 400;
        flight_data.max_speed = 500;
        flight_data.min_speed = 200;
        flight_data.max_altitude = 35000;
        flight_data.min_altitude = 15000;
    }
};

TEST_F(AircraftTest, Initialization) {
    Aircraft aircraft(input, flight_data);
    EXPECT_TRUE(aircraft.initialize());

    AircraftState state = aircraft.getState();
    EXPECT_EQ(state.callsign, "TEST123");
    EXPECT_DOUBLE_EQ(state.x, 50000);
    EXPECT_DOUBLE_EQ(state.y, 50000);
    EXPECT_DOUBLE_EQ(state.z, 20000);
}

TEST_F(AircraftTest, UpdateSpeed) {
    Aircraft aircraft(input, flight_data);
    aircraft.initialize();
    aircraft.updateSpeed(450);

    AircraftState state = aircraft.getState();
    EXPECT_DOUBLE_EQ(state.speed, 450);
}

TEST_F(AircraftTest, UpdateHeading) {
    Aircraft aircraft(input, flight_data);
    aircraft.initialize();
    aircraft.updateHeading(180);

    AircraftState state = aircraft.getState();
    EXPECT_DOUBLE_EQ(state.heading, 180);
}

TEST_F(AircraftTest, PositionUpdate) {
    Aircraft aircraft(input, flight_data);
    aircraft.initialize();
    aircraft.start();

    // Let the simulation run for 2 seconds
    std::this_thread::sleep_for(std::chrono::seconds(2));
    aircraft.stop();

    AircraftState state = aircraft.getState();
    // Since the aircraft is moving east at 400 units per second,
    // after 2 seconds, x should have increased by 800 units.
    EXPECT_NEAR(state.x, 50000 + 800, 1e-5);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

