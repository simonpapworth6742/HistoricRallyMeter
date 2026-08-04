#include "simple_tone.h"
#include <cmath>

namespace {
constexpr double QUIET_BAND_S = 0.2;
constexpr double UPDATE_THRESHOLD_S = 0.2;
constexpr double MAX_ERROR_S = 30.0;
constexpr double MIN_STAGE_DISTANCE_M = 250.0;
constexpr double BEHIND_FREQ_HZ = 1046.50;  // C6 -- speed up
constexpr double AHEAD_FREQ_HZ = 1396.91;   // F6 -- slow down
}  // namespace

SimpleToneResult updateSimpleTone(SimpleToneState& state,
                                    double secondsAheadBehind,
                                    double stageDistanceMeters,
                                    bool pastStageEnd) {
    bool in_zone = (stageDistanceMeters >= MIN_STAGE_DISTANCE_M) && !pastStageEnd;
    if (!in_zone) {
        state.lastCommittedSeconds = 0.0;
        return {false, 0.0};
    }

    if (std::abs(secondsAheadBehind - state.lastCommittedSeconds) > UPDATE_THRESHOLD_S) {
        state.lastCommittedSeconds = secondsAheadBehind;
    }

    double committed_abs = std::abs(state.lastCommittedSeconds);
    if (committed_abs <= QUIET_BAND_S || committed_abs > MAX_ERROR_S) {
        return {false, 0.0};
    }

    double freq = (state.lastCommittedSeconds < 0.0) ? BEHIND_FREQ_HZ : AHEAD_FREQ_HZ;
    return {true, freq};
}
