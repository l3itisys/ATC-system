#ifndef ATC_LOGGER_H
#define ATC_LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <memory>

namespace atc {

class Logger {
public:
    // Get the singleton instance of the logger
    static Logger& getInstance();

    // Delete copy constructor and assignment operator to prevent copies
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // Log a message
    void log(const std::string& message);

    // Set the log file (optional)
    void setLogFile(const std::string& filename);

    // Enable or disable console output
    void enableConsoleOutput(bool enable);

private:
    // Private constructor
    Logger();

    // Destructor
    ~Logger();

    // Helper method to write a message to the log
    void write(const std::string& message);

    // Mutex for thread safety
    std::mutex mutex_;

    // Output stream
    std::ofstream log_file_;

    // Flag to control console output
    bool console_output_enabled_;
};

} // namespace atc

#endif // ATC_LOGGER_H

