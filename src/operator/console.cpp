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

namespace {
    // Terminal handling helper class
    class TerminalSettings {
    public:
        TerminalSettings() {
            // Save current terminal settings
            tcgetattr(STDIN_FILENO, &original_settings_);

            // Configure new terminal settings
            termios new_settings = original_settings_;
            new_settings.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echo
            new_settings.c_cc[VMIN] = 1;  // Read minimum of 1 character
            new_settings.c_cc[VTIME] = 0; // No timeout
            tcsetattr(STDIN_FILENO, TCSANOW, &new_settings);
        }

        ~TerminalSettings() {
            // Restore original terminal settings
            tcsetattr(STDIN_FILENO, TCSANOW, &original_settings_);
        }

    private:
        termios original_settings_;
    };
}

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

    // Set thread priority using QNX scheduler
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
    char c;

    while (input_running_) {
        if (read(STDIN_FILENO, &c, 1) == 1) {
            if (c == '\n') {
                if (!input_buffer.empty()) {
                    enqueueCommand(input_buffer);
                    addToHistory(input_buffer);
                    input_buffer.clear();
                }
                if (echo_enabled_) {
                    std::cout << "\n";
                    displayPrompt();
                }
            }
            else if (c == 127 || c == 8) { // Backspace
                if (!input_buffer.empty()) {
                    input_buffer.pop_back();
                    if (echo_enabled_) {
                        std::cout << "\b \b" << std::flush;
                    }
                }
            }
            else if (c == 27) { // Escape sequence
                char seq[2];
                if (read(STDIN_FILENO, &seq[0], 1) == 1 &&
                    read(STDIN_FILENO, &seq[1], 1) == 1) {
                    if (seq[0] == '[') {
                        switch (seq[1]) {
                            case 'A': // Up arrow
                                input_buffer = getPreviousCommand();
                                break;
                            case 'B': // Down arrow
                                input_buffer = getNextCommand();
                                break;
                        }
                        if (echo_enabled_) {
                            clearInputLine();
                            displayPrompt();
                            std::cout << input_buffer << std::flush;
                        }
                    }
                }
            }
            else if (std::isprint(c) && input_buffer.length() < MAX_COMMAND_LENGTH) {
                input_buffer += c;
                if (echo_enabled_) {
                    std::cout << c << std::flush;
                }
            }
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
        auto result = command_processor_->processCommand(command);
        if (result.success) {
            if (result.response) {
                channel_->sendMessage(*result.response);
            }
            if (!result.message.empty()) {
                std::cout << result.message << std::endl;
            }
        }
        else {
            displayError(result.message);
        }

        processed_commands_++;
        updatePerformanceMetrics(start_time);
    }
    catch (const std::exception& e) {
        displayError(std::string("Command processing error: ") + e.what());
        Logger::getInstance().log("Command processing error: " + std::string(e.what()));
    }

    if (echo_enabled_) {
        displayPrompt();
    }
}

void OperatorConsole::updatePerformanceMetrics(const std::chrono::steady_clock::time_point& start_time) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    performance_.command_count++;
    performance_.average_processing_time_ms =
        (performance_.average_processing_time_ms * (performance_.command_count - 1) +
         duration.count()) / performance_.command_count;
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

void OperatorConsole::enqueueCommand(const std::string& command) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (command_queue_.size() < MAX_QUEUE_SIZE) {
        command_queue_.push(command);
        queue_cv_.notify_one();
    }
    else {
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

void OperatorConsole::clearScreen() const {
    std::cout << "\033[2J\033[H" << std::flush;
}

void OperatorConsole::displayPrompt() const {
    std::cout << PROMPT << std::flush;
}

void OperatorConsole::displayWelcomeMessage() const {
    clearScreen();
    std::cout << WELCOME_MESSAGE << std::endl;
    displayPrompt();
}

void OperatorConsole::displayError(const std::string& error) const {
    std::cout << "\033[31mError: " << error << "\033[0m" << std::endl;
}

void OperatorConsole::clearInputLine() const {
    std::cout << "\r" << std::string(PROMPT.length() + MAX_COMMAND_LENGTH, ' ') << "\r";
}

bool OperatorConsole::hasCommands() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return !command_queue_.empty();
}

size_t OperatorConsole::getCommandQueueSize() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return command_queue_.size();
}

void OperatorConsole::setEchoEnabled(bool enable) {
    echo_enabled_ = enable;
}

void OperatorConsole::displayStatus() const {
    std::cout << "\nOperator Console Status:"
              << "\nProcessed Commands: " << processed_commands_
              << "\nQueue Size: " << getCommandQueueSize()
              << "\nAverage Processing Time: " << std::fixed << std::setprecision(2)
              << performance_.average_processing_time_ms << "ms"
              << "\nOperational: " << (operational_ ? "Yes" : "No")
              << std::endl;
}

}
