#ifndef WEB_TELEMETRY_H
#define WEB_TELEMETRY_H

#include <string>

struct AppData;

struct NextPrevState {
    const char* label = "--->";
    bool enabled = false;
};

NextPrevState computeNextPrevState(AppData* data);

std::string buildTelemetryJson(AppData* data);
std::string buildStateJson(AppData* data);

#endif // WEB_TELEMETRY_H
