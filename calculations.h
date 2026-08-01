#ifndef CALCULATIONS_H
#define CALCULATIONS_H

#include <cstdint>
#include <string>
#include "rally_state.h"
#include "rally_types.h"

// Calculate distance in counts
int64_t calculateDistanceCounts(const RallyState& state, uint64_t cntr1, uint64_t cntr2,
                                  uint64_t start1, uint64_t start2);

// Convert counts to meters using calibration (high precision)
double countsToMeters(int64_t counts, long calibration);

// Convert counts to centimeters using calibration
long countsToCentimeters(int64_t counts, long calibration);

// Convert counts per hour to KPH (high precision)
double countsPerHourToKPH(double counts_per_hour, long calibration);

// Convert KPH to counts per hour (high precision)
double kphToCountsPerHour(double kph, long calibration);

// Get rally time (system time + offset)
int64_t getRallyTime_ms(const RallyState& state);

// Format time as HH:MM:SS
std::string formatTime(int64_t time_ms);

// Format duration as HH:MM:SS
std::string formatDuration(int64_t duration_ms);

// Calculate current speed from 10-second rolling average
double calculateCurrentSpeed(const RallyState& state, const CounterPoll& current, 
                            const CounterPoll& tenth);

// Calculate average speed
double calculateAverageSpeed(const RallyState& state, int64_t start_time_ms, 
                            int64_t current_time_ms, int64_t count_diff);

// Calculate seconds ahead/behind target (high precision) - single segment
double calculateAheadBehind(const RallyState& state, int64_t current_time_ms,
                          int64_t segment_start_time, double target_counts_per_hour,
                          int64_t actual_counts);

// Calculate ideal counts from stage start accounting for all segment speeds
double calculateIdealCountsFromStageStart(const RallyState& state, int64_t elapsed_ms);

// Calculate seconds ahead/behind from stage start (accounts for all segments)
double calculateAheadBehindFromStageStart(const RallyState& state, int64_t current_time_ms,
                                          int64_t actual_counts_from_stage_start);

// Geometry of the gauge needle for a given ahead/behind reading. Pure
// arithmetic so the clamping and angle mapping can be unit-tested without a
// display. The needle is a constant-width bar the same width as the major
// ticks, so it reads as "which tick am I on" rather than "roughly this
// direction".
struct NeedleGeometry {
    double angle;      // radians, Cairo convention (3*PI/2 is straight up)
    double length;     // hub to tip
    double halfWidth;  // half the bar's width
};

NeedleGeometry computeNeedleGeometry(double seconds, double max_seconds, double radius);

// The gauge's effective sweep, in seconds: fixed at 3 while the reading is
// within it (the needle deflects normally, exactly like pristine's green
// scale), then tracks |seconds| exactly -- not a preset -- up to a cap of
// 30. Feeding this into computeNeedleGeometry() as max_seconds is what pins
// the needle horizontal for any reading past 3s: seconds/max_seconds is
// then always exactly +-1, with no separate clamp needed anywhere else. The
// tick count (one per second of this value) grows around the pinned needle
// instead of the needle continuing to sweep a fixed dial.
double gaugeEffectiveMaxSeconds(double seconds);

// Which zone the reading is in: 0 = green (|seconds| < 10, still deflecting
// or just pinned), 1 = amber (10 to 30), 2 = red (30 and beyond, capped).
// Drives the arc colour, the scale-end chevron count, and the digital box's
// mm:ss/decimal format -- pristine read a single persisted, debounced
// data->gaugeScale (0/1/2) for all three; this supplies the same numbering,
// computed fresh from the reading every frame instead.
int gaugeZone(double seconds);

// Arc colour for a zone (see gaugeZone), matching pristine's exact
// per-scale RGB triples.
struct GaugeArcColor { double r, g, b; };
GaugeArcColor gaugeArcColor(int zone);

// Numeral for a tick on the gauge, one tick per second of the current
// effective sweep. Empty for the zero tick, which the centre triangle marks
// instead.
std::string gaugeTickLabel(int index);

// True while tick numerals should be drawn at all: only in the green zone.
// Past that, the dial has grown past the point where labelling individual
// second-ticks is useful -- the arc colour is the at-a-glance signal
// instead, and a numeral next to a pinned needle would claim a precision
// the needle position no longer carries.
bool gaugeTickLabelsVisible(double seconds);

// Angle (Cairo convention, same as NeedleGeometry::angle) for the major tick
// labelled `index` seconds, given the gauge's current effective sweep
// max_val (see gaugeEffectiveMaxSeconds). Uses the same seconds/max_val
// mapping as computeNeedleGeometry() so a tick numeral "i" always sits at
// the angle a reading of exactly i seconds would put the needle at -- even
// when max_val itself is fractional (e.g. max_val = 3.9 while still
// deflecting within the inner zone). Do not divide by a truncated tick
// count instead of max_val here: that would place tick "i" at
// i/tick_count of the sweep, i.e. i * (max_val / tick_count) seconds, not
// i seconds, whenever max_val has a fractional remainder.
double gaugeTickAngle(int index, double max_val);

#endif // CALCULATIONS_H
