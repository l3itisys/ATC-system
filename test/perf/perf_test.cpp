#include <gtest/gtest.h>
#include "main_system.h"
#include <chrono>
#include <random>

namespace atc {
namespace test {

class PerformanceTest : public testing::Test {
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

    // Helper to generate test aircraft data
    void generateTestData(int aircraft_count, const std::string& filename) {
        std::ofstream file(filename);
        file << "Time,ID,X,Y,Z,SpeedX,SpeedY,SpeedZ\n";

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> pos_dist(0, 100000);
        std::uniform_real_distribution<> alt_dist(15000, 25000);
        std::uniform_real_distribution<> speed_dist(-400, 400);

        for (int i = 0; i < aircraft_count; ++i) {
            file << "0,AC" << std::setfill('0') << std::setw(3) << i << ","
                 << pos_dist(gen) << ","
                 << pos_dist(gen) << ","
                 << alt_dist(gen) << ","
                 << speed_dist(gen) << ","
                 << speed_dist(gen) << ","
                 << "0\n";
        }
    }

    std::unique_ptr<MainSystem> system_;
};

TEST_F(PerformanceTest, HighLoadScenario) {
    // Generate test data with 100 aircraft
    generateTestData(100, "high_load_test.csv");

    auto start_time = std::chrono::steady_clock::now();

    // Load test data
    ASSERT_TRUE(system_->loadAircraftData("high_load_test.csv"));

    // Run system for 30 seconds
    std::thread system_thread([this]() {
        system_->run();
    });

    std::this_thread::sleep_for(std::chrono::seconds(30));

    // Get metrics
    auto metrics = system_->getMetrics();
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        end_time - start_time).count();

    // Verify performance requirements
    EXPECT_GT(metrics.violation_checks / duration, 10);  // At least 10 checks per second
    EXPECT_LT(metrics.violations_detected, 10);          // Less than 10 actual violations

    // Cleanup
    system_->shutdown();
    if (system_thread.joinable()) {
        system_thread.join();
    }
}

TEST_F(PerformanceTest, ResourceUsage) {
    // Generate test data with 50 aircraft
    generateTestData(50, "resource_test.csv");

    // Load test data
    ASSERT_TRUE(system_->loadAircraftData("resource_test.csv"));

    // Run system for 10 seconds
    std::thread system_thread([this]() {
        system_->run();
    });

    // Monitor resource usage
    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        auto metrics = system_->getMetrics();

        // Verify resource usage
        // Add specific checks based on your requirements
    }

    // Cleanup
    system_->shutdown();
    if (system_thread.joinable()) {
        system_thread.join();
    }
}

} // namespace test
} // namespace atc
