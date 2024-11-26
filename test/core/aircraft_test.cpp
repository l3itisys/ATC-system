#include <gtest/gtest.h>
#include "core/aircraft.h"
#include "common/constants.h"

namespace atc {
namespace test {

class AircraftTest : public testing::Test {
protected:
    void SetUp() override {
        initial_pos_ = Position{50000, 50000, 20000};
        initial_vel_ = Velocity{400, 0, 0};
        aircraft_ = std::make_shared<Aircraft>("TEST001", initial_pos_, initial_vel_);
    }

    Position initial_pos_;
    Velocity initial_vel_;
    std::shared_ptr<Aircraft> aircraft_;
};

TEST_F(AircraftTest, Initialization) {
    auto state = aircraft_->getState();
    EXPECT_EQ(state.callsign, "TEST001");
    EXPECT_EQ(state.position.x, initial_pos_.x);
    EXPECT_EQ(state.position.y, initial_pos_.y);
    EXPECT_EQ(state.position.z, initial_pos_.z);
    EXPECT_EQ(state.velocity.vx, initial_vel_.vx);
    EXPECT_EQ(state.velocity.vy, initial_vel_.vy);
    EXPECT_EQ(state.velocity.vz, initial_vel_.vz);
    EXPECT_EQ(state.status, AircraftStatus::ENTERING);
}

TEST_F(AircraftTest, UpdateSpeed) {
    EXPECT_TRUE(aircraft_->updateSpeed(300));
    auto state = aircraft_->getState();
    EXPECT_NEAR(state.getSpeed(), 300, 0.1);

    // Test invalid speed
    EXPECT_FALSE(aircraft_->updateSpeed(constants::MAX_SPEED + 100));
}

TEST_F(AircraftTest, UpdateHeading) {
    EXPECT_TRUE(aircraft_->updateHeading(90));
    auto state = aircraft_->getState();
    EXPECT_NEAR(state.heading, 90, 0.1);

    // Test invalid heading
    EXPECT_FALSE(aircraft_->updateHeading(400));
}

TEST_F(AircraftTest, UpdateAltitude) {
    double new_altitude = 19000;
    EXPECT_TRUE(aircraft_->updateAltitude(new_altitude));
    auto state = aircraft_->getState();
    EXPECT_EQ(state.position.z, new_altitude);

    // Test invalid altitude
    EXPECT_FALSE(aircraft_->updateAltitude(constants::AIRSPACE_Z_MAX + 1000));
}

TEST_F(AircraftTest, EmergencyHandling) {
    aircraft_->declareEmergency();
    auto state = aircraft_->getState();
    EXPECT_EQ(state.status, AircraftStatus::EMERGENCY);

    aircraft_->cancelEmergency();
    state = aircraft_->getState();
    EXPECT_EQ(state.status, AircraftStatus::CRUISING);
}

} // namespace test
} // namespace atc
