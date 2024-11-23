#ifndef ATC_LOGGER_H
#define ATC_LOGGER_H

#include <fstream>
#include <mutex>
#include <string>

namespace atc {

class Logger {
public:
    static Logger& getInstance();
    void log(const std::string& message);

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::ofstream log_file_;
    std::mutex log_mutex_;
};

} // namespace atc

#endif // ATC_LOGGER_H

