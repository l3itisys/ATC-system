#ifndef ATC_COMMAND_PROCESSOR_H
#define ATC_COMMAND_PROCESSOR_H

#include "communication/message_types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>

namespace atc {

class CommandProcessor {
public:
    CommandProcessor();
    ~CommandProcessor() = default;

    struct CommandResult {
        bool success;
        std::string message;
        comm::Message msg;  // Changed from optional to direct Message
        bool has_message;   // Flag to indicate if message is valid

        CommandResult(bool s = false, const std::string& msg_str = "",
                     const comm::Message& message = comm::Message())
            : success(s)
            , message(msg_str)
            , msg(message)
            , has_message(false) {}
    };

    // Command processing
    CommandResult processCommand(const std::string& command_line);
    std::string getHelpText() const;
    std::string getCommandHelp(const std::string& command) const;

private:
    struct CommandInfo {
        std::string syntax;
        std::string description;
        std::vector<std::string> examples;
    };

    struct ParsedCommand {
        std::string command;
        std::string aircraft_id;
        std::vector<std::string> parameters;
    };

    struct CommandDefinition {
        std::function<CommandResult(CommandProcessor*, const ParsedCommand&)> handler;
        CommandInfo info;
        size_t min_params;
        size_t max_params;
    };

    // Command handlers
    CommandResult handleAltitudeCommand(const ParsedCommand& cmd);
    CommandResult handleSpeedCommand(const ParsedCommand& cmd);
    CommandResult handleHeadingCommand(const ParsedCommand& cmd);
    CommandResult handleEmergencyCommand(const ParsedCommand& cmd);
    CommandResult handleStatusCommand(const ParsedCommand& cmd);
    CommandResult handleHelpCommand(const ParsedCommand& cmd);
    CommandResult handleTrackCommand(const ParsedCommand& cmd);
    CommandResult handleDisplayCommand(const ParsedCommand& cmd);

    // Helper methods
    void initializeCommandDefinitions();
    ParsedCommand parseCommandLine(const std::string& command_line) const;
    bool validateParameters(const ParsedCommand& cmd, size_t expected_count) const;
    bool validateAircraftId(const std::string& id) const;
    bool validateAltitude(double altitude) const;
    bool validateSpeed(double speed) const;
    bool validateHeading(double heading) const;

    std::unordered_map<std::string, CommandDefinition> command_definitions_;

    // Constants
    static const int MIN_AIRCRAFT_ID_LENGTH = 3;
    static const int MAX_AIRCRAFT_ID_LENGTH = 10;
    static const char COMMENT_CHAR = '#';
    static const char PARAMETER_SEPARATOR = ' ';

    // Error messages
    static const std::string ERR_INVALID_COMMAND;
    static const std::string ERR_UNKNOWN_COMMAND;
    static const std::string ERR_INVALID_PARAMETERS;
    static const std::string ERR_INVALID_AIRCRAFT_ID;
    static const std::string ERR_INVALID_VALUE;
};

} // namespace atc

#endif // ATC_COMMAND_PROCESSOR_H
