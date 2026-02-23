#ifndef RALLY_STATE_H
#define RALLY_STATE_H

#include <cstdint>
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
    std::vector<Segment> segments;
    
    // Up to 5 memory slots for storing/recalling segment setups
    static constexpr int MAX_MEMORY_SLOTS = 5;
    std::vector<Segment> memory_slots[5];
    
    // Driver window position/size (remembered across sessions)
    int driver_window_x = -1;      // -1 = not set
    int driver_window_y = -1;
    int driver_window_width = 1280;
    int driver_window_height = 400;
    int driver_window_monitor = 0;
    
    RallyState();
};

#endif // RALLY_STATE_H
