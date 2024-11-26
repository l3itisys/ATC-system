#include <gtest/gtest.h>
#include "core/radar_system.h"
#include "../mock_channel.h"

namespace atc {
namespace test {

class RadarSystemTest : public testing::Test {
protected:
    void SetUp() override {
        channel_ = std::make_shared<MockChannel>();
        radar_ = std::make_shared<RadarSystem>(channel_);

        test_aircraft_ = std::make_shared<Aircraft>("TEST001",
            Position{50000, 50000, 20000},
            Velocity{400, 0, 0});
    }

    std::shared_ptr<MockChannel> channel_;
    std::shared_ptr<RadarSystem> radar_;
    std::shared_ptr<Aircraft> test_aircraft_;
};

TEST_F(RadarSystemTest, TrackAircraft) {
    radar_->addAircraft(test_aircraft_);

    auto tracked = radar_->getTrackedAircraft();
    EXPECT_EQ(tracked.size(), 1);
    EXPECT_EQ(tracked[0].callsign, "TEST001");
}

TEST_F(RadarSystemTest, RemoveAircraft) {
    radar_->addAircraft(test_aircraft_);
    radar_->removeAircraft("TEST001");

    auto tracked = radar_->getTrackedAircraft();
    EXPECT_EQ(tracked.size(), 0);
}

} // namespace test
} // namespace atc
