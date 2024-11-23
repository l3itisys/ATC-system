#include "core/radar_system.h"
#include "core/aircraft.h"
#include "common/logger.h"
#include <gtest/gtest.h>
#include <memory>
#include <thread>

namespace atc {
namespace test {

class RadarSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        channel_ = std::make_shared<comm::QnxChannel>("TEST_CHANNEL");
        ASSERT_TRUE(channel_->initialize());
        radar_ = std::make_shared<RadarSystem>(channel_);
    }

    void TearDown() override {
        radar_.reset();
        channel_.reset();
    }

    std::shared_ptr<comm::QnxChannel> channel_;
    std::shared_ptr<RadarSystem> radar_;
};

TEST_F(RadarSystemTest, BasicTracking) {
    // Create test aircraft
    Position pos{50000, 50000, 20000};
    Velocity vel{100, 0, 0};
    auto aircraft = std::make_shared<Aircraft>("TEST1", pos, vel);

    // Add to radar
    radar_->addAircraft(aircraft);

    // Start radar and aircraft
    radar_->start();
    aircraft->start();

    // Wait for tracking to establish
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Verify tracking
    EXPECT_TRUE(radar_->isAircraftTracked("TEST1"));

    auto state = radar_->getAircraftState("TEST1");
    EXPECT_EQ(state.callsign, "TEST1");

    // Verify position is within error bounds
    const double MAX_ERROR = 100.0;  // Maximum allowed position error
    EXPECT_NEAR(state.position.x, pos.x, MAX_ERROR);
    EXPECT_NEAR(state.position.y, pos.y, MAX_ERROR);
    EXPECT_NEAR(state.position.z, pos.z, MAX_ERROR);

    // Stop tracking
    aircraft->stop();
    radar_->stop();
}

TEST_F(RadarSystemTest, MultipleAircraft) {
    std::vector<std::shared_ptr<Aircraft>> aircraft;
    const int NUM_AIRCRAFT = 5;

    // Create multiple test aircraft
    for (int i = 0; i < NUM_AIRCRAFT; i++) {
        Position pos{40000.0 + i * 5000.0, 50000.0, 20000.0};
        Velocity vel{100.0, 0.0, 0.0};
        auto ac = std::make_shared<Aircraft>(
            "TEST" + std::to_string(i), pos, vel);
        aircraft.push_back(ac);
        radar_->addAircraft(ac);
    }

    // Start radar and aircraft
    radar_->start();
    for (auto& ac : aircraft) {
        ac->start();
    }

    // Wait for tracking to establish
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Verify all aircraft are tracked
    auto tracked = radar_->getTrackedAircraft();
    EXPECT_EQ(tracked.size(), NUM_AIRCRAFT);

    // Cleanup
    for (auto& ac : aircraft) {
        ac->stop();
    }
    radar_->stop();
}

TEST_F(RadarSystemTest, TrackingLoss) {
    // Create test aircraft outside airspace boundaries
    Position pos{50000, 50000, 20000};
    Velocity vel{1000, 0, 0};  // High speed to quickly exit airspace
    auto aircraft = std::make_shared<Aircraft>("TEST1", pos, vel);

    radar_->addAircraft(aircraft);

    // Start tracking
    radar_->start();
    aircraft->start();

    // Wait for initial tracking
    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_TRUE(radar_->isAircraftTracked("TEST1"));

    // Wait for aircraft to exit airspace
    std::this_thread::sleep_for(std::chrono::seconds(60));
    EXPECT_FALSE(radar_->isAircraftTracked("TEST1"));

    // Cleanup
    aircraft->stop();
    radar_->stop();
}

TEST_F(RadarSystemTest, RadarPerformance) {
    const int NUM_AIRCRAFT = 20;  // Test with high load
    std::vector<std::shared_ptr<Aircraft>> aircraft;

    // Create multiple aircraft
    for (int i = 0; i < NUM_AIRCRAFT; i++) {
        Position pos{
            40000.0 + (i * 2000.0),
            50000.0 + (i * 1000.0),
            20000.0 + (i * 500.0)
        };
        Velocity vel{100.0, 50.0, 0.0};
        auto ac = std::make_shared<Aircraft>(
            "PERF" + std::to_string(i), pos, vel);
        aircraft.push_back(ac);
        radar_->addAircraft(ac);
    }

    // Measure execution time under load
    auto start_time = std::chrono::steady_clock::now();

    radar_->start();
    for (auto& ac : aircraft) {
        ac->start();
    }

    // Run for 30 seconds
    std::this_thread::sleep_for(std::chrono::seconds(30));

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time);

    // Log performance metrics
    std::cout << "Performance Test Results:\n"
              << "Number of aircraft: " << NUM_AIRCRAFT << "\n"
              << "Total execution time: " << duration.count() << " microseconds\n"
              << "Average time per aircraft: "
              << duration.count() / NUM_AIRCRAFT << " microseconds\n";

    // Verify tracking quality under load
    auto tracked = radar_->getTrackedAircraft();
    EXPECT_EQ(tracked.size(), NUM_AIRCRAFT);

    // Cleanup
    for (auto& ac : aircraft) {
        ac->stop();
    }
    radar_->stop();
}

}
}
