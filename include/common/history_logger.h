#ifndef ATC_HISTORY_LOGGER_H
#define ATC_HISTORY_LOGGER_H

#include "common/periodic_task.h"
#include "common/types.h"
#include "core/aircraft.h"
#include <vector>
#include <memory>
#include <fstream>
#include <string>

namespace atc {

class HistoryLogger : public PeriodicTask {
public:
    explicit HistoryLogger(const std::string& filename = "airspace_history.log");
    ~HistoryLogger();

    void updateAircraftStates(const std::vector<std::shared_ptr<Aircraft>>& aircraft);
    bool isOperational() const { return file_operational_; }


protected:
    void execute() override;

private:
    void writeHeader();
    void writeStateEntry(const std::vector<AircraftState>& states);
    void reopenFile();
    std::string getTimestamp() const;

    std::ofstream history_file_;
    std::mutex file_mutex_;
    std::vector<AircraftState> current_states_;
    bool file_operational_;
    const std::string filename_;
    static constexpr size_t MAX_BUFFER_SIZE = 1024 * 1024;  // 1MB buffer size
};

}
#endif // ATC_HISTORY_LOGGER_H
