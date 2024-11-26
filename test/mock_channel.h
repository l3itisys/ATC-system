#include "communication/qnx_channel.h"
#include <gmock/gmock.h>
#include <queue>

namespace atc {
namespace test {

class MockChannel : public comm::QnxChannel {
public:
    MOCK_METHOD(bool, initialize, (bool as_server), (override));
    MOCK_METHOD(bool, sendMessage, (const comm::Message& message), (override));
    MOCK_METHOD(bool, receiveMessage, (comm::Message& message, int timeout_ms), (override));

    // Helper methods for testing
    void queueMessage(const comm::Message& msg) {
        message_queue_.push(msg);
    }

    bool hasMessages() const {
        return !message_queue_.empty();
    }

private:
    std::queue<comm::Message> message_queue_;
};

} // namespace test
} // namespace atc
