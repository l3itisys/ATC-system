#ifndef ATC_DISPLAY_SYSTEM_H
#define ATC_DISPLAY_SYSTEM_H

#include "common/periodic_task.h"
#include "common/warning_level.h"
#include "core/aircraft.h"
#include "core/violation_detector.h"
#include <vector>
#include <memory>
#include <string>
#include <queue>

namespace atc {

class DisplaySystem : public PeriodicTask {
public:
    explicit DisplaySystem(std::shared_ptr<ViolationDetector> violation_detector);

    void addAircraft(const std::shared_ptr<Aircraft>& aircraft);
    void removeAircraft(const std::string& callsign);
    void displayAlert(const std::string& message);
    void setTrackedAircraft(const std::string& callsign);
    void clearTrackedAircraft();

protected:
    void execute() override;

private:
    struct GridCell {
        char symbol{' '};
        std::string aircraft_id;
        bool is_predicted{false};
        bool is_tracked{false};
        bool is_emergency{false};
        bool is_conflict_point{false};
        WarningLevel warning_level{WarningLevel::NONE};
    };

    // Grid management
    void initializeGrid();
    void updateGrid();
    void updateDisplay();

    // Display methods
    void displayGrid();
    void displayCell(const GridCell& cell);
    void displayHeader();
    void displayAircraftDetails();
    void clearScreen() const;

    // Helper methods
    WarningLevel determineWarningLevel(const AircraftState& state) const;
    char getAircraftSymbol(const AircraftState& state) const;
    const char* getWarningColor(WarningLevel level) const;
    std::string formatPosition(const Position& pos) const;
    bool isValidGridPosition(int x, int y) const;
    void markPredictedConflictPoint(const Position& point);

    // Member variables
    std::vector<std::vector<GridCell>> grid_;
    std::vector<std::shared_ptr<Aircraft>> aircraft_;
    std::shared_ptr<ViolationDetector> violation_detector_;
    std::queue<std::string> alerts_;
    std::string tracked_aircraft_;
    std::mutex mutex_;

    // Display settings
    bool paused_{false};
    bool show_grid_{true};
    int refresh_rate_{5};
    std::chrono::steady_clock::time_point last_update_;

    // Constants
    static constexpr int GRID_WIDTH = 50;
    static constexpr int GRID_HEIGHT = 25;
    static constexpr size_t MAX_ALERTS = 5;
    static constexpr int MIN_REFRESH_RATE = 1;
    static constexpr int MAX_REFRESH_RATE = 30;
};

} // namespace atc

#endif // ATC_DISPLAY_SYSTEM_H
