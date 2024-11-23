#include "common/logger.h"
#include <iostream>
#include <chrono>
#include <ctime>

namespace atc {

Logger::Logger() {
    log_file_.open("system.log", std::ios::out | std::ios::app);
    if (!log_file_) {
        std::cerr << "Failed to open log file" << std::endl;
    }
}

Logger::~Logger() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::log(const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    if (log_file_.is_open()) {
        // Get current time
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        log_file_ << std::ctime(&time) << ": " << message << std::endl;
    }
}

} // namespace atc

