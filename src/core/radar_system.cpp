#include "core/radar_system.h"
#include "common/logger.h"
#include "common/constants.h"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <utility>

namespace atc {

RadarSystem::RadarSystem(std::shared_ptr<comm::QnxChannel> channel)
    : PeriodicTask(std::chrono::milliseconds(SSR_INTERROGATION_INTERVAL),
                   constants::RADAR_PRIORITY)
    , channel_(channel)
    , last_primary_scan_(std::chrono::steady_clock::now())
    , last_secondary_scan_(std::chrono::steady_clock::now()) {

    Logger::getInstance().log("Radar system initialized");
}

void RadarSystem::addAircraft(const std::shared_ptr<Aircraft>& aircraft) {
    std::lock_guard<std::mutex> lock(radar_mutex_);
    aircraft_.push_back(aircraft);
    Logger::getInstance().log("Added aircraft to radar tracking: " +
                            aircraft->getState().callsign);
}

void RadarSystem::removeAircraft(const std::string& callsign) {
    std::lock_guard<std::mutex> lock(radar_mutex_);

    // Remove from tracked aircraft
    aircraft_.erase(
        std::remove_if(aircraft_.begin(), aircraft_.end(),
            [&callsign](const auto& aircraft) {
                return aircraft->getState().callsign == callsign;
            }),
        aircraft_.end()
    );

    // Remove from tracks
    if (tracks_.erase(callsign) > 0) {
        Logger::getInstance().log("Removed aircraft from radar tracking: " + callsign);
    }
}

void RadarSystem::execute() {
    auto now = std::chrono::steady_clock::now();

    // Perform primary radar scan every PSR_SCAN_INTERVAL
    if (std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_primary_scan_).count() >= PSR_SCAN_INTERVAL) {
        performPrimaryScan();
        last_primary_scan_ = now;
    }

    // Perform secondary radar interrogation every SSR_INTERROGATION_INTERVAL
    if (std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_secondary_scan_).count() >= SSR_INTERROGATION_INTERVAL) {
        performSecondaryInterrogation();
        last_secondary_scan_ = now;
    }

    updateTracks();
    cleanupStaleTracks();
}

void RadarSystem::performPrimaryScan() {
    std::lock_guard<std::mutex> lock(radar_mutex_);
    primary_scan_count_++;

    for (const auto& aircraft : aircraft_) {
        try {
            auto state = aircraft->getState();

            // Simulate radar detection with position error
            Position detected_pos = {
                state.position.x + (std::rand() % 100 - 50), // ±50 units error
                state.position.y + (std::rand() % 100 - 50),
                state.position.z + (std::rand() % 100 - 50)
            };

            if (validateRadarReturn(detected_pos)) {
                auto& track = tracks_[state.callsign];
                track.state.position = detected_pos;
                track.last_update = std::chrono::steady_clock::now();
                track.track_quality = std::min(100, track.track_quality + 10);
            }
        } catch (const std::exception& e) {
            Logger::getInstance().log("Error in primary radar scan: " +
                                    std::string(e.what()));
        }
    }

    Logger::getInstance().log("Completed primary radar scan #" +
                            std::to_string(primary_scan_count_));
}

void RadarSystem::performSecondaryInterrogation() {
    std::lock_guard<std::mutex> lock(radar_mutex_);
    secondary_scan_count_++;

    for (const auto& track_pair : tracks_) {
        const auto& callsign = track_pair.first;
        auto& track = track_pair.second;

        if (channel_) {
            comm::Message msg = comm::Message::createPositionUpdate(
                "RADAR", track.state);
            channel_->sendMessage(msg);
        }
    }
}

void RadarSystem::updateTracks() {
    std::lock_guard<std::mutex> lock(radar_mutex_);
    track_updates_++;

    for (auto& track_pair : tracks_) {
        auto& track = track_pair.second;
        auto now = std::chrono::steady_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - track.last_update).count();

        if (age > 1000) {
            track.track_quality = std::max(0, track.track_quality - 5);
        }
    }

    if (track_updates_ % 10 == 0) {
        logTrackUpdates();
    }
}

void RadarSystem::cleanupStaleTracks() {
    std::lock_guard<std::mutex> lock(radar_mutex_);
    auto now = std::chrono::steady_clock::now();

    auto it = tracks_.begin();
    while (it != tracks_.end()) {
        auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - it->second.last_update).count();

        if (age > MAX_TRACK_AGE_MS || it->second.track_quality < MIN_TRACK_QUALITY) {
            Logger::getInstance().log("Removing stale track: " + it->first);
            it = tracks_.erase(it);
        } else {
            ++it;
        }
    }
}

bool RadarSystem::validateRadarReturn(const Position& pos) const {
    // Check if position is within valid airspace
    return pos.isValid();
}

void RadarSystem::logTrackUpdates() const {
    std::ostringstream oss;
    oss << "\nRadar Track Update #" << track_updates_ << "\n"
        << "Active Tracks: " << tracks_.size() << "\n"
        << "Primary Scans: " << primary_scan_count_ << "\n"
        << "Secondary Interrogations: " << secondary_scan_count_ << "\n\n"
        << "Track Details:\n";

    for (const auto& track_pair : tracks_) {
        const auto& callsign = track_pair.first;
        const auto& track = track_pair.second;

        oss << "Aircraft: " << callsign << "\n"
            << "  Position: ("
            << std::fixed << std::setprecision(1)
            << track.state.position.x << ", "
            << track.state.position.y << ", "
            << track.state.position.z << ")\n"
            << "  Quality: " << track.track_quality << "%\n"
            << "  Transponder: " << (track.has_transponder_response ? "Active" : "Inactive")
            << "\n";
    }

    Logger::getInstance().log(oss.str());
}

std::vector<AircraftState> RadarSystem::getTrackedAircraft() const {
    std::lock_guard<std::mutex> lock(radar_mutex_);
    std::vector<AircraftState> states;
    states.reserve(tracks_.size());

    for (const auto& track_pair : tracks_) {
        const auto& track = track_pair.second;
        if (track.track_quality >= MIN_TRACK_QUALITY) {
            states.push_back(track.state);
        }
    }

    return states;
}

AircraftState RadarSystem::getAircraftState(const std::string& callsign) const {
    std::lock_guard<std::mutex> lock(radar_mutex_);
    auto it = tracks_.find(callsign);
    if (it != tracks_.end() && it->second.track_quality >= MIN_TRACK_QUALITY) {
        return it->second.state;
    }
    throw std::runtime_error("Aircraft not tracked: " + callsign);
}

bool RadarSystem::isAircraftTracked(const std::string& callsign) const {
    std::lock_guard<std::mutex> lock(radar_mutex_);
    auto it = tracks_.find(callsign);
    return it != tracks_.end() && it->second.track_quality >= MIN_TRACK_QUALITY;
}

}
