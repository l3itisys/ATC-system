#include "operator/operator_console.h"
#include "operator/command_processor.h"
#include "common/constants.h"
#include "gtest/gtest.h"
#include <memory>
#include <thread>
#include <chrono>

namespace atc {
namespace test {

class MockQnxChannel : public comm::QnxChannel {
public:
    MockQnxChannel() : QnxChannel("TEST_CHANNEL") {}

    bool initialize() override { return true; }

    bool sendMessage(const comm::Message& message) override {
        std::lock_guard<std::mutex> lock(mutex_);
        sent_messages_.push_back(message);
        return true;
    }

    bool receiveMessage(comm::Message& message, int timeout_ms) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (received_messages_.empty()) return false;
        message = received_messages_.front();
        received_messages_.pop_front();
        return true;
    }

    // Test helper methods
    void queueReceivedMessage(const comm::Message& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        received_messages_.push_back(message);
    }

    std::vector<comm::Message> getSentMessages() {
        std::lock_guard<std::mutex> lock(mutex_);
        return sent_messages_;
    }

    void clearMessages() {
        std::lock_guard<std::mutex> lock(mutex_);
        sent_messages_.clear();
        received_messages_.clear();
    }

private:
    std::mutex mutex_;
    std::vector<comm::Message> sent_messages_;
    std::deque<comm::Message> received_messages_;
};

class OperatorConsoleTest : public ::testing::Test {
protected:
    void SetUp() override {
        channel_ = std::make_shared<MockQnxChannel>();
        console_ = std::make_unique<OperatorConsole>(channel_);
        console_->setEchoEnabled(false);  // Disable echo for testing
    }

    void TearDown() override {
        console_.reset();
        channel_.reset();
    }

    void WaitForCommandProcessing() {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::shared_ptr<MockQnxChannel> channel_;
    std::unique_ptr<OperatorConsole> console_;
};

TEST_F(OperatorConsoleTest, InitializationTest) {
    EXPECT_TRUE(console_->isOperational());
    EXPECT_EQ(console_->getProcessedCommandCount(), 0);
    EXPECT_EQ(console_->getCommandQueueSize(), 0);
}

TEST_F(OperatorConsoleTest, ValidAltitudeCommand) {
    console_->inputCommand("ALT AC001 20000");
    WaitForCommandProcessing();

    auto messages = channel_->getSentMessages();
    ASSERT_EQ(messages.size(), 1);

    const auto& msg = messages[0];
    EXPECT_EQ(msg.type, comm::MessageType::COMMAND);

    const auto& cmd = std::get<comm::CommandData>(msg.payload);
    EXPECT_EQ(cmd.target_id, "AC001");
    EXPECT_EQ(cmd.command, "ALTITUDE");
    ASSERT_EQ(cmd.params.size(), 1);
    EXPECT_EQ(cmd.params[0], "20000");
}

TEST_F(OperatorConsoleTest, InvalidAltitudeCommand) {
    console_->inputCommand("ALT AC001 999999");  // Above max altitude
    WaitForCommandProcessing();
    EXPECT_EQ(channel_->getSentMessages().size(), 0);
}

TEST_F(OperatorConsoleTest, ValidSpeedCommand) {
    console_->inputCommand("SPD AC001 300");
    WaitForCommandProcessing();

    auto messages = channel_->getSentMessages();
    ASSERT_EQ(messages.size(), 1);

    const auto& msg = messages[0];
    EXPECT_EQ(msg.type, comm::MessageType::COMMAND);

    const auto& cmd = std::get<comm::CommandData>(msg.payload);
    EXPECT_EQ(cmd.target_id, "AC001");
    EXPECT_EQ(cmd.command, "SPEED");
    ASSERT_EQ(cmd.params.size(), 1);
    EXPECT_EQ(cmd.params[0], "300");
}

TEST_F(OperatorConsoleTest, InvalidSpeedCommand) {
    console_->inputCommand("SPD AC001 1000");  // Above max speed
    WaitForCommandProcessing();
    EXPECT_EQ(channel_->getSentMessages().size(), 0);
}

TEST_F(OperatorConsoleTest, ValidHeadingCommand) {
    console_->inputCommand("HDG AC001 090");
    WaitForCommandProcessing();

    auto messages = channel_->getSentMessages();
    ASSERT_EQ(messages.size(), 1);

    const auto& msg = messages[0];
    const auto& cmd = std::get<comm::CommandData>(msg.payload);
    EXPECT_EQ(cmd.command, "HEADING");
    EXPECT_EQ(cmd.params[0], "090");
}

TEST_F(OperatorConsoleTest, EmergencyCommands) {
    console_->inputCommand("EMERG AC001 ON");
    WaitForCommandProcessing();

    auto messages = channel_->getSentMessages();
    ASSERT_EQ(messages.size(), 1);

    const auto& cmd = std::get<comm::CommandData>(messages[0].payload);
    EXPECT_EQ(cmd.command, "EMERGENCY");
    EXPECT_EQ(cmd.params[0], "1");

    channel_->clearMessages();

    console_->inputCommand("EMERG AC001 OFF");
    WaitForCommandProcessing();

    messages = channel_->getSentMessages();
    ASSERT_EQ(messages.size(), 1);
    const auto& cmd2 = std::get<comm::CommandData>(messages[0].payload);
    EXPECT_EQ(cmd2.params[0], "0");
}

TEST_F(OperatorConsoleTest, CommandHistory) {
    std::vector<std::string> commands = {
        "ALT AC001 20000",
        "SPD AC001 300",
        "HDG AC001 090"
    };

    for (const auto& cmd : commands) {
        console_->inputCommand(cmd);
        WaitForCommandProcessing();
    }

    EXPECT_EQ(console_->getProcessedCommandCount(), commands.size());
}

TEST_F(OperatorConsoleTest, QueueManagement) {
    for (int i = 0; i < 150; ++i) {  // Try to overflow queue
        console_->inputCommand("STATUS");
    }

    // Queue size should be limited
    EXPECT_LE(console_->getCommandQueueSize(), 100);
}

TEST_F(OperatorConsoleTest, ConcurrencyTest) {
    std::atomic<bool> running{true};
    std::vector<std::thread> threads;

    // Create multiple threads sending commands
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([this, &running, i]() {
            while (running) {
                std::string cmd = "STATUS AC00" + std::to_string(i);
                console_->inputCommand(cmd);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    // Let it run for a short time
    std::this_thread::sleep_for(std::chrono::seconds(1));
    running = false;

    // Clean up threads
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify no crashes and commands were processed
    EXPECT_GT(console_->getProcessedCommandCount(), 0);
}

TEST_F(OperatorConsoleTest, HelpCommand) {
    console_->inputCommand("HELP");
    WaitForCommandProcessing();
    EXPECT_EQ(channel_->getSentMessages().size(), 0);  // Help doesn't send messages

    console_->inputCommand("HELP ALT");
    WaitForCommandProcessing();
    EXPECT_EQ(channel_->getSentMessages().size(), 0);
}

TEST_F(OperatorConsoleTest, InvalidCommands) {
    std::vector<std::string> invalid_commands = {
        "",                     // Empty command
        "INVALID",             // Unknown command
        "ALT",                 // Missing parameters
        "SPD AC001",          // Missing speed value
        "HDG AC001 ABC",      // Invalid heading value
        "EMERG AC001 MAYBE"   // Invalid emergency state
    };

    for (const auto& cmd : invalid_commands) {
        console_->inputCommand(cmd);
        WaitForCommandProcessing();
        EXPECT_EQ(channel_->getSentMessages().size(), 0);
        channel_->clearMessages();
    }
}

}
}
