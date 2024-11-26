#ifndef ATC_OPERATOR_CONSOLE_H
#define ATC_OPERATOR_CONSOLE_H

#include "common/periodic_task.h"
#include "operator/command.h"
#include "communication/qnx_channel.h"
#include <thread>
#include <mutex>
#include <queue>
#include <string>
#include <vector>
#include <memory>
#include <condition_variable>
#include <unordered_map>
#include <functional>
#include <chrono>

namespace atc {

// Forward declaration for CommandHandler
using CommandHandler = std::function<void(const std::string&)>;

class OperatorConsole : public PeriodicTask {
public:
    OperatorConsole(std::shared_ptr<comm::QnxChannel> channel);
    ~OperatorConsole();

    bool isOperational() const { return operational_; }
    void setEchoEnabled(bool enable);
    void registerCustomHandler(const std::string& command, CommandHandler handler);
    void unregisterCustomHandler(const std::string& command);
    std::vector<std::string> getCommandHistory() const;
    void clearCommandHistory();

    struct Performance {
        size_t command_count = 0;
        double average_processing_time_ms = 0.0;
        std::chrono::system_clock::time_point last_command_time;
    };
    Performance getPerformanceMetrics() const;
    void resetPerformanceMetrics();
    const std::string& getLastError() const;
    void setLastError(const std::string& error);

private:
    void startInputThread();
    void stopInputThread();
    void inputThreadFunction();
    void handleKeypress(char c, std::string& buffer);
    void handleEscapeSequence(std::string& buffer);
    void execute() override;
    void processMessages();
    void handleStatusResponse(const comm::Message& msg);
    void handleAlert(const comm::Message& msg);
    void processCommandQueue();
    void processCommand(const std::string& command);
    void enqueueCommand(const std::string& command);
    std::string dequeueCommand();
    void addToHistory(const std::string& command);
    std::string getPreviousCommand();
    std::string getNextCommand();
    void updatePerformanceMetrics(const std::chrono::steady_clock::time_point& start_time);
    bool shouldLogPerformance() const;
    void logPerformanceStats() const;
    void displayWelcomeMessage() const;
    void displayPrompt() const;
    void displayError(const std::string& error) const;
    void clearScreen() const;
    void clearInputLine() const;
    void displayStatus() const;
    bool hasCommands() const;
    size_t getCommandQueueSize() const;
    long getSystemUptime() const;
    void handleSystemSignal(int signal);
    void handleWindowResize();
    bool validateCommand(const std::string& command, std::string& error) const;
    void logCommand(const std::string& command, bool success) const;
    std::string formatTimestamp(const std::chrono::system_clock::time_point& time_point) const;
    void registerMessageHandlers();
    std::string getPrompt() const;

private:
    // Member variables
    std::shared_ptr<comm::QnxChannel> channel_;
    std::unique_ptr<CommandProcessor> command_processor_;
    std::thread input_thread_;
    bool input_running_;
    bool operational_;
    size_t processed_commands_;
    bool echo_enabled_;

    mutable std::mutex queue_mutex_;    // Changed to mutable
    std::queue<std::string> command_queue_;
    std::condition_variable queue_cv_;
    static const size_t MAX_QUEUE_SIZE = 100;

    std::vector<std::string> command_history_;
    size_t history_index_;
    static const size_t MAX_HISTORY_SIZE = 50;
    static const size_t MAX_COMMAND_LENGTH = 256;

    Performance performance_;
    std::chrono::steady_clock::time_point system_start_time_ = std::chrono::steady_clock::now();

    std::string last_error_;
    mutable std::mutex handlers_mutex_;  // Changed to mutable
    std::unordered_map<std::string, CommandHandler> custom_handlers_;
    mutable std::mutex history_mutex_;   // Changed to mutable

    int terminal_width_ = 80;
    int terminal_height_ = 24;

    static const std::string PROMPT;
};

} // namespace atc

#endif // ATC_OPERATOR_CONSOLE_H

