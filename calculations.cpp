#include "calculations.h"
#include <cmath>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <sstream>

int64_t calculateDistanceCounts(const RallyState& state, uint64_t cntr1, uint64_t cntr2,
                                  uint64_t start1, uint64_t start2,
                                  bool dont_apply_offset) {
    int64_t delta1 = static_cast<int64_t>(cntr1) - static_cast<int64_t>(start1);
    int64_t cntr_a;
    
    if (state.counters) {
        // Two wheel: average
        int64_t delta2 = static_cast<int64_t>(cntr2) - static_cast<int64_t>(start2);
        cntr_a = (delta1 + delta2) / 2;
    } else {
        // One gearbox: just CNTR_1
        cntr_a = delta1;
    }
    return dont_apply_offset ? cntr_a : cntr_a + state.distance_offset_counts;
}

void restartDistancesAfterPowerLoss(RallyState& state, uint64_t cntr1, uint64_t cntr2,
                                    int64_t now_ms) {
    state.total_start_cntr1 = cntr1;
    state.total_start_cntr2 = cntr2;
    state.total_start_time_ms = now_ms;
    state.trip_start_cntr1 = cntr1;
    state.trip_start_cntr2 = cntr2;
    state.trip_start_time_ms = now_ms;
    state.segment_start_cntr1 = cntr1;
    state.segment_start_cntr2 = cntr2;
    state.segment_start_time_ms = now_ms;
    state.segment_current_number = -1;
    state.alarm_distance_km = 0;
    state.alarm_target_counts = 0;
    state.distance_offset_counts = 0;
}

long countsToCentimeters(int64_t counts, long calibration) {
    // meters = (counts * calibration) / 1000 / 1000
    // centimeters = (counts * calibration) / 1000 / 10
    return (counts * calibration) / 10000;
}

double countsToMeters(double counts, long calibration) {
    // calibration = mm per 1000 counts, so m/count = calibration / 1e6
    return (counts * calibration) / 1e6;
}

double metersToCounts(double meters, long calibration) {
    // Inverse of countsToMeters
    return (meters * 1e6) / calibration;
}

// Split entry text into an optional leading sign and whole metres.
// Returns false unless the rest is one or more digits.
static bool parseSignedMetres(const std::string& text, char& sign, long& metres) {
    size_t i = 0;
    while (i < text.size() && text[i] == ' ') i++;
    sign = 0;
    if (i < text.size() && (text[i] == '+' || text[i] == '-')) sign = text[i++];
    size_t digits_start = i;
    long value = 0;
    while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
        value = value * 10 + (text[i] - '0');
        i++;
    }
    while (i < text.size() && text[i] == ' ') i++;
    if (i == digits_start || i != text.size()) return false;
    metres = value;
    return true;
}

bool applyDistanceAdjustmentText(const std::string& text, int64_t current_offset_counts,
                                 int64_t current_total_counts, long calibration,
                                 int64_t& new_offset_counts) {
    char sign; long metres;
    if (!parseSignedMetres(text, sign, metres)) return false;
    int64_t counts = static_cast<int64_t>(std::llround(metersToCounts(static_cast<double>(metres), calibration)));
    if (sign == '+') new_offset_counts = current_offset_counts + counts;
    else if (sign == '-') new_offset_counts = current_offset_counts - counts;
    else if (metres == 0) new_offset_counts = 0;   // "0" zeroes the offset
    else new_offset_counts = current_offset_counts + (counts - current_total_counts);  // total reads N
    return true;
}

std::string nudgeDistanceAdjustmentText(const std::string& text, long delta_m) {
    char sign = '+'; long metres = 0;
    if (!text.empty() && !parseSignedMetres(text, sign, metres)) return text;
    if (text.empty()) sign = '+';
    if (sign == '+' || sign == '-') {
        long value = (sign == '-' ? -metres : metres) + delta_m;
        return (value < 0 ? "-" : "+") + std::to_string(value < 0 ? -value : value);
    }
    long value = metres + delta_m;
    if (value < 0) value = 0;
    return std::to_string(value);
}

double countsPerHourToKPH(double counts_per_hour, long calibration) {
    // calibration = mm per 1000 counts
    // m/count = calibration / 1000000
    // meters/hour = counts_per_hour * calibration / 1000000
    // km/hour = meters/hour / 1000 = counts_per_hour * calibration / 1e9
    return (counts_per_hour * calibration) / 1e9;
}

double kphToCountsPerHour(double kph, long calibration) {
    // Inverse of countsPerHourToKPH
    // counts_per_hour = kph * 1e9 / calibration
    return (kph * 1e9) / calibration;
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

double calculateCurrentSpeed(const RallyState& state, const CounterPoll& current, 
                            const CounterPoll& tenth) {
    if (tenth.time_ms == 0 || current.time_ms == 0) {
        return -1.0;  // Invalid
    }
    
    int64_t time_diff_ms = current.time_ms - tenth.time_ms;
    if (time_diff_ms <= 0) {
        return -1.0;
    }
    
    // Raw difference between two readings: the distance offset does not apply
    int64_t count_diff = calculateDistanceCounts(state, current.cntr1, current.cntr2,
                                                  tenth.cntr1, tenth.cntr2, true);
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
                          int64_t segment_start_time, double target_counts_per_hour,
                          int64_t actual_counts) {
    if (state.segment_current_number < 0 || target_counts_per_hour == 0.0) {
        return 0.0;
    }
    
    // High precision calculation
    double time_hours_since_segment = static_cast<double>(current_time_ms - segment_start_time) / 3600000.0;
    double ideal_counts = time_hours_since_segment * target_counts_per_hour;
    double diff = static_cast<double>(actual_counts) - ideal_counts;
    double counts_per_second = target_counts_per_hour / 3600.0;
    double seconds = diff / counts_per_second;
    return seconds;
}

double calculateIdealCountsFromStageStart(const RallyState& state, int64_t elapsed_ms) {
    if (state.segment_current_number < 0 || state.segments.empty()) {
        return 0.0;
    }
    
    double ideal_counts = 0.0;
    double remaining_time_s = elapsed_ms / 1000.0;
    
    // Go through each segment up to and including current
    for (int i = 0; i <= state.segment_current_number && i < static_cast<int>(state.segments.size()); i++) {
        const Segment& seg = state.segments[i];
        
        if (seg.target_speed_counts_per_hour <= 0.0) {
            continue;  // Skip invalid segments
        }
        
        // Time to complete this segment at target speed (in seconds)
        // time = distance / speed = distance_counts / (counts_per_hour / 3600)
        double segment_time_s = seg.distance_counts * 3600.0 / seg.target_speed_counts_per_hour;
        
        if (i < state.segment_current_number) {
            // Completed segment - add full distance
            ideal_counts += seg.distance_counts;
            remaining_time_s -= segment_time_s;
        } else {
            // Current segment - use remaining time at this segment's speed
            // remaining_time_s can be negative if ahead (drove earlier segments faster
            // than target), which correctly reduces ideal_counts
            double partial_counts = (remaining_time_s / 3600.0) * seg.target_speed_counts_per_hour;
            ideal_counts += partial_counts;
        }
    }
    
    return ideal_counts;
}

double calculateAheadBehindFromStageStart(const RallyState& state, int64_t current_time_ms,
                                          int64_t actual_counts_from_stage_start) {
    if (state.segment_current_number < 0 || state.segments.empty()) {
        return 0.0;
    }
    
    // Get elapsed time since stage (total) start
    int64_t elapsed_ms = current_time_ms - state.total_start_time_ms;
    if (elapsed_ms <= 0) {
        return 0.0;
    }
    
    // Calculate where we should ideally be
    double ideal_counts = calculateIdealCountsFromStageStart(state, elapsed_ms);
    
    // Difference: positive = ahead (traveled more than ideal), negative = behind
    double diff = static_cast<double>(actual_counts_from_stage_start) - ideal_counts;
    
    // Convert to seconds using current segment's target speed
    const Segment& current_seg = state.segments[state.segment_current_number];
    if (current_seg.target_speed_counts_per_hour <= 0.0) {
        return 0.0;
    }
    
    double counts_per_second = current_seg.target_speed_counts_per_hour / 3600.0;
    double seconds = diff / counts_per_second;
    
    return seconds;
}
