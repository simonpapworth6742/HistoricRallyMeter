#ifndef SIMPLE_TONE_H
#define SIMPLE_TONE_H

// Runtime (non-persisted) state for the alternative time-error-only tone
// decision logic. Lives in AppData; reset each app run (default-constructed
// value is the correct "nothing latched yet" starting point).
struct SimpleToneState {
    double lastCommittedSeconds = 0.0;
};

struct SimpleToneResult {
    bool active;      // true => sound at freq_hz; false => silence
    double freq_hz;    // meaningful only when active
};

// Passed as ToneGenerator::setCadence()'s tone_ms when active, with
// silence_ms=0, to get a continuous tone out of the existing pulsed-cadence
// engine unmodified: the on/off toggle it runs internally never actually
// fires within a realistic session, so it reads as fully sustained.
constexpr int SIMPLE_TONE_SUSTAIN_MS = 3600000;  // 1 hour

// Decides whether the alternative ahead/behind tone should sound, and at
// what frequency, using only the raw time-error seconds -- no speed or
// recovery-distance calculation (contrast with the existing tone,
// ui_driver.cpp:578-650).
//
// Zone gating matches the existing tone: silent before 250m into the stage
// or once past the stage's last segment (stageDistanceMeters/pastStageEnd,
// computed by the caller exactly as the existing tone block already does).
//
// Within the zone, the module maintains one latched value,
// state.lastCommittedSeconds. Each call, if the new secondsAheadBehind
// differs from the latch by more than 0.2s, the latch moves to the new
// value; otherwise it is left untouched. All output (silence vs. tone,
// and which direction's frequency) is decided from the LATCH, not the raw
// input -- so small jitter around a boundary (the +/-0.2s quiet band, or
// the +/-30s giving-up point) does not chatter the tone on and off; only a
// genuine >0.2s move does. This is what makes the tone "constant": once
// latched, nothing changes until the error has moved a clear amount.
//
// Leaving the zone resets the latch to 0.0, so re-entering (e.g. a new
// stage) starts fresh rather than comparing against a stale value from the
// previous stage.
SimpleToneResult updateSimpleTone(SimpleToneState& state,
                                    double secondsAheadBehind,
                                    double stageDistanceMeters,
                                    bool pastStageEnd);

#endif // SIMPLE_TONE_H
