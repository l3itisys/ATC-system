#ifndef ATC_DISPLAY_SYSTEM_H
#define ATC_DISPLAY_SYSTEM_H

#include "common/periodic_task.h"
#include "common/warning_level.h"
#include "core/violation_detector.h"
#include "core/aircraft.h"
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <chrono>

namespace atc {

class DisplaySystem : public PeriodicTask {
public:
    explicit DisplaySystem(std::shared_ptr<ViolationDetector> violation_detector);

    void addAircraft(const std::shared_ptr<Aircraft>& aircraft);
    void removeAircraft(const std::string& callsign);
    void displayAlert(const std::string& message);
    void updateDisplay();
    void updateDisplay(const std::vector<std::shared_ptr<Aircraft>>& aircraft);

protected:
    void execute() override;

private:
    struct GridCell {
        char symbol = ' ';
        std::string aircraft_id;
        bool is_predicted = false;
        WarningLevel warning_level = WarningLevel::NONE;
        bool has_conflict = false;

        bool isEmpty() const {
            return symbol == ' ' && !is_predicted;
        }
    };

    static constexpr int GRID_WIDTH = 50;
    static constexpr int GRID_HEIGHT = 25;

    std::vector<std::vector<GridCell>> grid_;
    std::vector<std::shared_ptr<Aircraft>> aircraft_;
    std::shared_ptr<ViolationDetector> violation_detector_;
    std::string current_alert_;

    void initializeGrid();
    void updateGrid();
    void displayGrid();
    void displayHeader();
    void displayAircraftDetails();
    const char* getWarningColor(WarningLevel level) const;
    char getDirectionSymbol(double heading) const;
    std::string formatPosition(const Position& pos) const;
};

} // namespace atc

#endif
