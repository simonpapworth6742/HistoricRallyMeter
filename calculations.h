#ifndef CALCULATIONS_H
#define CALCULATIONS_H

#include <cstdint>
#include <string>
#include <vector>
#include <utility>
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

// Splits a ';'-separated list of numbers (as typed into the Stage Setup
// screen's speed/distance entries) into parsed doubles, skipping empty
// tokens (e.g. from a leading/trailing/doubled ';'). Order preserved.
std::vector<double> parseSemicolonList(const std::string& input);

// Pairs a parsed speed list with a parsed distance list for on_add_segment()
// (RB-SEG-03). If speeds has exactly one entry, it is broadcast to every
// distance (the original, still-supported single-speed/multi-distance use
// case). If speeds and distances have the same non-zero size, they are
// paired positionally (speed i with distance i) -- this is the fix: the
// previous code silently reused only the first parsed speed for every
// segment when multiple were entered. Any other combination (speeds empty,
// or counts differ and neither is 1) is rejected: `out` is cleared and
// false is returned, so the caller adds nothing rather than guessing.
bool buildSegmentSpeedDistancePairs(const std::vector<double>& speeds,
                                     const std::vector<double>& distances,
                                     std::vector<std::pair<double, double>>& out);

#endif // CALCULATIONS_H
