#ifndef ATC_RADAR_SYSTEM_H
#define ATC_RADAR_SYSTEM_H

#include "common/periodic_task.h"
#include "common/types.h"
#include "communication/qnx_channel.h"
#include "core/aircraft.h"
#include <memory>
#include <vector>
#include <mutex>
#include <unordered_map>

namespace atc {

class RadarSystem : public PeriodicTask {
public:
    static constexpr int PSR_SCAN_INTERVAL = 4000;  // milliseconds
    static constexpr int SSR_INTERROGATION_INTERVAL = 1000;  // milliseconds

    explicit RadarSystem(std::shared_ptr<comm::QnxChannel> channel);
    ~RadarSystem() = default;

    // Aircraft tracking management
    void addAircraft(const std::shared_ptr<Aircraft>& aircraft);
    void removeAircraft(const std::string& callsign);

    // Radar data access
    std::vector<AircraftState> getTrackedAircraft() const;
    AircraftState getAircraftState(const std::string& callsign) const;
    bool isAircraftTracked(const std::string& callsign) const;

protected:
    void execute() override;

private:
    struct RadarTrack {
        AircraftState state;
        bool has_transponder_response;
        std::chrono::steady_clock::time_point last_update;
        int track_quality;  // 0-100%, indicates confidence in track

        RadarTrack() : has_transponder_response(false), track_quality(0) {}
    };

    void performPrimaryScan();
    void performSecondaryInterrogation();
    void updateTracks();
    void cleanupStaleTracks();
    bool validateRadarReturn(const Position& pos) const;
    void logTrackUpdates() const;

    std::shared_ptr<comm::QnxChannel> channel_;
    std::unordered_map<std::string, RadarTrack> tracks_;
    std::vector<std::shared_ptr<Aircraft>> aircraft_;
    mutable std::mutex radar_mutex_;

    // Performance metrics
    int primary_scan_count_{0};
    int secondary_scan_count_{0};
    int track_updates_{0};
    std::chrono::steady_clock::time_point last_primary_scan_;
    std::chrono::steady_clock::time_point last_secondary_scan_;

    // Track management parameters
    static constexpr int MAX_TRACK_AGE_MS = 10000;  // Maximum age of track before removal
    static constexpr int MIN_TRACK_QUALITY = 30;    // Minimum quality for valid track
    static constexpr double MAX_POSITION_ERROR = 100.0; // Maximum position error in units
};

}

#endif // ATC_RADAR_SYSTEM_H
