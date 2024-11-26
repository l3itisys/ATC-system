#include "operator/console.h"
#include "common/logger.h"
#include "common/constants.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

namespace atc {

const std::string HELP_TEXT = R"(
Available Air Traffic Control Commands:
----------------------------------------
SPEED <id> <value>  - Change aircraft speed (150-500 units)
ALT <id> <value>    - Change aircraft altitude (15000-25000 feet)
HDG <id> <value>    - Change aircraft heading (0-359 degrees)
STATUS             - Display system status
TRACK <id>         - Focus on specific aircraft
PAUSE              - Pause display updates
RESUME             - Resume display updates
DISPLAY <rate>     - Set display refresh rate (2-30 seconds)
CLEAR              - Clear screen
HELP               - Show this help message
EXIT               - Exit system

Example: ALT AC001 20000
)";

// Terminal settings helper class
class TerminalSettings {
public:
    TerminalSettings() {
        tcgetattr(STDIN_FILENO, &original_settings_);
        termios new_settings = original_settings_;
        new_settings.c_lflag &= ~(ICANON | ECHO);
        new_settings.c_cc[VMIN] = 1;
        new_settings.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &new_settings);
    }

    ~TerminalSettings() {
        tcsetattr(STDIN_FILENO, TCSANOW, &original_settings_);
    }

private:
    termios original_settings_;
};

OperatorConsole::OperatorConsole(std::shared_ptr<comm::QnxChannel> channel)
    : PeriodicTask(std::chrono::milliseconds(100), constants::OPERATOR_PRIORITY)
    , channel_(channel)
    , command_processor_(std::make_unique<CommandProcessor>())
    , input_running_(false)
    , operational_(false)
    , processed_commands_(0)
    , echo_enabled_(true)
    , history_index_(0) {

    if (!channel_) {
        throw std::runtime_error("Invalid communication channel");
    }

    startInputThread();
    operational_ = true;
    displayWelcomeMessage();
    Logger::getInstance().log("Operator console initialized");
}

OperatorConsole::~OperatorConsole() {
    stopInputThread();
}

void OperatorConsole::startInputThread() {
    input_running_ = true;
    input_thread_ = std::thread(&OperatorConsole::inputThreadFunction, this);

    struct sched_param param;
    param.sched_priority = constants::OPERATOR_PRIORITY;
    pthread_setschedparam(input_thread_.native_handle(), SCHED_RR, &param);
}

void OperatorConsole::stopInputThread() {
    input_running_ = false;
    if (input_thread_.joinable()) {
        input_thread_.join();
    }
}

void OperatorConsole::inputThreadFunction() {
    std::string input_buffer;
    displayPrompt();

    while (input_running_) {
        std::string line;
        if (std::getline(std::cin, line)) {
            if (!line.empty()) {
                enqueueCommand(line);
                addToHistory(line);
            }
            displayPrompt();
        }
    }
}

void OperatorConsole::execute() {
    while (hasCommands()) {
        processNextCommand();
    }
}

void OperatorConsole::processNextCommand() {
    std::string command = dequeueCommand();
    if (command.empty()) return;

    auto start_time = std::chrono::steady_clock::now();

    try {
        std::istringstream iss(command);
        std::vector<std::string> tokens;
        std::string token;
        while (iss >> token) {
            tokens.push_back(token);
        }

        if (tokens.empty()) return;

        std::string cmd = tokens[0];
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

        // Handle basic commands
        if (cmd == "HELP") {
            std::cout << HELP_TEXT << std::endl;
            return;
        }
        else if (cmd == "CLEAR") {
            clearScreen();
            displayWelcomeMessage();
            return;
        }
        else if (cmd == "EXIT") {
            operational_ = false;
            std::cout << "Shutting down ATC system..." << std::endl;
            return;
        }
        else if (cmd == "STATUS") {
            displayStatus();
            return;
        }
        else if (cmd == "PAUSE") {
            if (channel_) {
                comm::CommandData cmd_data("SYSTEM", "PAUSE");
                channel_->sendMessage(comm::Message::createCommand("OPERATOR", cmd_data));
                std::cout << "Display updates paused. Type 'RESUME' to continue." << std::endl;
            }
            return;
        }
        else if (cmd == "RESUME") {
            if (channel_) {
                comm::CommandData cmd_data("SYSTEM", "RESUME");
                channel_->sendMessage(comm::Message::createCommand("OPERATOR", cmd_data));
                std::cout << "Display updates resumed." << std::endl;
            }
            return;
        }
        else if (cmd == "DISPLAY" && tokens.size() > 1) {
            try {
                int rate = std::stoi(tokens[1]);
                if (rate >= 2 && rate <= 30) {
                    if (channel_) {
                        comm::CommandData cmd_data("SYSTEM", "DISPLAY_RATE");
                        cmd_data.params.push_back(tokens[1]);
                        channel_->sendMessage(comm::Message::createCommand("OPERATOR", cmd_data));
                        std::cout << "Display refresh rate set to " << rate << " seconds." << std::endl;
                    }
                } else {
                    std::cout << "Error: Display rate must be between 2 and 30 seconds." << std::endl;
                }
            } catch (...) {
                std::cout << "Error: Invalid display rate value." << std::endl;
            }
            return;
        }

        // Process command using command processor
        auto result = command_processor_->processCommand(command);
        if (result.success) {
            if (result.response) {
                channel_->sendMessage(*result.response);
            }
            if (!result.message.empty()) {
                std::cout << result.message << std::endl;
            }
        } else {
            displayError(result.message);
        }

        processed_commands_++;
        updatePerformanceMetrics(start_time);
    }
    catch (const std::exception& e) {
        displayError("Command processing error: " + std::string(e.what()));
        Logger::getInstance().log("Command processing error: " + std::string(e.what()));
    }

    if (echo_enabled_) {
        displayPrompt();
    }
}

void OperatorConsole::enqueueCommand(const std::string& command) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (command_queue_.size() < MAX_QUEUE_SIZE) {
        command_queue_.push(command);
        queue_cv_.notify_one();
    } else {
        displayError("Command queue full, command discarded");
    }
}

std::string OperatorConsole::dequeueCommand() {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (command_queue_.empty()) {
        return "";
    }
    std::string command = command_queue_.front();
    command_queue_.pop();
    return command;
}

void OperatorConsole::addToHistory(const std::string& command) {
    if (command_history_.size() >= MAX_HISTORY_SIZE) {
        command_history_.erase(command_history_.begin());
    }
    command_history_.push_back(command);
    history_index_ = command_history_.size();
}

std::string OperatorConsole::getPreviousCommand() {
    if (command_history_.empty() || history_index_ == 0) {
        return "";
    }
    history_index_ = std::max(size_t(0), history_index_ - 1);
    return command_history_[history_index_];
}

std::string OperatorConsole::getNextCommand() {
    if (command_history_.empty() || history_index_ >= command_history_.size()) {
        history_index_ = command_history_.size();
        return "";
    }
    history_index_++;
    return history_index_ < command_history_.size() ? command_history_[history_index_] : "";
}

void OperatorConsole::updatePerformanceMetrics(const std::chrono::steady_clock::time_point& start_time) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    performance_.average_processing_time_ms =
        (performance_.average_processing_time_ms * performance_.command_count + duration) /
        (performance_.command_count + 1);
    performance_.command_count++;
    performance_.last_command_time = end_time;

    if (shouldLogPerformance()) {
        logPerformanceStats();
    }
}

bool OperatorConsole::shouldLogPerformance() const {
    return performance_.command_count % 100 == 0;
}

void OperatorConsole::logPerformanceStats() const {
    std::ostringstream oss;
    oss << "Operator Console Performance:\n"
        << "  Commands Processed: " << performance_.command_count << "\n"
        << "  Average Processing Time: " << std::fixed << std::setprecision(2)
        << performance_.average_processing_time_ms << "ms";
    Logger::getInstance().log(oss.str());
}

void OperatorConsole::setEchoEnabled(bool enable) {
    echo_enabled_ = enable;
}

void OperatorConsole::clearScreen() const {
    std::cout << "\033[2J\033[H" << std::flush;
}

void OperatorConsole::displayPrompt() const {
    std::cout << "\r" << PROMPT << std::flush;
}

void OperatorConsole::displayWelcomeMessage() const {
    std::cout << WELCOME_MESSAGE << std::endl;
    displayPrompt();
}

void OperatorConsole::displayError(const std::string& error) const {
    std::cout << "\033[31mError: " << error << "\033[0m" << std::endl;
}

void OperatorConsole::clearInputLine() const {
    std::cout << "\r" << std::string(PROMPT.length() + MAX_COMMAND_LENGTH, ' ') << "\r";
}

void OperatorConsole::displayStatus() const {
    clearScreen();
    std::cout << "\n=== ATC System Status ===\n"
              << "Active Aircraft: " << getActiveAircraftCount() << "\n"
              << "Commands Processed: " << processed_commands_ << "\n"
              << "Queue Size: " << getCommandQueueSize() << "\n"
              << "System Uptime: " << getSystemUptime() << " seconds\n"
              << "Average Command Processing Time: " << std::fixed << std::setprecision(2)
              << performance_.average_processing_time_ms << "ms\n"
              << "\nType 'HELP' for available commands\n"
              << std::string(50, '-') << "\n";
    displayPrompt();
}

bool OperatorConsole::hasCommands() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return !command_queue_.empty();
}

size_t OperatorConsole::getCommandQueueSize() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return command_queue_.size();
}

long OperatorConsole::getSystemUptime() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(
        now - system_start_time_).count();
}

int OperatorConsole::getActiveAircraftCount() const {
    // This should be implemented to return the actual count from your system
    return 4; // Temporary hardcoded value
}

} // namespace atc
