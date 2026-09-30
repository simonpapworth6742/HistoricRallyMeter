#ifndef RALLY_STATE_H
#define RALLY_STATE_H

#include <cstdint>
#include <string>
#include <vector>
#include "rally_types.h"

class RallyState {
public:
    bool units = false;  // false = KPH, true = MPH
    long calibration = 600000;  // mm per 1000 counts
    bool counters = true;  // false = one gearbox, true = two wheel
    uint64_t total_start_cntr1 = 0;
    uint64_t total_start_cntr2 = 0;
    int64_t total_start_time_ms = 0;  // milliseconds since epoch
    uint64_t trip_start_cntr1 = 0;
    uint64_t trip_start_cntr2 = 0;
    int64_t trip_start_time_ms = 0;
    uint64_t segment_start_cntr1 = 0;
    uint64_t segment_start_cntr2 = 0;
    int64_t segment_start_time_ms = 0;
    long segment_current_number = -1;  // -1 = no segment
    long rallyTimeOffset_ms = 0;  // offset in milliseconds
    long ahead_behind_zero_offset_ms = 0;  // manual offset for driver's ahead/behind display
    // Counts added to CNTR_A for total, trip and segment. Negative removes a
    // detour. Zeroed by Total reset, stage go and the power-loss reset.
    int64_t distance_offset_counts = 0;

    // Trip and segment start readings include the offset so they read zero now.
    void startTripAt(uint64_t cntr1, uint64_t cntr2, int64_t now_ms) {
        trip_start_cntr1 = static_cast<uint64_t>(static_cast<int64_t>(cntr1) + distance_offset_counts);
        trip_start_cntr2 = static_cast<uint64_t>(static_cast<int64_t>(cntr2) + distance_offset_counts);
        trip_start_time_ms = now_ms;
    }
    void startSegmentAt(uint64_t cntr1, uint64_t cntr2, int64_t now_ms) {
        segment_start_cntr1 = static_cast<uint64_t>(static_cast<int64_t>(cntr1) + distance_offset_counts);
        segment_start_cntr2 = static_cast<uint64_t>(static_cast<int64_t>(cntr2) + distance_offset_counts);
        segment_start_time_ms = now_ms;
    }
    // Total reset: the offset goes to zero, so take it back out of the trip and
    // segment start readings to leave those two distances unchanged.
    void startTotalAt(uint64_t cntr1, uint64_t cntr2, int64_t now_ms) {
        int64_t off = distance_offset_counts;
        trip_start_cntr1 = static_cast<uint64_t>(static_cast<int64_t>(trip_start_cntr1) - off);
        trip_start_cntr2 = static_cast<uint64_t>(static_cast<int64_t>(trip_start_cntr2) - off);
        segment_start_cntr1 = static_cast<uint64_t>(static_cast<int64_t>(segment_start_cntr1) - off);
        segment_start_cntr2 = static_cast<uint64_t>(static_cast<int64_t>(segment_start_cntr2) - off);
        distance_offset_counts = 0;
        total_start_cntr1 = cntr1;
        total_start_cntr2 = cntr2;
        total_start_time_ms = now_ms;
    }
    // Last six trip distances in whole metres, most recent first, comma
    // separated. Every trip reset records the trip first (see resetTrip).
    std::string trip_history_m;
    uint64_t auto_start_rally_time_minutes = 0;  // minutes since 1/1/2020, 0 = not set
    std::vector<Segment> segments;
    
    // Up to 5 memory slots for storing/recalling segment setups
    static constexpr int MAX_MEMORY_SLOTS = 5;
    std::vector<Segment> memory_slots[5];
    
    // Alarm: co-pilot sets distance alarm that rings a doorbell
    int alarm_distance_km = 0;          // 0 = no alarm active
    int64_t alarm_target_counts = 0;    // absolute count target from total_start

    // Force single-display mode even when multiple screens exist
    bool force_single_display = false;

    // Embedded web server for phone browsers
    bool web_enabled = true;
    int web_port = 8080;

    // Remembered Bluetooth speaker. Both empty when none is remembered.
    std::string bluetooth_audio_name;
    std::string bluetooth_audio_address;
    // When set, the app reconnects that speaker on startup. BlueZ trust follows it.
    bool bluetooth_audio_autoconnect = false;
    
    // Driver window position/size (remembered across sessions)
    int driver_window_x = -1;      // -1 = not set
    int driver_window_y = -1;
    int driver_window_width = 800;
    int driver_window_height = 480;
    int driver_window_monitor = 0;
    
    RallyState();
};

#endif // RALLY_STATE_H
