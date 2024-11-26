#include <gtest/gtest.h>
#include "main_system.h"
#include <thread>
#include <chrono>

namespace atc {
namespace test {

class SystemIntegrationTest : public testing::Test {
protected:
    void SetUp() override {
        system_ = std::make_unique<MainSystem>();
        ASSERT_TRUE(system_->initialize());
    }

    void TearDown() override {
        if (system_) {
            system_->shutdown();
        }
    }

    std::unique_ptr<MainSystem> system_;
};

TEST_F(SystemIntegrationTest, SystemStartup) {
    // Load test data
    EXPECT_TRUE(system_->loadAircraftData("test_data.csv"));

    // Start system in separate thread
    std::thread system_thread([this]() {
        system_->run();
    });

    // Allow system to run for a few seconds
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Verify system is running
    EXPECT_TRUE(system_->isRunning());

    // Trigger shutdown
    system_->shutdown();
    if (system_thread.joinable()) {
        system_thread.join();
    }
}

TEST_F(SystemIntegrationTest, ComponentCommunication) {
    // Load test data
    EXPECT_TRUE(system_->loadAircraftData("test_data.csv"));

    // Start system
    std::thread system_thread([this]() {
        system_->run();
    });

    // Allow system to initialize
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Check metrics
    auto metrics = system_->getMetrics();
    EXPECT_GT(metrics.active_aircraft, 0);
    EXPECT_GT(metrics.violation_checks, 0);

    // Cleanup
    system_->shutdown();
    if (system_thread.joinable()) {
        system_thread.join();
    }
}

} // namespace test
} // namespace atc
