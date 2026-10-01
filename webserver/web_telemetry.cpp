#include "web_telemetry.h"
#include "calculations.h"
#include "callbacks.h"   // getAutoStartEpochMs
#include "rally_types.h"
#include "counter_poller.h"
#include "rally_state.h"

#include <cmath>
#include <cstdio>
#include <sstream>
#include <iomanip>

NextPrevState computeNextPrevState(AppData* data) {
    NextPrevState out;
    if (!data || !data->state || !data->poller) return out;

    if (data->state->segment_current_number < 0 ||
        data->state->segment_current_number >= static_cast<long>(data->state->segments.size())) {
        return out;
    }

    auto current_poll = data->poller->getMostRecent();
    const Segment& cur_seg = data->state->segments[data->state->segment_current_number];
    int64_t seg_count_diff = calculateDistanceCounts(*data->state,
        current_poll.cntr1, current_poll.cntr2,
        data->state->segment_start_cntr1, data->state->segment_start_cntr2);
    int64_t remaining_counts = cur_seg.distance_counts - seg_count_diff;
    long remaining_m = countsToCentimeters(remaining_counts, data->state->calibration) / 100;
    long travelled_m = countsToCentimeters(seg_count_diff, data->state->calibration) / 100;

    long next_seg_idx = data->state->segment_current_number + 1;
    bool near_end = (remaining_m >= 0 && remaining_m <= 500) &&
                    (next_seg_idx < static_cast<long>(data->state->segments.size()));
    bool near_start = (travelled_m >= 0 && travelled_m <= 500) &&
                      (data->state->segment_current_number > 0);

    // Same precedence as the TwinMaster button: "next" wins when both apply.
    if (near_end) out.next_enabled = true;
    else if (near_start) out.prev_enabled = true;
    return out;
}

// For a figure that is genuinely KPH whatever the meter is set to: the
// segment's target speed. calculateCurrentSpeed and calculateAverageSpeed
// already return MPH when state.units is set, so they must not come through
// here or the phone would show them converted twice.
static double displayKph(AppData* data, double kph) {
    if (data->state->units) return kph * 0.621371;
    return kph;
}

std::string toneCadenceJson(const ToneCadence& tone) {
    char buf[128];
    snprintf(buf, sizeof(buf),
        "{\"tone_ms\":%d,\"silence_ms\":%d,\"freq_hz\":%.2f,\"wave\":\"sine\"}",
        tone.tone_ms, tone.silence_ms, tone.freq_hz);
    return buf;
}

std::string buildTelemetryJson(AppData* data) {
    auto current_poll = data->poller->getMostRecent();
    auto tenth_poll = data->poller->get10th();
    int64_t current_time_ms = getRallyTime_ms(*data->state);

    int64_t trip_count_diff = calculateDistanceCounts(*data->state,
        current_poll.cntr1, current_poll.cntr2,
        data->state->trip_start_cntr1, data->state->trip_start_cntr2);
    long trip_m = countsToCentimeters(trip_count_diff, data->state->calibration) / 100;
    double trip_avg = calculateAverageSpeed(*data->state,
        data->state->trip_start_time_ms, current_time_ms, trip_count_diff);
    if (trip_count_diff < 0) trip_avg = 0.0;   // trip taken negative by a distance adjustment

    int64_t total_count_diff = calculateDistanceCounts(*data->state,
        current_poll.cntr1, current_poll.cntr2,
        data->state->total_start_cntr1, data->state->total_start_cntr2);
    long total_m = countsToCentimeters(total_count_diff, data->state->calibration) / 100;
    double total_avg = calculateAverageSpeed(*data->state,
        data->state->total_start_time_ms, current_time_ms, total_count_diff);

    double cur_speed = calculateCurrentSpeed(*data->state, current_poll, tenth_poll);
    if (cur_speed < 0) cur_speed = 0.0;

    double target_kph = 0.0;
    double ahead_behind_s = 0.0;
    if (data->state->segment_current_number >= 0 &&
        data->state->segment_current_number < static_cast<long>(data->state->segments.size())) {
        const Segment& seg = data->state->segments[data->state->segment_current_number];
        target_kph = countsPerHourToKPH(seg.target_speed_counts_per_hour, data->state->calibration);
        ahead_behind_s = calculateAheadBehindFromStageStart(*data->state, current_time_ms, total_count_diff);
        ahead_behind_s += data->state->ahead_behind_zero_offset_ms / 1000.0;
    }

    NextPrevState np = computeNextPrevState(data);
    std::string rally_clock = formatTime(current_time_ms);

    // Sized for every field below plus the tone object; snprintf would
    // truncate silently and hand the phone unparseable JSON.
    char buf[1536];
    snprintf(buf, sizeof(buf),
        "{\"type\":\"telemetry\","
        "\"rally_clock\":\"%s\","
        "\"trip_m\":%ld,"
        "\"total_m\":%ld,"
        "\"cur_kph\":%.1f,"
        "\"trip_avg_kph\":%.1f,"
        "\"total_avg_kph\":%.1f,"
        "\"target_kph\":%.1f,"
        "\"ahead_behind_s\":%.1f,"
        "\"segment_number\":%ld,"
        "\"segment_count\":%zu,"
        "\"next_enabled\":%s,"
        "\"prev_enabled\":%s,"
        "\"units\":\"%s\","
        // The ahead/behind tone the meter is playing right now, so the phone
        // plays the same one.
        "\"tone\":%s,"
        // No Beep Assist in this version; the client ignores a null beep.
        "\"beep\":null}",
        rally_clock.c_str(),
        trip_m,
        total_m,
        cur_speed,      // already in the meter's units
        trip_avg,       // already in the meter's units
        total_avg,      // already in the meter's units
        displayKph(data, target_kph),   // stored in KPH; convert
        ahead_behind_s,
        data->state->segment_current_number >= 0 ? data->state->segment_current_number + 1 : 0,
        data->state->segments.size(),
        np.next_enabled ? "true" : "false",
        np.prev_enabled ? "true" : "false",
        data->state->units ? "mph" : "kph",
        toneCadenceJson(data->currentTone).c_str());
    return buf;
}

std::string buildStateJson(AppData* data) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1);
    ss << "{\"type\":\"state\","
       << "\"segment_current_number\":" << data->state->segment_current_number << ","
       // Reset Total from the phone is applied at once, as the TwinMaster
       // Total button is, so the phone never needs the stage-running dialog.
       << "\"total_reset_asks\":false,"
       << "\"segments\":[";
    for (size_t i = 0; i < data->state->segments.size(); i++) {
        const Segment& seg = data->state->segments[i];
        if (i > 0) ss << ',';
        ss << "{\"target_speed_kph\":" << seg.target_speed_kph
           << ",\"distance_m\":" << static_cast<long>(seg.distance_m)
           << ",\"autoNext\":" << (seg.autoNext ? "true" : "false") << "}";
    }
    ss << "],";

    // The auto start line for the phone's stage panel.
    ss << "\"autostart\":\""
       << formatAutoStartStatus(data->state->auto_start_rally_time_minutes, getAutoStartEpochMs())
       << "\",";

    // Which memory slots hold a stage, so the phone marks them as the Stage
    // Setup screen does.
    ss << "\"memory_populated\":[";
    for (int i = 0; i < RallyState::MAX_MEMORY_SLOTS; i++) {
        if (i > 0) ss << ',';
        ss << (data->state->memory_slots[i].empty() ? "false" : "true");
    }
    ss << "]}";
    return ss.str();
}
