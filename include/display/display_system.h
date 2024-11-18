#ifndef ATC_DISPLAY_SYSTEM_H
#define ATC_DISPLAY_SYSTEM_H

#include "common/periodic_task.h"
#include "common/types.h"
#include "core/aircraft.h"
#include "core/violation_detector.h"
#include <memory>
#include <vector>
#include <mutex>

namespace atc {

class DisplaySystem : public PeriodicTask {
public:
    explicit DisplaySystem(std::shared_ptr<ViolationDetector> violation_detector);
    ~DisplaySystem() = default;

    void addAircraft(const std::shared_ptr<Aircraft>& aircraft);
    void removeAircraft(const std::string& callsign);

protected:
    void execute() override;

private:
    void displayHeader() const;
    void displayAircraft() const;
    void displayViolations() const;
    void displayFooter() const;
    void clearScreen() const;

    std::vector<std::shared_ptr<Aircraft>> aircraft_;
    std::shared_ptr<ViolationDetector> violation_detector_;
    mutable std::mutex display_mutex_;
    int update_count_{0};
};

}

#endif // ATC_DISPLAY_SYSTEM_H
