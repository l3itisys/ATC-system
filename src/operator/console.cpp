#include "operator/console.h"
#include "common/logger.h"
#include "common/constants.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <csignal>
#include <algorithm>
#include <ctime>

namespace atc {

const std::string OperatorConsole::PROMPT = "> ";

class TerminalSettings {
public:
    TerminalSettings() {
        // Save current terminal settings
        tcgetattr(STDIN_FILENO, &original_settings_);

        // Configure terminal for immediate input without echo
        termios new_settings = original_settings_;
        new_settings.c_lflag &= ~(ICANON | ECHO);
        new_settings.c_cc[VMIN] = 1;
        new_settings.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &new_settings);
    }

    ~TerminalSettings() {
        // Restore original terminal settings
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
    , history_index_(0)
    , performance_() {

    if (!channel_) {
        throw std::runtime_error("Invalid communication channel");
    }

    // Register message handlers
    registerMessageHandlers();

    // Start input processing
    startInputThread();
    operational_ = true;

    // Display welcome message
    displayWelcomeMessage();
    Logger::getInstance().log("Operator console initialized");
}

OperatorConsole::~OperatorConsole() {
    stopInputThread();
}

void OperatorConsole::registerMessageHandlers() {
    // Since 'registerHandler' is not available, we will handle messages in the 'execute' method
}

void OperatorConsole::startInputThread() {
    input_running_ = true;
    input_thread_ = std::thread(&OperatorConsole::inputThreadFunction, this);

    // Set thread priority
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
    TerminalSettings terminal_settings;
    std::string input_buffer;
    displayPrompt();

    while (input_running_) {
        char c;
        if (read(STDIN_FILENO, &c, 1) == 1) {
            handleKeypress(c, input_buffer);
        }
    }
}

void OperatorConsole::handleKeypress(char c, std::string& buffer) {
    switch (c) {
        case '\n':  // Enter key
            if (!buffer.empty()) {
                std::cout << '\n';
                enqueueCommand(buffer);
                addToHistory(buffer);
                buffer.clear();
            }
            displayPrompt();
            break;

        case '\x7f':  // Backspace
            if (!buffer.empty()) {
                buffer.pop_back();
                std::cout << "\b \b";
            }
            break;

        case '\x1b':  // Escape sequence
            handleEscapeSequence(buffer);
            break;

        default:
            if (std::isprint(c)) {
                buffer += c;
                if (echo_enabled_) {
                    std::cout << c;
                }
            }
            break;
    }
    std::cout.flush();
}

void OperatorConsole::handleEscapeSequence(std::string& buffer) {
    char seq[2];
    if (read(STDIN_FILENO, seq, 2) != 2) return;

    if (seq[0] == '[') {
        switch (seq[1]) {
            case 'A':  // Up arrow
                if (!command_history_.empty()) {
                    clearInputLine();
                    buffer = getPreviousCommand();
                    std::cout << getPrompt() << buffer;
                }
                break;

            case 'B':  // Down arrow
                clearInputLine();
                buffer = getNextCommand();
                std::cout << getPrompt() << buffer;
                break;

            case 'C':  // Right arrow
                // Cursor movement could be implemented here
                break;

            case 'D':  // Left arrow
                // Cursor movement could be implemented here
                break;
        }
    }
}

void OperatorConsole::execute() {
    processMessages();
    processCommandQueue();
}

void OperatorConsole::processMessages() {
    comm::Message msg;
    while (channel_->receiveMessage(msg, 0)) {  // Non-blocking receive
        try {
            switch (msg.type) {
                case comm::MessageType::STATUS_RESPONSE:
                    handleStatusResponse(msg);
                    break;

                case comm::MessageType::ALERT:
                    handleAlert(msg);
                    break;

                default:
                    Logger::getInstance().log("Received unknown message type from " +
                                               msg.sender_id);
                    break;
            }
        } catch (const std::exception& e) {
            Logger::getInstance().log("Error processing message: " + std::string(e.what()));
        }
    }
}

void OperatorConsole::handleStatusResponse(const comm::Message& msg) {
    try {
        const auto& status = msg.payload.status_response; // Adjusted access
        std::cout << "\n" << status.status_text << "\n";
        displayPrompt();
    } catch (const std::exception& e) {
        Logger::getInstance().log("Error handling status response: " + std::string(e.what()));
    }
}

void OperatorConsole::handleAlert(const comm::Message& msg) {
    try {
        const auto& alert = msg.payload.alert_data; // Adjusted access

        // Clear current line
        clearInputLine();

        // Display alert with appropriate color based on level
        std::string color;
        switch (alert.level) {
            case comm::alerts::LEVEL_EMERGENCY:
                color = "\033[1;31m";  // Bright red
                break;
            case comm::alerts::LEVEL_CRITICAL:
                color = "\033[31m";    // Red
                break;
            case comm::alerts::LEVEL_WARNING:
                color = "\033[33m";    // Yellow
                break;
            default:
                color = "\033[36m";    // Cyan
        }

        std::cout << "\n" << color << "ALERT: " << alert.description << "\033[0m\n";
        displayPrompt();
    } catch (const std::exception& e) {
        Logger::getInstance().log("Error handling alert: " + std::string(e.what()));
    }
}

void OperatorConsole::processCommandQueue() {
    while (hasCommands()) {
        auto command = dequeueCommand();
        if (!command.empty()) {
            processCommand(command);
        }
    }
}

void OperatorConsole::processCommand(const std::string& command) {
    auto start_time = std::chrono::steady_clock::now();

    try {
        auto result = command_processor_->processCommand(command);
        if (result.success) {
            if (result.has_message) {
                channel_->sendMessage(result.msg);
            }
            if (!result.message.empty()) {
                std::cout << result.message << "\n";
            }
            if (command == "EXIT" || command == "exit") {
                // Implement logic to stop the console
                operational_ = false;
                stopInputThread();
                return;
            }
        } else {
            displayError(result.message);
        }

        processed_commands_++;
        updatePerformanceMetrics(start_time);
    }
    catch (const std::exception& e) {
        displayError("Command processing error: " + std::string(e.what()));
    }

    if (operational_) {
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
    std::lock_guard<std::mutex> lock(history_mutex_);
    if (command_history_.size() >= MAX_HISTORY_SIZE) {
        command_history_.erase(command_history_.begin());
    }
    command_history_.push_back(command);
    history_index_ = command_history_.size();
}

std::string OperatorConsole::getPreviousCommand() {
    std::lock_guard<std::mutex> lock(history_mutex_);
    if (command_history_.empty() || history_index_ == 0) {
        return "";
    }
    history_index_ = std::max(size_t(0), history_index_ - 1);
    return command_history_[history_index_];
}

std::string OperatorConsole::getNextCommand() {
    std::lock_guard<std::mutex> lock(history_mutex_);
    if (command_history_.empty() || history_index_ >= command_history_.size()) {
        return "";
    }
    history_index_++;
    return history_index_ < command_history_.size() ?
           command_history_[history_index_] : "";
}

void OperatorConsole::updatePerformanceMetrics(
    const std::chrono::steady_clock::time_point& start_time) {

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    performance_.average_processing_time_ms =
        (performance_.average_processing_time_ms * performance_.command_count + duration) /
        (performance_.command_count + 1);

    performance_.command_count++;
    performance_.last_command_time = std::chrono::system_clock::now(); // Use system_clock

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
        << performance_.average_processing_time_ms << "ms\n"
        << "  System Uptime: " << getSystemUptime() << "s";
    Logger::getInstance().log(oss.str());
}

void OperatorConsole::displayWelcomeMessage() const {
    clearScreen();
    std::cout << "\n=== Air Traffic Control System Console ===\n"
              << "Version: " << constants::SYSTEM_VERSION << "\n"
              << "Type 'HELP' for available commands\n"
              << "Type 'EXIT' to quit\n"
              << std::string(50, '=') << "\n\n";
}

void OperatorConsole::displayPrompt() const {
    std::cout << getPrompt() << std::flush;
}

void OperatorConsole::displayError(const std::string& error) const {
    std::cout << "\033[31mError: " << error << "\033[0m\n";
}

void OperatorConsole::clearScreen() const {
    std::cout << "\033[2J\033[H" << std::flush;  // ANSI escape codes
}

void OperatorConsole::clearInputLine() const {
    std::cout << "\r" << std::string(getPrompt().length() + MAX_COMMAND_LENGTH, ' ') << "\r";
}

void OperatorConsole::displayStatus() const {
    clearScreen();

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    std::cout << "\n=== ATC System Status ===\n"
              << "Time: " << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S") << "\n"
              << "System Uptime: " << getSystemUptime() << " seconds\n"
              << "Commands Processed: " << processed_commands_ << "\n"
              << "Current Queue Size: " << getCommandQueueSize() << "\n"
              << "Average Processing Time: " << std::fixed << std::setprecision(2)
              << performance_.average_processing_time_ms << "ms\n\n"
              << "Command History Size: " << command_history_.size() << "\n"
              << "Last Command Time: ";

    if (performance_.last_command_time.time_since_epoch().count() > 0) {
        auto last_cmd_time = performance_.last_command_time;
        auto last_cmd_c_time = std::chrono::system_clock::to_time_t(last_cmd_time);
        std::cout << std::put_time(std::localtime(&last_cmd_c_time), "%H:%M:%S");
    } else {
        std::cout << "None";
    }

    std::cout << "\n\nType 'HELP' for available commands\n"
              << std::string(50, '=') << "\n";
}

void OperatorConsole::setEchoEnabled(bool enable) {
    echo_enabled_ = enable;
    Logger::getInstance().log(std::string("Command echo ") +
                              (enable ? "enabled" : "disabled"));
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

const std::string& OperatorConsole::getLastError() const {
    return last_error_;
}

void OperatorConsole::setLastError(const std::string& error) {
    last_error_ = error;
    Logger::getInstance().log("Error: " + error);
}

void OperatorConsole::registerCustomHandler(const std::string& command,
                                            CommandHandler handler) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    custom_handlers_[command] = handler;
    Logger::getInstance().log("Registered custom handler for command: " + command);
}

void OperatorConsole::unregisterCustomHandler(const std::string& command) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    custom_handlers_.erase(command);
    Logger::getInstance().log("Unregistered custom handler for command: " + command);
}

std::vector<std::string> OperatorConsole::getCommandHistory() const {
    std::lock_guard<std::mutex> lock(history_mutex_);
    return command_history_;
}

void OperatorConsole::clearCommandHistory() {
    std::lock_guard<std::mutex> lock(history_mutex_);
    command_history_.clear();
    history_index_ = 0;
    Logger::getInstance().log("Command history cleared");
}

OperatorConsole::Performance OperatorConsole::getPerformanceMetrics() const {
    return performance_;
}

void OperatorConsole::resetPerformanceMetrics() {
    performance_ = Performance();
    Logger::getInstance().log("Performance metrics reset");
}

std::string OperatorConsole::formatTimestamp(
    const std::chrono::system_clock::time_point& time_point) const {

    auto time = std::chrono::system_clock::to_time_t(time_point);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string OperatorConsole::getPrompt() const {
    return PROMPT;
}

} // namespace atc

