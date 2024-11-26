// src/common/logger.cpp
#include "common/logger.h"
#include <iostream>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <sstream>

namespace atc {

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger()
    : console_output_enabled_(true) {
    // Default constructor
}

Logger::~Logger() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

void Logger::setLogFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (log_file_.is_open()) {
        log_file_.close();
    }
    log_file_.open(filename, std::ios::out | std::ios::app);
    if (!log_file_) {
        std::cerr << "Failed to open log file: " << filename << std::endl;
    }
}

void Logger::enableConsoleOutput(bool enable) {
    console_output_enabled_ = enable;
}

void Logger::log(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    write(message);
}

void Logger::write(const std::string& message) {
    // Get current time
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);

    // Format time string
    std::tm tm_now;
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&tm_now, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_now);
#endif
    std::ostringstream time_stream;
    time_stream << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S");

    // Prepare the full log message
    std::string full_message = "[" + time_stream.str() + "] " + message;

    // Write to console
    if (console_output_enabled_) {
        std::cout << full_message << std::endl;
    }

    // Write to log file if open
    if (log_file_.is_open()) {
        log_file_ << full_message << std::endl;
    }
}

} // namespace atc

