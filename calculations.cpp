#include "calculations.h"
#include <cmath>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <sstream>

int64_t calculateDistanceCounts(const RallyState& state, uint64_t cntr1, uint64_t cntr2,
                                  uint64_t start1, uint64_t start2) {
    int64_t delta1 = static_cast<int64_t>(cntr1) - static_cast<int64_t>(start1);
    
    if (state.counters) {
        // Two wheel: average
        int64_t delta2 = static_cast<int64_t>(cntr2) - static_cast<int64_t>(start2);
        return (delta1 + delta2) / 2;
    } else {
        // One gearbox: just CNTR_1
        return delta1;
    }
}

long countsToCentimeters(int64_t counts, long calibration) {
    // meters = (counts * calibration) / 1000 / 1000
    // centimeters = (counts * calibration) / 1000 / 10
    return (counts * calibration) / 10000;
}

double countsPerHourToKPH(long counts_per_hour, long calibration) {
    // counts/h * (cal/1000/1000) m/count * 1000 m/km = counts/h * cal / 1000
    double meters_per_hour = (counts_per_hour * calibration) / 1000.0;
    return meters_per_hour / 1000.0;  // to km/h
}

long kphToCountsPerHour(double kph, long calibration) {
    // kph * 1000 m/km / (cal/1000/1000) m/count = kph * 1000 * 1000 * 1000 / cal
    return static_cast<long>((kph * 1000000.0) / calibration);
}

int64_t getRallyTime_ms(const RallyState& state) {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    return ms + state.rallyTimeOffset_ms;
}

std::string formatTime(int64_t time_ms) {
    time_t seconds = time_ms / 1000;
    struct tm* tm = localtime(&seconds);
    char buf[20];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
    return std::string(buf);
}

std::string formatDuration(int64_t duration_ms) {
    int64_t total_seconds = duration_ms / 1000;
    int hours = total_seconds / 3600;
    int minutes = (total_seconds % 3600) / 60;
    int seconds = total_seconds % 60;
    char buf[20];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hours, minutes, seconds);
    return std::string(buf);
}

double calculateCurrentSpeed(const RallyState& state, const CounterPoll& current, 
                            const CounterPoll& tenth) {
    if (tenth.time_ms == 0 || current.time_ms == 0) {
        return -1.0;  // Invalid
    }
    
    int64_t time_diff_ms = current.time_ms - tenth.time_ms;
    if (time_diff_ms <= 0) {
        return -1.0;
    }
    
    int64_t count_diff = calculateDistanceCounts(state, current.cntr1, current.cntr2,
                                                  tenth.cntr1, tenth.cntr2);
    long cm_diff = countsToCentimeters(count_diff, state.calibration);
    
    // Speed in cm/s
    double speed_cm_per_s = (cm_diff * 1000.0) / time_diff_ms;
    
    // Convert to KPH or MPH
    if (state.units) {
        // MPH: cm/s * 3600 / 160934
        return (speed_cm_per_s * 3600.0) / 160934.0;
    } else {
        // KPH: cm/s * 3600 / 100000
        return (speed_cm_per_s * 3600.0) / 100000.0;
    }
}

double calculateAverageSpeed(const RallyState& state, int64_t start_time_ms, 
                            int64_t current_time_ms, int64_t count_diff) {
    int64_t time_diff_ms = current_time_ms - start_time_ms;
    if (time_diff_ms <= 0) {
        return 0.0;
    }
    
    long cm_diff = countsToCentimeters(count_diff, state.calibration);
    double speed_cm_per_s = (cm_diff * 1000.0) / time_diff_ms;
    
    if (state.units) {
        return (speed_cm_per_s * 3600.0) / 160934.0;  // MPH
    } else {
        return (speed_cm_per_s * 3600.0) / 100000.0;  // KPH
    }
}

double calculateAheadBehind(const RallyState& state, int64_t current_time_ms,
                          int64_t segment_start_time, long target_counts_per_hour,
                          int64_t actual_counts) {
    if (state.segment_current_number < 0 || target_counts_per_hour == 0) {
        return 0.0;
    }
    
    int64_t time_ms_since_segment = current_time_ms - segment_start_time;
    double ideal_counts = (time_ms_since_segment / 3600000.0) * target_counts_per_hour;
    double diff = actual_counts - ideal_counts;
    double seconds = diff / (target_counts_per_hour / 3600.0);
    return seconds;
}
