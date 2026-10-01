#include "web_commands.h"
#include "config_file.h"
#include "calculations.h"
#include "callbacks.h"
#include "rally_types.h"
#include "rally_state.h"
#include "counter_poller.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

static const char* jsonFindString(const char* json, const char* key, char* out, size_t out_len) {
    std::string needle = std::string("\"") + key + "\":\"";
    const char* p = strstr(json, needle.c_str());
    if (!p) return nullptr;
    p += needle.size();
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < out_len) out[i++] = *p++;
    out[i] = '\0';
    return out;
}

static bool jsonFindBool(const char* json, const char* key, bool* out) {
    std::string needle = std::string("\"") + key + "\":";
    const char* p = strstr(json, needle.c_str());
    if (!p) return false;
    p += needle.size();
    while (*p && isspace(static_cast<unsigned char>(*p))) p++;
    if (strncmp(p, "true", 4) == 0) { *out = true; return true; }
    if (strncmp(p, "false", 5) == 0) { *out = false; return true; }
    return false;
}

static bool jsonFindInt(const char* json, const char* key, int* out) {
    std::string needle = std::string("\"") + key + "\":";
    const char* p = strstr(json, needle.c_str());
    if (!p) return false;
    p += needle.size();
    *out = static_cast<int>(strtol(p, nullptr, 10));
    return true;
}

static bool jsonFindDouble(const char* json, const char* key, double* out) {
    std::string needle = std::string("\"") + key + "\":";
    const char* p = strstr(json, needle.c_str());
    if (!p) return false;
    p += needle.size();
    *out = strtod(p, nullptr);
    return true;
}

// The total as the displays show it: counts from the total start including
// the distance offset.
static int64_t currentTotalCounts(AppData* data) {
    auto current_poll = data->poller->getMostRecent();
    return calculateDistanceCounts(*data->state,
        current_poll.cntr1, current_poll.cntr2,
        data->state->total_start_cntr1, data->state->total_start_cntr2);
}

// The phone's Adjust row goes through the same arithmetic as the Adjust
// Distance Traveled screen: "+10"/"-10" for the nudges, "N" for set.
static bool applyDistanceText(AppData* data, const std::string& text) {
    int64_t new_offset = data->state->distance_offset_counts;
    if (!applyDistanceAdjustmentText(text, data->state->distance_offset_counts,
                                     currentTotalCounts(data),
                                     data->state->calibration, new_offset)) {
        return false;
    }
    data->state->distance_offset_counts = new_offset;
    ConfigFile::save(*data->state);
    return true;
}

bool webHandleCommand(AppData* data, const char* json) {
    if (!data || !json) return false;

    char type[64];
    if (!jsonFindString(json, "type", type, sizeof(type))) return false;

    if (strcmp(type, "reset_trip") == 0) {
        on_trip_reset(nullptr, data);
        return true;
    }
    if (strcmp(type, "reset_total") == 0) {
        // Applied at once, as the TwinMaster Total button is. The client's
        // "choice" field (other builds ask about a running stage) is ignored.
        on_total_reset(nullptr, data);
        return true;
    }
    if (strcmp(type, "reset_total_capture") == 0 || strcmp(type, "reset_total_cancel") == 0 ||
        strcmp(type, "beep_set") == 0 || strcmp(type, "tone_set") == 0) {
        return false;   // sent by other builds of the client; nothing to do here
    }
    if (strcmp(type, "next") == 0 || strcmp(type, "prev") == 0) {
        // Both run the TwinMaster [next/prev] logic, which picks next or prev
        // from the 500 m window the vehicle is in. The phone only enables the
        // button matching that window, so the right correction is applied.
        on_next_prev_segment(nullptr, data);
        return true;
    }
    if (strcmp(type, "distance_adjust") == 0) {
        int delta_m = 0;
        if (!jsonFindInt(json, "delta_m", &delta_m)) return false;
        if (delta_m == 0 || delta_m < -1000 || delta_m > 1000) return false;
        char text[16];
        snprintf(text, sizeof(text), "%+d", delta_m);
        return applyDistanceText(data, text);
    }
    if (strcmp(type, "distance_set") == 0) {
        double meters = 0.0;
        if (!jsonFindDouble(json, "meters", &meters)) return false;
        if (!(meters >= 0.0 && meters <= 999999.0)) return false;
        char text[16];
        snprintf(text, sizeof(text), "%ld", static_cast<long>(meters + 0.5));
        return applyDistanceText(data, text);
    }
    if (strcmp(type, "segment_set") == 0) {
        int index = 0;
        double kph = 0, meters = 0;
        bool autoNext = true;
        if (!jsonFindInt(json, "index", &index)) return false;
        if (!jsonFindDouble(json, "target_speed_kph", &kph)) return false;
        if (!jsonFindDouble(json, "distance_m", &meters)) return false;
        jsonFindBool(json, "autoNext", &autoNext);
        if (index < 0 || index >= static_cast<int>(data->state->segments.size())) return false;
        if (kph <= 0 || meters <= 0) return false;

        Segment& seg = data->state->segments[index];
        seg.target_speed_kph = kph;
        seg.target_speed_counts_per_hour = kphToCountsPerHour(kph, data->state->calibration);
        seg.distance_m = meters;
        seg.distance_counts = metersToCounts(meters, data->state->calibration);
        seg.autoNext = autoNext;
        ConfigFile::save(*data->state);
        refreshSegmentList(data);
        return true;
    }
    if (strcmp(type, "segment_add") == 0) {
        double kph = 0, meters = 0;
        bool autoNext = true;
        if (!jsonFindDouble(json, "target_speed_kph", &kph)) return false;
        if (!jsonFindDouble(json, "distance_m", &meters)) return false;
        jsonFindBool(json, "autoNext", &autoNext);
        if (kph <= 0 || meters <= 0) return false;

        Segment seg;
        seg.target_speed_kph = kph;
        seg.target_speed_counts_per_hour = kphToCountsPerHour(kph, data->state->calibration);
        seg.distance_m = meters;
        seg.distance_counts = metersToCounts(meters, data->state->calibration);
        seg.autoNext = autoNext;
        data->state->segments.push_back(seg);
        ConfigFile::save(*data->state);
        refreshSegmentList(data);
        return true;
    }
    if (strcmp(type, "segment_delete") == 0) {
        int index = 0;
        if (!jsonFindInt(json, "index", &index)) return false;
        if (index < 0 || index >= static_cast<int>(data->state->segments.size())) return false;
        data->state->segments.erase(data->state->segments.begin() + index);
        if (data->state->segment_current_number >= static_cast<long>(data->state->segments.size())) {
            data->state->segment_current_number =
                data->state->segments.empty() ? -1 : static_cast<long>(data->state->segments.size()) - 1;
        }
        ConfigFile::save(*data->state);
        refreshSegmentList(data);
        return true;
    }
    if (strcmp(type, "memory_store") == 0) {
        int slot = 0;
        if (!jsonFindInt(json, "slot", &slot)) return false;
        if (slot < 1 || slot > RallyState::MAX_MEMORY_SLOTS) return false;
        data->state->memory_slots[slot - 1] = data->state->segments;
        ConfigFile::save(*data->state);
        return true;
    }
    if (strcmp(type, "memory_recall") == 0) {
        int slot = 0;
        if (!jsonFindInt(json, "slot", &slot)) return false;
        if (slot < 1 || slot > RallyState::MAX_MEMORY_SLOTS) return false;
        if (data->state->memory_slots[slot - 1].empty()) return false;
        data->state->segments = data->state->memory_slots[slot - 1];
        data->state->segment_current_number = data->state->segments.empty() ? -1 : 0;
        ConfigFile::save(*data->state);
        refreshSegmentList(data);
        return true;
    }
    return false;
}
