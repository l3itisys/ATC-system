#include "operator/command.h"
#include "common/logger.h"
#include "common/constants.h"
#include "communication/message_types.h"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iomanip>

namespace atc {

// Static member initialization
const std::string CommandProcessor::ERR_INVALID_COMMAND = "Invalid command format";
const std::string CommandProcessor::ERR_UNKNOWN_COMMAND = "Unknown command";
const std::string CommandProcessor::ERR_INVALID_PARAMETERS = "Invalid parameter count";
const std::string CommandProcessor::ERR_INVALID_AIRCRAFT_ID = "Invalid aircraft identifier";
const std::string CommandProcessor::ERR_INVALID_VALUE = "Invalid value";

using CommandHandlerFunc = std::function<CommandProcessor::CommandResult(
    CommandProcessor*, const CommandProcessor::ParsedCommand&)>;

CommandProcessor::CommandProcessor() {
    initializeCommandDefinitions();
}

CommandProcessor::~CommandProcessor() {
    // Destructor implementation (if any cleanup is needed)
}

void CommandProcessor::initializeCommandDefinitions() {
    command_definitions_.clear();

    // Helper function to create command handler
    auto createHandler = [](CommandProcessor* processor,
                            CommandResult (CommandProcessor::*handler)(const ParsedCommand&)) {
        return [processor, handler](CommandProcessor*, const ParsedCommand& cmd) {
            return (processor->*handler)(cmd);
        };
    };

    command_definitions_["ALTITUDE"] = CommandDefinition{
        createHandler(this, &CommandProcessor::handleAltitudeCommand),
        {"ALTITUDE <aircraft_id> <value>", "Change aircraft altitude (15000-25000 feet)",
         {"ALTITUDE AC001 20000"}},
        2, 2
    };

    command_definitions_["SPEED"] = CommandDefinition{
        createHandler(this, &CommandProcessor::handleSpeedCommand),
        {"SPEED <aircraft_id> <value>", "Change aircraft speed (150-500 units)",
         {"SPEED AC001 300"}},
        2, 2
    };

    command_definitions_["HEADING"] = CommandDefinition{
        createHandler(this, &CommandProcessor::handleHeadingCommand),
        {"HEADING <aircraft_id> <value>", "Change aircraft heading (0-359 degrees)",
         {"HEADING AC001 90"}},
        2, 2
    };

    command_definitions_["EMERGENCY"] = CommandDefinition{
        createHandler(this, &CommandProcessor::handleEmergencyCommand),
        {"EMERGENCY <aircraft_id> [ON|OFF]", "Set emergency state",
         {"EMERGENCY AC001 ON"}},
        2, 2
    };

    command_definitions_["STATUS"] = CommandDefinition{
        createHandler(this, &CommandProcessor::handleStatusCommand),
        {"STATUS [aircraft_id]", "Display status",
         {"STATUS", "STATUS AC001"}},
        0, 1
    };

    command_definitions_["HELP"] = CommandDefinition{
        createHandler(this, &CommandProcessor::handleHelpCommand),
        {"HELP [command]", "Show help information",
         {"HELP", "HELP ALTITUDE"}},
        0, 1
    };

    command_definitions_["TRACK"] = CommandDefinition{
        createHandler(this, &CommandProcessor::handleTrackCommand),
        {"TRACK <aircraft_id>", "Track specific aircraft",
         {"TRACK AC001", "TRACK NONE"}},
        1, 1
    };
}

CommandProcessor::CommandResult
CommandProcessor::processCommand(const std::string& command_line) {
    try {
        ParsedCommand cmd = parseCommandLine(command_line);
        if (cmd.command.empty()) {
            return CommandResult(false, ERR_INVALID_COMMAND);
        }

        std::transform(cmd.command.begin(), cmd.command.end(),
                       cmd.command.begin(), ::toupper);

        auto it = command_definitions_.find(cmd.command);
        if (it == command_definitions_.end()) {
            return CommandResult(false, ERR_UNKNOWN_COMMAND);
        }

        const auto& def = it->second;
        if (cmd.parameters.size() < def.min_params ||
            cmd.parameters.size() > def.max_params) {
            return CommandResult(false, ERR_INVALID_PARAMETERS);
        }

        return def.handler(this, cmd);

    } catch (const std::exception& e) {
        return CommandResult(false, "Error processing command: " + std::string(e.what()));
    }
}

CommandProcessor::CommandResult
CommandProcessor::handleAltitudeCommand(const ParsedCommand& cmd) {
    if (!validateAircraftId(cmd.aircraft_id)) {
        return CommandResult(false, ERR_INVALID_AIRCRAFT_ID);
    }

    try {
        double altitude = std::stod(cmd.parameters[0]);
        if (!validateAltitude(altitude)) {
            return CommandResult(false, "Invalid altitude value");
        }

        comm::CommandData cmd_data(cmd.aircraft_id, comm::commands::CMD_ALTITUDE);
        cmd_data.params.push_back(std::to_string(altitude));

        auto msg = comm::Message::createCommand("OPERATOR", cmd_data);
        CommandResult result(true, "Altitude change command sent", msg);
        result.has_message = true;
        return result;

    } catch (const std::exception&) {
        return CommandResult(false, "Invalid altitude value");
    }
}

CommandProcessor::CommandResult
CommandProcessor::handleSpeedCommand(const ParsedCommand& cmd) {
    if (!validateAircraftId(cmd.aircraft_id)) {
        return CommandResult(false, ERR_INVALID_AIRCRAFT_ID);
    }

    try {
        double speed = std::stod(cmd.parameters[0]);
        if (!validateSpeed(speed)) {
            return CommandResult(false, "Invalid speed value");
        }

        comm::CommandData cmd_data(cmd.aircraft_id, comm::commands::CMD_SPEED);
        cmd_data.params.push_back(std::to_string(speed));

        auto msg = comm::Message::createCommand("OPERATOR", cmd_data);
        CommandResult result(true, "Speed change command sent", msg);
        result.has_message = true;
        return result;

    } catch (const std::exception&) {
        return CommandResult(false, "Invalid speed value");
    }
}

CommandProcessor::CommandResult
CommandProcessor::handleHeadingCommand(const ParsedCommand& cmd) {
    if (!validateAircraftId(cmd.aircraft_id)) {
        return CommandResult(false, ERR_INVALID_AIRCRAFT_ID);
    }

    try {
        double heading = std::stod(cmd.parameters[0]);
        if (!validateHeading(heading)) {
            return CommandResult(false, "Invalid heading value");
        }

        comm::CommandData cmd_data(cmd.aircraft_id, comm::commands::CMD_HEADING);
        cmd_data.params.push_back(std::to_string(heading));

        auto msg = comm::Message::createCommand("OPERATOR", cmd_data);
        CommandResult result(true, "Heading change command sent", msg);
        result.has_message = true;
        return result;

    } catch (const std::exception&) {
        return CommandResult(false, "Invalid heading value");
    }
}

CommandProcessor::CommandResult
CommandProcessor::handleEmergencyCommand(const ParsedCommand& cmd) {
    if (!validateAircraftId(cmd.aircraft_id)) {
        return CommandResult(false, ERR_INVALID_AIRCRAFT_ID);
    }

    std::string state = cmd.parameters[0];
    std::transform(state.begin(), state.end(), state.begin(), ::toupper);

    if (state != "ON" && state != "OFF") {
        return CommandResult(false, "Emergency state must be ON or OFF");
    }

    comm::CommandData cmd_data(cmd.aircraft_id, comm::commands::CMD_EMERGENCY);
    cmd_data.params.push_back(state == "ON" ? "1" : "0");

    std::string message = "Emergency " + std::string(state == "ON" ? "declared" : "cancelled") +
                          " for " + cmd.aircraft_id;

    auto msg = comm::Message::createCommand("OPERATOR", cmd_data);
    CommandResult result(true, message, msg);
    result.has_message = true;
    return result;
}

CommandProcessor::CommandResult
CommandProcessor::handleStatusCommand(const ParsedCommand& cmd) {
    comm::CommandData cmd_data;

    if (cmd.parameters.empty()) {
        cmd_data = comm::CommandData("SYSTEM", comm::commands::CMD_STATUS);
        auto msg = comm::Message::createCommand("OPERATOR", cmd_data);
        CommandResult result(true, "System status requested", msg);
        result.has_message = true;
        return result;
    }

    if (!validateAircraftId(cmd.parameters[0])) {
        return CommandResult(false, ERR_INVALID_AIRCRAFT_ID);
    }

    cmd_data = comm::CommandData(cmd.parameters[0], comm::commands::CMD_STATUS);
    auto msg = comm::Message::createCommand("OPERATOR", cmd_data);
    CommandResult result(true, "Aircraft status requested", msg);
    result.has_message = true;
    return result;
}

CommandProcessor::CommandResult
CommandProcessor::handleHelpCommand(const ParsedCommand& cmd) {
    if (cmd.parameters.empty()) {
        return CommandResult(true, getHelpText());
    }

    std::string command = cmd.parameters[0];
    std::transform(command.begin(), command.end(), command.begin(), ::toupper);
    return CommandResult(true, getCommandHelp(command));
}

CommandProcessor::CommandResult
CommandProcessor::handleTrackCommand(const ParsedCommand& cmd) {
    std::string target = cmd.parameters[0];
    if (target == "NONE") {
        comm::CommandData cmd_data("DISPLAY", "TRACK_CLEAR");
        auto msg = comm::Message::createCommand("OPERATOR", cmd_data);
        CommandResult result(true, "Tracking cleared", msg);
        result.has_message = true;
        return result;
    }

    if (!validateAircraftId(target)) {
        return CommandResult(false, ERR_INVALID_AIRCRAFT_ID);
    }

    comm::CommandData cmd_data("DISPLAY", "TRACK");
    cmd_data.params.push_back(target);
    auto msg = comm::Message::createCommand("OPERATOR", cmd_data);
    CommandResult result(true, "Now tracking " + target, msg);
    result.has_message = true;
    return result;
}

CommandProcessor::ParsedCommand
CommandProcessor::parseCommandLine(const std::string& command_line) const {
    ParsedCommand result;
    std::istringstream iss(command_line);
    std::string token;

    if (!(iss >> token)) {
        return result;
    }
    result.command = token;

    if (iss >> token) {
        result.aircraft_id = token;
    }

    while (iss >> token) {
        result.parameters.push_back(token);
    }

    return result;
}

bool CommandProcessor::validateAircraftId(const std::string& id) const {
    if (id.length() < MIN_AIRCRAFT_ID_LENGTH ||
        id.length() > MAX_AIRCRAFT_ID_LENGTH) {
        return false;
    }

    return std::all_of(id.begin(), id.end(),
                       [](char c) { return std::isalnum(c); });
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

std::string CommandProcessor::getHelpText() const {
    std::ostringstream oss;
    oss << "\nAvailable Commands:\n"
        << "==================\n";

    for (const auto& pair : command_definitions_) {
        oss << std::left << std::setw(15) << pair.first << " - "
            << pair.second.info.description << "\n";
    }

    oss << "\nUse 'HELP <command>' for detailed information.\n";
    return oss.str();
}

std::string CommandProcessor::getCommandHelp(const std::string& command) const {
    auto it = command_definitions_.find(command);
    if (it == command_definitions_.end()) {
        return "Unknown command: " + command;
    }

    const auto& info = it->second.info;
    std::ostringstream oss;
    oss << "\nCommand: " << command << "\n"
        << "Syntax: " << info.syntax << "\n"
        << "Description: " << info.description << "\n"
        << "Examples:\n";

    for (const auto& example : info.examples) {
        oss << "  " << example << "\n";
    }

    return oss.str();
}

} // namespace atc

