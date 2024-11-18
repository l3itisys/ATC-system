#ifndef ATC_MESSAGE_TYPES_H
#define ATC_MESSAGE_TYPES_H

#include "common/types.h"
#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <variant>

namespace atc {
namespace comm {

enum class MessageType {
    POSITION_UPDATE,    // Regular position updates
    COMMAND,           // Controller commands
    ALERT,            // System alerts
    STATUS_REQUEST,   // Request for aircraft status
    STATUS_RESPONSE   // Response with aircraft status
};

// Command data structure
struct CommandData {
    std::string target_id;
    std::string command;
    std::vector<std::string> params;

    CommandData() = default;
    CommandData(const std::string& id, const std::string& cmd)
        : target_id(id), command(cmd) {}
};

// Alert data structure
struct AlertData {
    uint8_t level;
    std::string description;

    AlertData() : level(0) {}
    AlertData(uint8_t l, const std::string& desc)
        : level(l), description(desc) {}
};

// Message payload variant type
using MessagePayload = std::variant<AircraftState, CommandData, AlertData>;

// Message structure
struct Message {
    MessageType type;
    std::string sender_id;
    uint64_t timestamp;
    MessagePayload payload;

    Message()
        : type(MessageType::STATUS_REQUEST)
        , timestamp(0)
        , payload(AircraftState{}) {}

    static Message createPositionUpdate(const std::string& sender, const AircraftState& state) {
        Message msg;
        msg.type = MessageType::POSITION_UPDATE;
        msg.sender_id = sender;
        msg.payload = state;
        return msg;
    }

    static Message createCommand(const std::string& sender, const CommandData& cmd) {
        Message msg;
        msg.type = MessageType::COMMAND;
        msg.sender_id = sender;
        msg.payload = cmd;
        return msg;
    }

    static Message createAlert(const std::string& sender, const AlertData& alert) {
        Message msg;
        msg.type = MessageType::ALERT;
        msg.sender_id = sender;
        msg.payload = alert;
        return msg;
    }
};

}
}

#endif // ATC_MESSAGE_TYPES_H
