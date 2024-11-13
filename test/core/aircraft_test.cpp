#include <gtest/gtest.h>
#include "core/aircraft.h"
#include "common/constants.h"

namespace atc {
namespace test {

class AircraftTest : public ::testing::Test {
protected:
    FlightCharacteristics characteristics;
    Position initial_pos;
    Velocity initial_vel;

    void SetUp() override {
        characteristics.model = "A340";
        characteristics.type = AircraftType::COMMERCIAL;
        characteristics.cruise_speed = 400;
        characteristics.max_speed = 500;
        characteristics.min_speed = 200;
        characteristics.max_altitude = 35000;
        characteristics.min_altitude = 15000;
        characteristics.max_climb_rate = 2000;
        characteristics.max_descent_rate = 2500;

        initial_pos.x = 50000;
        initial_pos.y = 50000;
        initial_pos.z = 20000;

        // Setup initial velocity (heading east at 400 units/s)
        initial_vel.setFromSpeedAndHeading(400, 90);
    }
};

TEST_F(AircraftTest, Initialization) {
    Aircraft aircraft("TEST123", initial_pos, initial_vel, characteristics);

    auto state = aircraft.getState();
    EXPECT_EQ(state.callsign, "TEST123");
    EXPECT_DOUBLE_EQ(state.position.x, 50000);
    EXPECT_DOUBLE_EQ(state.position.y, 50000);
    EXPECT_DOUBLE_EQ(state.position.z, 20000);
    EXPECT_DOUBLE_EQ(state.speed, 400);
    EXPECT_NEAR(state.heading, 90, 0.1);
}

TEST_F(AircraftTest, UpdateSpeed) {
    Aircraft aircraft("TEST123", initial_pos, initial_vel, characteristics);

    EXPECT_TRUE(aircraft.updateSpeed(450));

    auto state = aircraft.getState();
    EXPECT_DOUBLE_EQ(state.speed, 450);
}

TEST_F(AircraftTest, SpeedLimits) {
    Aircraft aircraft("TEST123", initial_pos, initial_vel, characteristics);

    EXPECT_FALSE(aircraft.updateSpeed(characteristics.min_speed - 1));
    EXPECT_FALSE(aircraft.updateSpeed(characteristics.max_speed + 1));

    auto state = aircraft.getState();
    EXPECT_DOUBLE_EQ(state.speed, 400);  // Should remain unchanged
}

TEST_F(AircraftTest, UpdateHeading) {
    Aircraft aircraft("TEST123", initial_pos, initial_vel, characteristics);

    EXPECT_TRUE(aircraft.updateHeading(180));

    auto state = aircraft.getState();
    EXPECT_DOUBLE_EQ(state.heading, 180);
}

TEST_F(AircraftTest, HeadingLimits) {
    Aircraft aircraft("TEST123", initial_pos, initial_vel, characteristics);

    EXPECT_FALSE(aircraft.updateHeading(-1));
    EXPECT_FALSE(aircraft.updateHeading(360));

    auto state = aircraft.getState();
    EXPECT_NEAR(state.heading, 90, 0.1);  // Should remain unchanged
}

TEST_F(AircraftTest, PositionUpdate) {
    Aircraft aircraft("TEST123", initial_pos, initial_vel, characteristics);

    aircraft.start();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    aircraft.stop();

    auto state = aircraft.getState();
    // Moving east at 400 units/s for 2 seconds
    EXPECT_NEAR(state.position.x, initial_pos.x + 800, 1.0);
    EXPECT_NEAR(state.position.y, initial_pos.y, 1.0);
    EXPECT_NEAR(state.position.z, initial_pos.z, 1.0);
}

TEST_F(AircraftTest, EmergencyStatus) {
    Aircraft aircraft("TEST123", initial_pos, initial_vel, characteristics);

    aircraft.declareEmergency();
    auto state = aircraft.getState();
    EXPECT_EQ(state.status, AircraftStatus::EMERGENCY);
    EXPECT_GT(state.alert_level, 0);

    aircraft.cancelEmergency();
    state = aircraft.getState();
    EXPECT_EQ(state.status, AircraftStatus::CRUISING);
    EXPECT_EQ(state.alert_level, 0);
}

}
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
