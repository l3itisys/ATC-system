#include "display/display_system.h"
#include "core/violation_detector.h"
#include "common/constants.h"
#include <gtest/gtest.h>

namespace atc {
namespace test {

class DisplaySystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        violation_detector_ = std::make_shared<ViolationDetector>();
        display_system_ = std::make_unique<DisplaySystem>(violation_detector_);
    }

    // Helper to create test aircraft
    std::shared_ptr<Aircraft> createTestAircraft(
        const std::string& id,
        double x, double y, double z,
        double vx, double vy, double vz) {

        Position pos{x, y, z};
        Velocity vel{vx, vy, vz};
        return std::make_shared<Aircraft>(id, pos, vel);
    }

    std::shared_ptr<ViolationDetector> violation_detector_;
    std::unique_ptr<DisplaySystem> display_system_;
};

TEST_F(DisplaySystemTest, BasicDisplay) {
    // Add single aircraft
    auto ac1 = createTestAircraft("AC001", 20000, 50000, 20000, 400, 0, 0);
    display_system_->addAircraft(ac1);

    // Verify display updates without crashing
    ASSERT_NO_THROW(display_system_->updateDisplay());
}

TEST_F(DisplaySystemTest, CollisionWarning) {
    // Create two aircraft on collision course
    auto ac1 = createTestAircraft("AC001", 20000, 50000, 20000, 400, 0, 0);
    auto ac2 = createTestAircraft("AC002", 80000, 50000, 20000, -400, 0, 0);

    display_system_->addAircraft(ac1);
    display_system_->addAircraft(ac2);
    violation_detector_->addAircraft(ac1);
    violation_detector_->addAircraft(ac2);

    // Let them move towards each other
    for(int i = 0; i < 10; i++) {
        ac1->execute();
        ac2->execute();
        display_system_->updateDisplay();

        // Verify violations are detected and displayed
        auto violations = violation_detector_->getCurrentViolations();
        if(!violations.empty()) {
            // Should see warning indicators
            ASSERT_NO_THROW(display_system_->updateDisplay());
        }
    }
}

// Test aircraft at different altitudes
    auto ac_high = createTestAircraft("AC001", 20000, 50000, 22000, 400, 0, 0);  // High altitude
    auto ac_mid = createTestAircraft("AC002", 40000, 50000, 20000, 400, 0, 0);   // Mid altitude
    auto ac_low = createTestAircraft("AC003", 60000, 50000, 18000, 400, 0, 0);   // Low altitude

    display_system_->addAircraft(ac_high);
    display_system_->addAircraft(ac_mid);
    display_system_->addAircraft(ac_low);

    // Verify display updates
    ASSERT_NO_THROW(display_system_->updateDisplay());
}

TEST_F(DisplaySystemTest, RemoveAircraft) {
    auto ac1 = createTestAircraft("AC001", 20000, 50000, 20000, 400, 0, 0);
    display_system_->addAircraft(ac1);
    display_system_->updateDisplay();

    // Remove aircraft
    display_system_->removeAircraft("AC001");
    display_system_->updateDisplay();

    // Add different aircraft
    auto ac2 = createTestAircraft("AC002", 40000, 50000, 20000, 400, 0, 0);
    display_system_->addAircraft(ac2);
    ASSERT_NO_THROW(display_system_->updateDisplay());
}

TEST_F(DisplaySystemTest, AlertDisplay) {
    auto ac1 = createTestAircraft("AC001", 20000, 50000, 20000, 400, 0, 0);
    display_system_->addAircraft(ac1);

    // Test alert display
    display_system_->displayAlert("Test Alert Message");
    ASSERT_NO_THROW(display_system_->updateDisplay());
}

TEST_F(DisplaySystemTest, LoadTest) {
    // Add multiple aircraft
    for(int i = 0; i < 20; i++) {
        auto ac = createTestAircraft(
            "AC" + std::to_string(i),
            20000 + i * 3000,
            50000,
            20000,
            400, 0, 0
        );
        display_system_->addAircraft(ac);
    }

    // Verify system handles load
    ASSERT_NO_THROW(display_system_->updateDisplay());
}

} // namespace test
} // namespace atc

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
