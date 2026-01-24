#include "rally_state.h"
#include <chrono>

RallyState::RallyState() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    total_start_time_ms = ms;
    trip_start_time_ms = ms;
    segment_start_time_ms = ms;
}
