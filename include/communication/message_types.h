#ifndef ATC_MESSAGE_TYPES_H
#define ATC_MESSAGE_TYPES_H

#include "common/types.h"
#include <string>
#include <vector>
#include <cstdint>
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

    bool isValid() const {
        return !target_id.empty() && !command.empty();
    }
};

// Command-related constants namespace
namespace commands {
    // Aircraft control commands
    static const std::string CMD_ALTITUDE = "ALTITUDE";
    static const std::string CMD_SPEED = "SPEED";
    static const std::string CMD_HEADING = "HEADING";
    static const std::string CMD_EMERGENCY = "EMERGENCY";
    static const std::string CMD_STATUS = "STATUS";

    // Operator console commands
    static const std::string CMD_HELP = "HELP";
    static const std::string CMD_EXIT = "EXIT";
    static const std::string CMD_CLEAR = "CLEAR";
    static const std::string CMD_LIST = "LIST";
    static const std::string CMD_MONITOR = "MONITOR";

    // Emergency states
    static const std::string EMERGENCY_ON = "ON";
    static const std::string EMERGENCY_OFF = "OFF";
}

// Alert levels namespace
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

// Message structure with union for payload
struct Message {
    MessageType type;
    std::string sender_id;
    uint64_t timestamp;

    // Union to replace std::variant
    union PayloadData {
        AircraftState aircraft_state;
        CommandData command_data;
        AlertData alert_data;
        StatusResponse status_response;

        PayloadData() : aircraft_state() {}  // Default constructor
        ~PayloadData() {}  // Destructor
    } payload;

    Message()
        : type(MessageType::STATUS_REQUEST)
        , timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::system_clock::now().time_since_epoch()).count()) {
        new (&payload.aircraft_state) AircraftState();
    }

    // Copy constructor
    Message(const Message& other)
        : type(other.type)
        , sender_id(other.sender_id)
        , timestamp(other.timestamp) {
        copyPayload(other);
    }

    // Assignment operator
    Message& operator=(const Message& other) {
        if (this != &other) {
            type = other.type;
            sender_id = other.sender_id;
            timestamp = other.timestamp;
            copyPayload(other);
        }
        return *this;
    }

    ~Message() {
        clearPayload();
    }

    // Factory methods
    static Message createPositionUpdate(const std::string& sender, const AircraftState& state) {
        Message msg;
        msg.type = MessageType::POSITION_UPDATE;
        msg.sender_id = sender;
        msg.payload.aircraft_state = state;
        return msg;
    }

    static Message createCommand(const std::string& sender, const CommandData& cmd) {
        Message msg;
        msg.type = MessageType::COMMAND;
        msg.sender_id = sender;
        new (&msg.payload.command_data) CommandData(cmd);
        return msg;
    }

    static Message createAlert(const std::string& sender, const AlertData& alert) {
        Message msg;
        msg.type = MessageType::ALERT;
        msg.sender_id = sender;
        new (&msg.payload.alert_data) AlertData(alert);
        return msg;
    }

    static Message createStatusResponse(const std::string& sender, const StatusResponse& status) {
        Message msg;
        msg.type = MessageType::STATUS_RESPONSE;
        msg.sender_id = sender;
        new (&msg.payload.status_response) StatusResponse(status);
        return msg;
    }

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

private:
    void clearPayload() {
        switch (type) {
            case MessageType::POSITION_UPDATE:
                payload.aircraft_state.~AircraftState();
                break;
            case MessageType::COMMAND:
                payload.command_data.~CommandData();
                break;
            case MessageType::ALERT:
                payload.alert_data.~AlertData();
                break;
            case MessageType::STATUS_RESPONSE:
                payload.status_response.~StatusResponse();
                break;
            default:
                break;
        }
    }

    void copyPayload(const Message& other) {
        clearPayload();
        switch (other.type) {
            case MessageType::POSITION_UPDATE:
                new (&payload.aircraft_state) AircraftState(other.payload.aircraft_state);
                break;
            case MessageType::COMMAND:
                new (&payload.command_data) CommandData(other.payload.command_data);
                break;
            case MessageType::ALERT:
                new (&payload.alert_data) AlertData(other.payload.alert_data);
                break;
            case MessageType::STATUS_RESPONSE:
                new (&payload.status_response) StatusResponse(other.payload.status_response);
                break;
            default:
                new (&payload.aircraft_state) AircraftState();
                break;
        }
    }
};

} // namespace comm
} // namespace atc

#endif // ATC_MESSAGE_TYPES_H
