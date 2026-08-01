#ifndef CALCULATIONS_H
#define CALCULATIONS_H

#include <cstdint>
#include <string>
#include <vector>
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

// Limit a proposed manual distance correction so the corrected reading can
// never fall below zero. Clamping the correction rather than the displayed
// value matters: otherwise repeated downward presses bank an invisible debt
// that silently swallows the next stretch of real travel.
long clampDistanceAdjust(long raw_cm, long proposed_adjust_cm);

// Apply a manual distance correction (centimetres) to a raw distance
// (centimetres) and return whole metres, truncating the same way the
// uncorrected path does.
long adjustedDistanceMeters(long raw_cm, long adjust_cm);

// Heading for the co-pilot's next-segment row: the current segment's own
// target speed, with an arrow toward the change ahead. This is the one fact
// about the current segment the app can state honestly -- there is no
// reliable segment or stage number to show instead. Falls back to a
// placeholder when no segment is running. current_speed is already in the
// active display unit -- this function does not convert.
std::string segmentRowHeading(double current_speed, bool has_current_segment);

// Speed cell for that row: the speed coming up. Does not repeat the current
// speed -- that is already the row's heading -- so this answers "and then
// what", not "from what". Whole numbers -- roadbook speeds are called as
// whole figures. next_speed is already in the active display unit; is_mph
// only selects which unit word follows it.
std::string segmentSpeedTransition(double next_speed, bool has_next, bool is_mph);

// Parse an operator-entered Beep Assist waypoint list. Values are kilometres
// separated by commas, semicolons or whitespace, and are returned as metres
// in ascending order with duplicates removed. A malformed token is skipped
// rather than aborting the parse: one typo must not cost the whole list.
std::vector<double> parseBeepWaypointsKm(const std::string& text);

// Render waypoint metres back to the kilometre display form.
std::string formatBeepWaypointsKm(const std::vector<double>& waypoints_m);

// Index of the first waypoint not yet reached at the given travelled
// distance. Used to re-derive the runtime cursor after a restart, so a power
// blip mid-stage does not replay every waypoint already behind the car.
size_t beepCursorFor(const std::vector<double>& waypoints_m, double travelled_m);

// Ideal elapsed time, in seconds, to reach a target distance from the stage
// start, walking segments in order at each one's own target speed. Uses each
// segment's calibration-independent distance_m/target_speed_kph -- distance
// actually travelled never enters this calculation, only the roadbook's own
// numbers do. Returns -1 if the target lies beyond the distance the loaded
// segments cover.
double idealSecondsToReachDistance(const std::vector<Segment>& segments, double target_distance_m);

// Navigation mode due-check: true once the Total distance is within
// advance_m of the waypoint. Pure distance -- current speed plays no part.
bool navigationBeepDue(double waypoint_m, double travelled_m, double advance_m);

// Timing mode due-check: true once the scheduled time to reach the waypoint,
// less advance_s, has elapsed. Meaningless outside a stage; callers gate on
// that themselves (this function has no access to segment_current_number).
bool timingBeepDue(double waypoint_m, double elapsed_stage_s,
                   const std::vector<Segment>& segments, double advance_s);

// Index of the first waypoint due under either active mode, or -1 if none is
// due. Navigation and timing are independent triggers on the same waypoint
// list -- either firing is enough. With both on, whichever condition is met
// first in real time is naturally the one that fires, since this is polled
// continuously and the caller advances from_index past whatever fires.
long dueBeepWaypoint(const std::vector<double>& waypoints_m, size_t from_index,
                     double travelled_m,
                     bool navigation_mode, double advance_m,
                     bool timing_mode, double advance_s,
                     bool stage_active, double elapsed_stage_s,
                     const std::vector<Segment>& segments);

// Counts per kilometre implied by a calibration value (millimetres per 1000
// counts). This is the figure the operator compares against a known-good
// number, so the screen shows it directly rather than leaving it to be
// derived from the raw calibration. Returns 0 for a non-positive calibration
// rather than dividing by zero.
double pulsesPerKm(long calibration);

// The calibration screen's readout line: distance covered, the averaged pulse
// count, each sensor's own count -- so one wheel sensor disagreeing with the
// other is visible -- and the resulting pulses per kilometre.
std::string calibrationReadoutLine(long distance_m, int64_t counts_avg,
                                   int64_t counts_s1, int64_t counts_s2,
                                   long calibration);

// The calibration value (millimetres per 1000 counts) that gives an exact
// pulses/km figure. Identical arithmetic to pulsesPerKm() -- calibration and
// pulses/km are reciprocals of each other through the same 1e9 constant --
// named separately so each call site reads for what it means. Returns 0 for
// a non-positive input rather than dividing by zero.
long calibrationFromPulsesPerKm(double pulses_per_km);

#endif // CALCULATIONS_H
