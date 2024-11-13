#ifndef ATC_MESSAGE_TYPES_H
#define ATC_MESSAGE_TYPES_H

#include "common/types.h"
#include <string>
#include <vector>
#include <cstdint>

namespace atc {
namespace comm {

enum class MessageType {
    POSITION_UPDATE,    // Regular position updates
    COMMAND,           // Controller commands
    ALERT,            // System alerts
    STATUS_REQUEST,   // Request for aircraft status
    STATUS_RESPONSE   // Response with aircraft status
};

// Forward declare main message struct
struct Message;

// Command data structure
struct CommandData {
    std::string target_id;
    std::string command;
    std::vector<std::string> params;
};

// Alert data structure
struct AlertData {
    uint8_t level;
    std::string description;
};

// Message structure
struct Message {
    MessageType type;
    std::string sender_id;
    uint64_t timestamp;

    union {
        AircraftState aircraft_state;
        CommandData command_data;
        AlertData alert_data;
    } payload;
};

}
}

#endif // ATC_MESSAGE_TYPES_H
