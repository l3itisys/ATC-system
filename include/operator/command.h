#ifndef ATC_COMMAND_PROCESSOR_H
#define ATC_COMMAND_PROCESSOR_H

#include "communication/message_types.h"
#include "common/types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <optional>
#include <memory>

namespace atc {

class CommandProcessor {
public:
    CommandProcessor();
    ~CommandProcessor() = default;

    // Command processing
    struct CommandResult {
        bool success;
        std::string message;
        std::optional<comm::Message> response;

        CommandResult(bool s = false, const std::string& msg = "",
                     std::optional<comm::Message> resp = std::nullopt)
            : success(s), message(msg), response(resp) {}
    };

    // Command validation and processing
    CommandResult processCommand(const std::string& command_line);
    bool validateCommand(const std::string& command_line, std::string& error_message) const;

    // Command information
    struct CommandInfo {
        std::string syntax;
        std::string description;
        std::vector<std::string> examples;
    };

    // Help information
    std::string getHelpText() const;
    std::string getCommandHelp(const std::string& command) const;

private:
    // Command parsing
    struct ParsedCommand {
        std::string command;
        std::string aircraft_id;
        std::vector<std::string> parameters;
    };

    // Command definition
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

    // Validation helpers
    bool validateAltitude(double altitude) const;
    bool validateSpeed(double speed) const;
    bool validateHeading(double heading) const;
    bool validateAircraftId(const std::string& id) const;
    bool validateParameters(const ParsedCommand& cmd, size_t expected_count) const;

    // Parsing helpers
    ParsedCommand parseCommandLine(const std::string& command_line) const;
    std::vector<std::string> tokenizeCommand(const std::string& command_line) const;

    // Initialize commands
    void initializeCommandDefinitions();

    // Command registry
    std::unordered_map<std::string, CommandDefinition> command_definitions_;

    // Constants
    static constexpr int MIN_AIRCRAFT_ID_LENGTH = 3;
    static constexpr int MAX_AIRCRAFT_ID_LENGTH = 10;
    static constexpr char COMMENT_CHAR = '#';
    static constexpr char PARAMETER_SEPARATOR = ' ';

    // Error messages
    static const inline std::string ERR_INVALID_COMMAND = "Invalid command format";
    static const inline std::string ERR_UNKNOWN_COMMAND = "Unknown command";
    static const inline std::string ERR_INVALID_PARAMETERS = "Invalid parameter count";
    static const inline std::string ERR_INVALID_AIRCRAFT_ID = "Invalid aircraft identifier";
    static const inline std::string ERR_INVALID_VALUE = "Invalid value";
};

}

#endif // ATC_COMMAND_PROCESSOR_H
