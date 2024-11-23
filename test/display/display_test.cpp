#include <gtest/gtest.h>
#include "display/display_system.h"
#include "core/violation_detector.h"
#include <memory>

namespace atc {
namespace test {

class DisplaySystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        violation_detector_ = std::make_shared<ViolationDetector>();
        display_system_ = std::make_shared<DisplaySystem>(violation_detector_);
    }

    void TearDown() override {
        display_system_->stop();
    }

    std::shared_ptr<ViolationDetector> violation_detector_;
    std::shared_ptr<DisplaySystem> display_system_;
};

TEST_F(DisplaySystemTest, AddRemoveAircraft) {
    Position pos{50000, 50000, 20000};
    Velocity vel{100, 0, 0};
    auto aircraft = std::make_shared<Aircraft>("TEST1", pos, vel);

    display_system_->addAircraft(aircraft);
    display_system_->selectAircraft("TEST1");

    // Test removal
    display_system_->removeAircraft("TEST1");
    // Selection should be cleared
    display_system_->execute();  // Trigger a display update
}

TEST_F(DisplaySystemTest, RefreshRateLimit) {
    // Test minimum refresh rate
    display_system_->setRefreshRate(500);  // Too fast
    EXPECT_GE(display_system_->getPeriod().count(), DisplaySystem::MIN_REFRESH_RATE);

    // Test maximum refresh rate
    display_system_->setRefreshRate(15000);  // Too slow
    EXPECT_LE(display_system_->getPeriod().count(), DisplaySystem::MAX_REFRESH_RATE);
}

TEST_F(DisplaySystemTest, ViolationDisplay) {
    // Create two aircraft in violation
    Position pos1{50000, 50000, 20000};
    Position pos2{50100, 50100, 20000};
    Velocity vel{0, 0, 0};

    auto aircraft1 = std::make_shared<Aircraft>("TEST1", pos1, vel);
    auto aircraft2 = std::make_shared<Aircraft>("TEST2", pos2, vel);

    violation_detector_->addAircraft(aircraft1);
    violation_detector_->addAircraft(aircraft2);
    display_system_->addAircraft(aircraft1);
    display_system_->addAircraft(aircraft2);

    // Test violation prediction toggle
    display_system_->toggleViolationPrediction(false);
    display_system_->execute();

    display_system_->toggleViolationPrediction(true);
    display_system_->execute();
}

}
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
