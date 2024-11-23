#include "common/history_logger.h"
#include "common/constants.h"
#include "common/logger.h"
#include <iomanip>
#include <sstream>
#include <ctime>

namespace atc {

HistoryLogger::HistoryLogger(const std::string& filename)
    : PeriodicTask(std::chrono::milliseconds(constants::HISTORY_LOGGING_INTERVAL),
                   constants::LOGGING_PRIORITY)
    , filename_(filename)
    , file_operational_(false) {

    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << filename_ << "_" << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S") << ".log";

    history_file_.open(ss.str(), std::ios::out | std::ios::app);
    if (history_file_.is_open()) {
        writeHeader();
        file_operational_ = true;
        Logger::getInstance().log("History logger initialized: " + ss.str());
    } else {
        Logger::getInstance().log("Failed to initialize history logger");
        file_operational_ = false;
    }
}

HistoryLogger::~HistoryLogger() {
    if (history_file_.is_open()) {
        history_file_.close();
    }
}

void HistoryLogger::writeHeader() {
    std::lock_guard<std::mutex> lock(file_mutex_);
    if (file_operational_) {
        history_file_ << "\n=== ATC System History Log ===\n";
        history_file_ << "Started at: " << getTimestamp() << "\n";
        history_file_ << "Logging interval: " << constants::HISTORY_LOGGING_INTERVAL << "ms\n";
        history_file_ << std::string(50, '-') << "\n";
        history_file_.flush();
    }
}

std::string HistoryLogger::getTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void HistoryLogger::updateAircraftStates(const std::vector<std::shared_ptr<Aircraft>>& aircraft) {
    std::lock_guard<std::mutex> lock(file_mutex_);
    current_states_.clear();
    for (const auto& ac : aircraft) {
        if (ac) {
            current_states_.push_back(ac->getState());
        }
    }
}

void HistoryLogger::writeStateEntry(const std::vector<AircraftState>& states) {
    if (!file_operational_) return;

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    std::stringstream buffer;
    buffer << "\n=== Airspace State at "
           << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
           << " ===\n";
    buffer << "Active Aircraft: " << states.size() << "\n\n";

    for (const auto& state : states) {
        buffer << std::fixed << std::setprecision(2)
               << "Aircraft ID: " << state.callsign << "\n"
               << "Position: (" << state.position.x << ", "
               << state.position.y << ", " << state.position.z << ")\n"
               << "Speed: " << state.getSpeed() << " units/s\n"
               << "Heading: " << state.heading << " degrees\n"
               << "Status: " << Aircraft::getStatusString(state.status) << "\n"
               << "Timestamp: " << state.timestamp << "\n\n";

        if (buffer.tellp() >= MAX_BUFFER_SIZE) {
            history_file_ << buffer.str();
            history_file_.flush();
            buffer.str("");
            buffer.clear();
        }
    }

    if (states.size() > 1) {
        buffer << "Separation Analysis:\n";
        for (size_t i = 0; i < states.size(); ++i) {
            for (size_t j = i + 1; j < states.size(); ++j) {
                Position pos1 = states[i].position;
                Position pos2 = states[j].position;

                double dx = pos1.x - pos2.x;
                double dy = pos1.y - pos2.y;
                double dz = std::abs(pos1.z - pos2.z);
                double separation = std::sqrt(dx*dx + dy*dy);

                buffer << states[i].callsign << " - " << states[j].callsign
                      << ": Horizontal: " << separation
                      << "m, Vertical: " << dz << "m\n";
            }
        }
    }

    buffer << std::string(80, '-') << "\n";
    history_file_ << buffer.str();
    history_file_.flush();

    if (history_file_.fail()) {
        file_operational_ = false;
        Logger::getInstance().log("Failed writing to history file");
    }
}

void HistoryLogger::execute() {
    std::lock_guard<std::mutex> lock(file_mutex_);
    if (!file_operational_) {
        Logger::getInstance().log("History logger not operational - attempting to reopen file");
        reopenFile();
        return;
    }

    if (!current_states_.empty()) {
        try {
            writeStateEntry(current_states_);

            if (history_file_.fail()) {
                Logger::getInstance().log("Failed to write to history file - attempting to recover");
                reopenFile();
            }
        } catch (const std::exception& e) {
            Logger::getInstance().log("Error writing history: " + std::string(e.what()));
            file_operational_ = false;
        }
    }
}

void HistoryLogger::reopenFile() {
    if (history_file_.is_open()) {
        history_file_.close();
    }

    history_file_.clear();
    history_file_.open(filename_, std::ios::out | std::ios::app);

    if (history_file_.is_open()) {
        file_operational_ = true;
        Logger::getInstance().log("Successfully reopened history file");
        writeHeader();
    } else {
        file_operational_ = false;
        Logger::getInstance().log("Failed to reopen history file");
    }
}

}
