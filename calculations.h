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

// Format a distance in meters with thousands-separator commas. No unit
// suffix and no padding -- callers that need column alignment use GTK
// label width/xalign, not string padding.
std::string formatDistanceGrouped(long meters);

// Format a distance choosing meters or kilometers so the printed magnitude
// stays bounded (switches to km above +/-999,999 m). Writes "m" or "km"
// through *unit_out.
std::string formatDistanceAutoUnit(long meters, const char** unit_out);

// Format an elapsed interval as minutes:seconds, switching to hours:minutes
// once the minutes need more than four digits, and to "toolong" when even
// hours will not fit. Emits no padding -- callers align the column with GTK
// label width and xalign, so the result does not depend on which monospace
// font the platform resolves.
std::string formatElapsedInterval(int64_t total_secs);

#endif // CALCULATIONS_H
