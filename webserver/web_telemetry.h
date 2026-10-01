#ifndef WEB_TELEMETRY_H
#define WEB_TELEMETRY_H

#include <string>

struct AppData;
struct ToneCadence;

// Whether the TwinMaster [next/prev] button would read "next" or "prev" right
// now (500 m window at segment end or start). Both false otherwise.
struct NextPrevState {
    bool next_enabled = false;
    bool prev_enabled = false;
};

NextPrevState computeNextPrevState(AppData* data);

// The "tone" telemetry object for a cadence: silence is tone_ms 0.
std::string toneCadenceJson(const ToneCadence& tone);

std::string buildTelemetryJson(AppData* data);
std::string buildStateJson(AppData* data);

#endif // WEB_TELEMETRY_H
