#ifndef ATC_QNX_CHANNEL_H
#define ATC_QNX_CHANNEL_H

#include "channel.h"
#include <string>
#include <mutex>

struct _name_attach;
typedef struct _name_attach name_attach_t;

namespace atc {
namespace comm {

class QnxChannel : public IChannel {
public:
    explicit QnxChannel(const std::string& channel_name);
    ~QnxChannel() override;

    bool initialize() override;
    bool sendMessage(const Message& message) override;
    bool receiveMessage(Message& message, int timeout_ms) override;

private:
    bool createChannel();
    void cleanup();

    std::string channel_name_;
    int channel_id_;
    int connection_id_;
    name_attach_t* attach_ptr_;
    mutable std::mutex channel_mutex_;
};

}
}

#endif // ATC_QNX_CHANNEL_H
