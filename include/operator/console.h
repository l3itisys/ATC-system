#ifndef ATC_OPERATOR_CONSOLE_H
#define ATC_OPERATOR_CONSOLE_H

#include "common/periodic_task.h"
#include "communication/qnx_channel.h"
#include "operator/command.h"
#include <memory>
#include <queue>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <functional>

namespace atc {

using CommandHandler = std::function<void(const std::string&)>;

class OperatorConsole : public PeriodicTask {
public:
    explicit OperatorConsole(std::shared_ptr<comm::QnxChannel> channel);
    ~OperatorConsole() override;

    // Command input methods
    void inputCommand(const std::string& command);
    bool hasCommands() const;

    // Console control
    void setEchoEnabled(bool enable);
    void clearScreen() const;
    void displayPrompt() const;
    void displayWelcomeMessage() const;
    void displayStatus() const;
    void displayError(const std::string& error) const;
    void clearInputLine() const;

    // Status methods
    bool isOperational() const { return operational_; }
    size_t getCommandQueueSize() const;
    size_t getProcessedCommandCount() const { return processed_commands_; }
    const std::string& getLastError() const;
    std::vector<std::string> getCommandHistory() const;
    Performance getPerformanceMetrics() const;

protected:
    void execute() override;

private:
    // Message handling
    void registerMessageHandlers();
    void handleStatusResponse(const comm::Message& msg);
    void handleAlert(const comm::Message& msg);
    void processMessages();

    // Command processing
    void processCommandQueue();
    void processCommand(const std::string& command);
    bool validateCommand(const std::string& command, std::string& error) const;
    void logCommand(const std::string& command, bool success) const;

    // Input handling
    void handleKeypress(char c, std::string& buffer);
    void handleEscapeSequence(std::string& buffer);
    void handleSystemSignal(int signal);
    void handleWindowResize();

    // Custom handlers
    void registerCustomHandler(const std::string& command, CommandHandler handler);
    void unregisterCustomHandler(const std::string& command);

    // Performance management
    void resetPerformanceMetrics();
    std::string formatTimestamp(const std::chrono::steady_clock::time_point& time_point) const;
    void setLastError(const std::string& error);
    void clearCommandHistory();
    // Constants
    static constexpr size_t MAX_QUEUE_SIZE = 100;
    static constexpr int INPUT_TIMEOUT_MS = 100;
    static constexpr int MAX_COMMAND_LENGTH = 256;
    static constexpr size_t MAX_HISTORY_SIZE = 50;

    static std::string getPrompt() {
        static const std::string prompt = "ATC> ";
        return prompt;
    }

    static std::string getWelcomeMessage() {
        static const std::string msg = 
            "\n=== Air Traffic Control System Console ===\n"
            "Type 'HELP' for available commands\n"
            "Type 'EXIT' to quit\n";
        return msg;
    }

    // Input handling
    void startInputThread();
    void stopInputThread();
    void inputThreadFunction();
    void processInputBuffer();

    // Command handling
    void enqueueCommand(const std::string& command);
    std::string dequeueCommand();
    void processNextCommand();
    void handleCommandResponse(const comm::Message& response);

    // History management
    void addToHistory(const std::string& command);
    std::string getPreviousCommand();
    std::string getNextCommand();

    // Performance tracking
    void updatePerformanceMetrics(const std::chrono::steady_clock::time_point& start_time);
    bool shouldLogPerformance() const;
    void logPerformanceStats() const;
    long getSystemUptime() const;
    int getActiveAircraftCount() const;

    // Data members
    std::shared_ptr<comm::QnxChannel> channel_;
    std::unique_ptr<CommandProcessor> command_processor_;
    std::queue<std::string> command_queue_;
    mutable std::mutex queue_mutex_;
    std::atomic<bool> input_running_;
    std::atomic<bool> operational_;
    std::atomic<size_t> processed_commands_;
    std::thread input_thread_;
    std::condition_variable queue_cv_;
    bool echo_enabled_;

    // Command history
    std::vector<std::string> command_history_;
    size_t history_index_;

    // Performance monitoring
    struct Performance {
        std::chrono::steady_clock::time_point last_command_time;
        double average_processing_time_ms;
        size_t command_count;
        Performance() : average_processing_time_ms(0), command_count(0) {}
    } performance_;

    std::chrono::steady_clock::time_point system_start_time_{std::chrono::steady_clock::now()};
};

} // namespace atc

#endif // ATC_OPERATOR_CONSOLE_H
