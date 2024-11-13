#ifndef ATC_CHANNEL_H
#define ATC_CHANNEL_H

#include "message_types.h"
#include <string>

namespace atc {
namespace comm {

class IChannel {
public:
    virtual ~IChannel() = default;

    virtual bool initialize() = 0;
    virtual bool sendMessage(const Message& message) = 0;
    virtual bool receiveMessage(Message& message, int timeout_ms) = 0;
};

}
}

#endif // ATC_CHANNEL_H
