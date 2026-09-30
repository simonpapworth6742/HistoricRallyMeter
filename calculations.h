#ifndef CALCULATIONS_H
#define CALCULATIONS_H

#include <cstdint>
#include <string>
#include <vector>
#include "rally_state.h"
#include "rally_types.h"

// Calculate distance in counts (CNTR_A) since a start reading, including
// distance_offset_counts. Pass dont_apply_offset for a raw difference between
// two readings: the current speed and the calibration run.
int64_t calculateDistanceCounts(const RallyState& state, uint64_t cntr1, uint64_t cntr2,
                                  uint64_t start1, uint64_t start2,
                                  bool dont_apply_offset = false);

// The meter was off long enough for the counter chips to lose power, so both
// counts restarted from zero. Start total, trip and segment again from the live
// counts and the given time.
void restartDistancesAfterPowerLoss(RallyState& state, uint64_t cntr1, uint64_t cntr2,
                                    int64_t now_ms);

// Put trip_m at the front of a comma-separated history, keeping at most max_entries.
std::string pushTripHistory(const std::string& history, long trip_m, size_t max_entries = 6);

// Split a comma-separated history into metres, most recent first. Junk is skipped.
std::vector<long> parseTripHistory(const std::string& history);

// Reset the trip: record the trip distance as it reads now in the history,
// then start the trip again from the live counts. Every trip reset goes
// through here so the history is complete.
void resetTrip(RallyState& state, uint64_t cntr1, uint64_t cntr2, int64_t now_ms);

// Convert counts to centimeters using calibration (integer, for display)
long countsToCentimeters(int64_t counts, long calibration);

// Convert counts to metres (high precision, for segment distances)
double countsToMeters(double counts, long calibration);

// Convert metres to counts (high precision) - inverse of countsToMeters
double metersToCounts(double meters, long calibration);

// Adjust Distance Traveled entry. "+N"/"-N" add to or subtract from the current
// offset; "N" changes the offset so the total (current_total_counts, which
// already includes the offset) reads N metres; "0" zeroes the offset. Returns
// false for empty or non-numeric text, leaving new_offset_counts untouched.
bool applyDistanceAdjustmentText(const std::string& text, int64_t current_offset_counts,
                                 int64_t current_total_counts, long calibration,
                                 int64_t& new_offset_counts);

// Nudge the entry text by delta_m metres. Empty is treated as "+0"; a signed
// entry stays signed through zero; an unsigned entry stays unsigned and stops at 0.
std::string nudgeDistanceAdjustmentText(const std::string& text, long delta_m);

// Convert counts per hour to KPH (high precision)
double countsPerHourToKPH(double counts_per_hour, long calibration);

// Convert KPH to counts per hour (high precision)
double kphToCountsPerHour(double kph, long calibration);

// Get rally time (system time + offset)
int64_t getRallyTime_ms(const RallyState& state);

// Format time as HH:MM:SS
std::string formatTime(int64_t time_ms);

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

#endif // CALCULATIONS_H
