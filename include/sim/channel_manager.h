#ifndef CHANNEL_MANAGER_H
#define CHANNEL_MANAGER_H

#include <string>
#include <atomic>
#include <sys/neutrino.h>
#include "common/aircraft_types.h"

enum class MessageType {
    MSG_POSITION_UPDATE,
    MSG_COMMAND
};

// ATC message structure
struct ATCMessage {
    MessageType type;
    uint32_t sender_id;
    uint64_t timestamp;
    AircraftState state;  // For position updates
    // Additional fields for commands if necessary
};

class ChannelManager {
public:
    ChannelManager(const std::string& channel_name);
    ~ChannelManager();

    bool initialize();
    bool sendMessage(const ATCMessage& message);

private:
    std::string channel_name;
    int server_coid;  // Connection ID to the server
    static std::atomic<bool> name_attached;
};

#endif // CHANNEL_MANAGER_H

