#include "operator/command.h"
#include "common/constants.h"
#include "common/logger.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <cmath>

namespace atc {

CommandProcessor::CommandProcessor() {
    initializeCommandDefinitions();
}

void CommandProcessor::initializeCommandDefinitions() {
    // Clear any existing definitions
    command_definitions_.clear();

    using HandlerFunc = std::function<CommandResult(CommandProcessor*, const ParsedCommand&)>;

    // Helper to create a command definition
    auto createDef = [](HandlerFunc handler,
                       std::string syntax,
                       std::string desc,
                       std::vector<std::string> examples,
                       size_t min_params,
                       size_t max_params) {
        return CommandDefinition{
            std::move(handler),
            CommandInfo{std::move(syntax), std::move(desc), std::move(examples)},
            min_params,
            max_params
        };
    };

    // Add altitude command
    command_definitions_["ALT"] = createDef(
        [](CommandProcessor* self, const ParsedCommand& cmd) { return self->handleAltitudeCommand(cmd); },
        "ALT <aircraft_id> <altitude>",
        "Change aircraft altitude (feet)",
        {"ALT AC001 20000", "ALT AC002 22000"},
        1, 1
    );

    // Add speed command
    command_definitions_["SPD"] = createDef(
        [](CommandProcessor* self, const ParsedCommand& cmd) { return self->handleSpeedCommand(cmd); },
        "SPD <aircraft_id> <speed>",
        "Change aircraft speed (knots)",
        {"SPD AC001 250", "SPD AC002 300"},
        1, 1
    );

    // Add heading command
    command_definitions_["HDG"] = createDef(
        [](CommandProcessor* self, const ParsedCommand& cmd) { return self->handleHeadingCommand(cmd); },
        "HDG <aircraft_id> <heading>",
        "Change aircraft heading (degrees)",
        {"HDG AC001 090", "HDG AC002 270"},
        1, 1
    );

    // Add emergency command
    command_definitions_["EMERG"] = createDef(
        [](CommandProcessor* self, const ParsedCommand& cmd) { return self->handleEmergencyCommand(cmd); },
        "EMERG <aircraft_id> <ON|OFF>",
        "Declare or cancel aircraft emergency",
        {"EMERG AC001 ON", "EMERG AC002 OFF"},
        1, 1
    );

    // Add status command
    command_definitions_["STATUS"] = createDef(
        [](CommandProcessor* self, const ParsedCommand& cmd) { return self->handleStatusCommand(cmd); },
        "STATUS [aircraft_id]",
        "Display system or aircraft status",
        {"STATUS", "STATUS AC001"},
        0, 1
    );

    // Add help command
    command_definitions_["HELP"] = createDef(
        [](CommandProcessor* self, const ParsedCommand& cmd) { return self->handleHelpCommand(cmd); },
        "HELP [command]",
        "Display help information",
        {"HELP", "HELP ALT"},
        0, 1
    );
}

CommandProcessor::CommandResult CommandProcessor::processCommand(const std::string& command_line) {
    std::string error_message;
    if (!validateCommand(command_line, error_message)) {
        return CommandResult{false, error_message, std::nullopt};
    }

    ParsedCommand parsed = parseCommandLine(command_line);
    auto it = command_definitions_.find(parsed.command);
    if (it == command_definitions_.end()) {
        return CommandResult{false, ERR_UNKNOWN_COMMAND, std::nullopt};
    }

    if (!validateParameters(parsed, it->second.min_params)) {
        return CommandResult{false, ERR_INVALID_PARAMETERS, std::nullopt};
    }

    return it->second.handler(this, parsed);
}

bool CommandProcessor::validateCommand(const std::string& command_line, std::string& error_message) const {
    if (command_line.empty()) {
        error_message = "Empty command";
        return false;
    }

    // Skip comments
    if (command_line[0] == COMMENT_CHAR) {
        error_message = "Comment line";
        return false;
    }

    // Basic format validation
    std::istringstream iss(command_line);
    std::string command;
    iss >> command;

    if (command.empty()) {
        error_message = ERR_INVALID_COMMAND;
        return false;
    }

    // Convert to uppercase for comparison
    std::transform(command.begin(), command.end(), command.begin(), ::toupper);

    // Check if command exists
    if (command_definitions_.find(command) == command_definitions_.end()) {
        error_message = ERR_UNKNOWN_COMMAND + ": " + command;
        return false;
    }

    return true;
}

CommandProcessor::ParsedCommand CommandProcessor::parseCommandLine(const std::string& command_line) const {
    ParsedCommand result;
    std::vector<std::string> tokens = tokenizeCommand(command_line);

    if (!tokens.empty()) {
        result.command = tokens[0];
        std::transform(result.command.begin(), result.command.end(),
                      result.command.begin(), ::toupper);

        if (tokens.size() > 1) {
            result.aircraft_id = tokens[1];
            result.parameters.insert(result.parameters.end(),
                                  tokens.begin() + 2, tokens.end());
        }
    }

    return result;
}

std::vector<std::string> CommandProcessor::tokenizeCommand(const std::string& command_line) const {
    std::vector<std::string> tokens;
    std::istringstream iss(command_line);
    std::string token;

    while (iss >> token) {
        tokens.push_back(token);
    }

    return tokens;
}

bool CommandProcessor::validateParameters(const ParsedCommand& cmd, size_t expected_count) const {
    return cmd.parameters.size() >= expected_count &&
           cmd.parameters.size() <= command_definitions_.at(cmd.command).max_params;
}

bool CommandProcessor::validateAircraftId(const std::string& id) const {
    if (id.length() < MIN_AIRCRAFT_ID_LENGTH || id.length() > MAX_AIRCRAFT_ID_LENGTH) {
        return false;
    }

    return std::all_of(id.begin(), id.end(), [](char c) {
        return std::isalnum(c);
    });
}

bool CommandProcessor::validateAltitude(double altitude) const {
    return altitude >= constants::AIRSPACE_Z_MIN &&
           altitude <= constants::AIRSPACE_Z_MAX;
}

bool CommandProcessor::validateSpeed(double speed) const {
    return speed >= constants::MIN_SPEED &&
           speed <= constants::MAX_SPEED;
}

bool CommandProcessor::validateHeading(double heading) const {
    return heading >= 0.0 && heading < 360.0;
}

CommandProcessor::CommandResult CommandProcessor::handleAltitudeCommand(const ParsedCommand& cmd) {
    if (!validateAircraftId(cmd.aircraft_id)) {
        return CommandResult{false, ERR_INVALID_AIRCRAFT_ID, std::nullopt};
    }

    try {
        double altitude = std::stod(cmd.parameters[0]);
        if (!validateAltitude(altitude)) {
            return CommandResult{false,
                "Invalid altitude: Must be between " +
                std::to_string(static_cast<int>(constants::AIRSPACE_Z_MIN)) +
                " and " +
                std::to_string(static_cast<int>(constants::AIRSPACE_Z_MAX)) +
                " feet",
                std::nullopt};
        }

        comm::CommandData cmd_data(cmd.aircraft_id, "ALTITUDE");
        cmd_data.params.push_back(cmd.parameters[0]);
        return CommandResult{true, "Altitude change command sent",
                comm::Message::createCommand("OPERATOR", cmd_data)};
    }
    catch (const std::exception&) {
        return CommandResult{false, ERR_INVALID_VALUE + ": " + cmd.parameters[0],
                std::nullopt};
    }
}

CommandProcessor::CommandResult CommandProcessor::handleSpeedCommand(const ParsedCommand& cmd) {
    if (!validateAircraftId(cmd.aircraft_id)) {
        return CommandResult{false, ERR_INVALID_AIRCRAFT_ID, std::nullopt};
    }

    try {
        double speed = std::stod(cmd.parameters[0]);
        if (!validateSpeed(speed)) {
            return CommandResult{false,
                "Invalid speed: Must be between " +
                std::to_string(static_cast<int>(constants::MIN_SPEED)) +
                " and " +
                std::to_string(static_cast<int>(constants::MAX_SPEED)) +
                " knots",
                std::nullopt};
        }

        comm::CommandData cmd_data(cmd.aircraft_id, "SPEED");
        cmd_data.params.push_back(cmd.parameters[0]);
        return CommandResult{true, "Speed change command sent",
                comm::Message::createCommand("OPERATOR", cmd_data)};
    }
    catch (const std::exception&) {
        return CommandResult{false, ERR_INVALID_VALUE + ": " + cmd.parameters[0],
                std::nullopt};
    }
}

CommandProcessor::CommandResult CommandProcessor::handleHeadingCommand(const ParsedCommand& cmd) {
    if (!validateAircraftId(cmd.aircraft_id)) {
        return CommandResult{false, ERR_INVALID_AIRCRAFT_ID, std::nullopt};
    }

    try {
        double heading = std::stod(cmd.parameters[0]);
        if (!validateHeading(heading)) {
            return CommandResult{false, "Invalid heading: Must be between 0 and 359 degrees",
                    std::nullopt};
        }

        comm::CommandData cmd_data(cmd.aircraft_id, "HEADING");
        cmd_data.params.push_back(cmd.parameters[0]);
        return CommandResult{true, "Heading change command sent",
                comm::Message::createCommand("OPERATOR", cmd_data)};
    }
    catch (const std::exception&) {
        return CommandResult{false, ERR_INVALID_VALUE + ": " + cmd.parameters[0],
                std::nullopt};
    }
}

CommandProcessor::CommandResult CommandProcessor::handleEmergencyCommand(const ParsedCommand& cmd) {
    if (!validateAircraftId(cmd.aircraft_id)) {
        return CommandResult{false, ERR_INVALID_AIRCRAFT_ID, std::nullopt};
    }

    std::string state = cmd.parameters[0];
    std::transform(state.begin(), state.end(), state.begin(), ::toupper);

    if (state != "ON" && state != "OFF") {
        return CommandResult{false, "Invalid emergency state: Must be ON or OFF",
                std::nullopt};
    }

    comm::CommandData cmd_data(cmd.aircraft_id, "EMERGENCY");
    cmd_data.params.push_back(state == "ON" ? "1" : "0");
    return CommandResult{true, "Emergency state command sent",
            comm::Message::createCommand("OPERATOR", cmd_data)};
}

CommandProcessor::CommandResult CommandProcessor::handleStatusCommand(const ParsedCommand& cmd) {
    if (!cmd.aircraft_id.empty() && !validateAircraftId(cmd.aircraft_id)) {
        return CommandResult{false, ERR_INVALID_AIRCRAFT_ID, std::nullopt};
    }

    comm::CommandData cmd_data(cmd.aircraft_id.empty() ? "SYSTEM" : cmd.aircraft_id,
                             "STATUS");
    return CommandResult{true, "Status request sent",
            comm::Message::createCommand("OPERATOR", cmd_data)};
}

CommandProcessor::CommandResult CommandProcessor::handleHelpCommand(const ParsedCommand& cmd) {
    if (cmd.parameters.empty()) {
        return CommandResult{true, getHelpText(), std::nullopt};
    }

    std::string command = cmd.parameters[0];
    std::transform(command.begin(), command.end(), command.begin(), ::toupper);
    auto it = command_definitions_.find(command);
    if (it == command_definitions_.end()) {
        return CommandResult{false, ERR_UNKNOWN_COMMAND + ": " + command, std::nullopt};
    }

    return CommandResult{true, getCommandHelp(command), std::nullopt};
}

std::string CommandProcessor::getHelpText() const {
    std::ostringstream oss;
    oss << "\nAvailable Commands:\n"
        << "==================\n";

    for (const auto& [cmd, def] : command_definitions_) {
        oss << std::left << std::setw(10) << cmd
            << def.info.description << "\n";
    }

    oss << "\nType 'HELP <command>' for detailed information about a command.";
    return oss.str();
}

std::string CommandProcessor::getCommandHelp(const std::string& command) const {
    auto it = command_definitions_.find(command);
    if (it == command_definitions_.end()) {
        return ERR_UNKNOWN_COMMAND;
    }

    const auto& info = it->second.info;
    std::ostringstream oss;
    oss << "\nCommand: " << command << "\n"
        << "Syntax: " << info.syntax << "\n"
        << "Description: " << info.description << "\n"
        << "Examples:";

    for (const auto& example : info.examples) {
        oss << "\n  " << example;
    }

    if (it->second.min_params == it->second.max_params) {
        oss << "\nParameters: Requires exactly " << it->second.min_params << " parameter(s)";
    } else {
        oss << "\nParameters: Requires " << it->second.min_params << " to "
            << it->second.max_params << " parameter(s)";
    }

    return oss.str();
}

}
