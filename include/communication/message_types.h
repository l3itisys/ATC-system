#ifndef ATC_MESSAGE_TYPES_H
#define ATC_MESSAGE_TYPES_H

#include "common/types.h"
#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <variant>
#include <chrono>

namespace atc {
namespace comm {

enum class MessageType {
    POSITION_UPDATE,    // Aircraft position updates
    COMMAND,           // Control commands
    ALERT,             // System alerts
    STATUS_REQUEST,    // Status information requests
    STATUS_RESPONSE,   // Status information responses
    OPERATOR_INPUT,    // Operator console input
    OPERATOR_RESPONSE  // Operator console responses
};

// Command data structure
struct CommandData {
    std::string target_id;
    std::string command;
    std::vector<std::string> params;

    CommandData() = default;

    CommandData(const std::string& id, const std::string& cmd)
        : target_id(id), command(cmd) {}

    CommandData(const std::string& id, const std::string& cmd,
               const std::vector<std::string>& parameters)
        : target_id(id), command(cmd), params(parameters) {}

    // Validation helper
    bool isValid() const {
        return !target_id.empty() && !command.empty();
    }
};

// Command-related constants
namespace commands {
    // Aircraft control commands
    const inline std::string CMD_ALTITUDE = "ALTITUDE";
    const inline std::string CMD_SPEED = "SPEED";
    const inline std::string CMD_HEADING = "HEADING";
    const inline std::string CMD_EMERGENCY = "EMERGENCY";
    const inline std::string CMD_STATUS = "STATUS";

    // Operator console commands
    const inline std::string CMD_HELP = "HELP";
    const inline std::string CMD_EXIT = "EXIT";
    const inline std::string CMD_CLEAR = "CLEAR";
    const inline std::string CMD_LIST = "LIST";
    const inline std::string CMD_MONITOR = "MONITOR";

    // Emergency states
    const inline std::string EMERGENCY_ON = "ON";
    const inline std::string EMERGENCY_OFF = "OFF";
}

// Alert levels
namespace alerts {
    const uint8_t LEVEL_INFO = 0;
    const uint8_t LEVEL_WARNING = 1;
    const uint8_t LEVEL_CRITICAL = 2;
    const uint8_t LEVEL_EMERGENCY = 3;
}

// Alert data structure
struct AlertData {
    uint8_t level;
    std::string description;
    std::chrono::system_clock::time_point timestamp;

    AlertData()
        : level(alerts::LEVEL_INFO)
        , timestamp(std::chrono::system_clock::now()) {}

    AlertData(uint8_t l, const std::string& desc)
        : level(l)
        , description(desc)
        , timestamp(std::chrono::system_clock::now()) {}

    // Helper method to check alert severity
    bool isCritical() const {
        return level >= alerts::LEVEL_CRITICAL;
    }
};

// Status response structure
struct StatusResponse {
    std::string target_id;
    std::string status_text;
    std::chrono::system_clock::time_point timestamp;

    StatusResponse()
        : timestamp(std::chrono::system_clock::now()) {}

    StatusResponse(const std::string& id, const std::string& text)
        : target_id(id)
        , status_text(text)
        , timestamp(std::chrono::system_clock::now()) {}
};

// Message payload variant type
using MessagePayload = std::variant<
    AircraftState,
    CommandData,
    AlertData,
    StatusResponse
>;

// Message structure
struct Message {
    MessageType type;
    std::string sender_id;
    uint64_t timestamp;
    MessagePayload payload;

    Message()
        : type(MessageType::STATUS_REQUEST)
        , timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::system_clock::now().time_since_epoch()).count())
        , payload(AircraftState{}) {}

    // Factory methods for creating different types of messages
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

    static Message createStatusResponse(const std::string& sender, const StatusResponse& status) {
        Message msg;
        msg.type = MessageType::STATUS_RESPONSE;
        msg.sender_id = sender;
        msg.payload = status;
        return msg;
    }

    // Helper methods
    bool isValid() const {
        return !sender_id.empty() && timestamp > 0;
    }

    bool isCommand() const {
        return type == MessageType::COMMAND;
    }

    bool isAlert() const {
        return type == MessageType::ALERT;
    }

    bool requiresResponse() const {
        return type == MessageType::STATUS_REQUEST ||
               type == MessageType::OPERATOR_INPUT;
    }
};

}
}

#endif // ATC_MESSAGE_TYPES_H
