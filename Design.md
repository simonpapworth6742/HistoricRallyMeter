## Environment
This environment is a Raspberry Pi 5 with 4GB memory connected to 3 LSI ls7866c 32-bit counters (CNTR_1, CNTR_2, CNTR_3) on I2C bus 1 at addresses 0x70, 0x71 and 0x72 The 32-bit counter register 0x07 and its value is big-endian. the application should be 
    GTK3-based GUI window application with two display windows. 
    Use high-resolution chrono timers for accurate measurement
    C++  version 20

The application is ment to run on dual 400x1280 screens one connected to hdmi-A-1 and the other connected to DSI-2, however during development it has an hdmi 4k screen and a single 7inch (400x1280) screen connected to DSI-2. ensure that the co-pilots display window opens full screen on the screen connected to DSI-2 or if not found then it opens as a 400x1280 window, and that the drivers display remembers its start position,size and which display it is connected to.


## Build Requirements

- GTK3 development libraries (`libgtk-3-dev`)
- Standard C++ compiler with C++20 support
- Linux I2C device interface
- Clear structure with only one class per file
- Application excutable and name should be HistoricRallyMeter

# Source code control
The system should be stored in github simonpapworth6742/HistoricRallyMeter repo

## Application Overview


The purpose of the Historic Car Regulation Rally meter is to enable drivers and co-pilots of classic cars to win regulation rallies, where the classic car must be driven over a multi-segment stage at set speeds to within 0.1 seconds of the measured time over distances varying from 2 km to hundreds of kilometres. All distances shown are calculated with the current calibration.


              The system will maintain several global variables backed by a local json file, loading them into global variables on start up and updating them and the global variables in memory via the various screens, sometimes with a save option and sometimes automatically such as Total / Trip resets. Functions should have all required global variables passed as parameters to enable unit testing simply. If the json file is missing or corrupted the values default as defined below or to the current count / time depending on the type of variable.

              Global variables: -

Boolean units – false = KPH (default), true = MPH
long calibration = see calibration below, defaults to 600000.
Boolean counters – false = One gearbox 32 bit counter CNTR_1, True= two wheel 32 bit counters CNTR_1 & CNTR_2, when set the number of counts from the total_start and trip_start is the average of CNTR_1 and CNTR_2
ulong total_start_cntr1 - Total distance start count for CNTR_1
ulong total_start_cntr2 - Total distance start count for CNTR_2
time total_start_time – the time to at least the nearest ms that the total distance counters was last reset – defaults to now on startup if not present
ulong trip_start_cntr1 - Trip distance start count for CNTR_1
ulong trip_start_cntr2 - Trip distance start count for CNTR_2
time trip_start_time – the time to at least the nearest ms that the trip distance counters was last reset – defaults to now on startup if not present
ulong segment_start_cntr1 - segment distance start count for CNTR_1
ulong segment_start_cntr2 – segment distance start count for CNTR_2
time segment_start_time – the time to at least the nearest ms that the last segment was started
long segment_current_number – the current segment of the stage so defining the current target average speed displayed. On startup/empty segments, segment_current_number defaults to -1; blank Seg/target/±/ETA/next line on drivers display.
 
long rallyTimeOffset – ms offset of rally time to operating system time, defaults to 0
structure segment[]  - Stage segments contain target speed over distance segments of the stage, and if manual or automatic progression to the next segment is required. Defaults to no segments.
long target_speed – stored in number of counts per hour via the calibration.
long distance – number of counts for the segment
Boolean autoNext – True =  when the distance of this segment has been reached the next segment is started automatically, changing the segment_current value and segment_start counter as well as resetting the trip counter values, false = the next segment button on the co-pilots TwinMaster display must be pressed to advance to the next segment, setting the segment_current, segment_counters and Trip counters
 

The system has two counters available CNTR_1 and CNTR_2, when configured to use one gearbox counter then the distance since total, trip or segment  will be the value of the current CNTR_1 minus the total / trip / segment start_cntr1. If configured to two wheel then the distance since total, tip or segment will be ( (CNTR_1 - the total / trip / segment start_cntr1) + (CNTR_2 - the total / trip / segment start_cntr2) ) then divided by 2. Use integer maths.

Calibration, As the wheels / gearbox rotate and the car moves forward, the amount the car travels in meters per counter increment has to be set via calibration. It is expected that there could be as few as one counter increment per wheel revolution and as many as sixteen. To ensure accuracy the calibration will be stored as the number of millimetres travelled per 1000 counts. Meters travelled = (count_diff * calibration) / 1000 / 1000. Keeping the calibration as a larger number enables integer maths to calculate speeds and distance travelled in centimetres. When updating the calibration new_cal = (input_meters * 1000 * 1000) / total_count_diff (to match mm/1000 counts).

 

Calculation of speed and display of speed / target speed. Taking account of the formula for two wheel counting vs gearbox counting the difference in counted distance travelled from a (total / trip /segment start multiplied by the calibration) and divided by 10000 = centimetres travelled. centimetres travelled divided by the time taken since the start gives a speed, and allow for integer maths for all but the final conversion to KPH or MPH, all speeds will be calculated for display from the counts travelled / time taken and then converted to KPH or MPH. There are 100,000 cm in a kilometre and 160,934 cm in a Mile. if current speed is 100 km/h, switching to MHP will display on next update "62.14" mph

 

As the car is moving the counters will be polled regularly, but not more than every 5ms, if polled in less than 5ms the function will return the same result as last time. When a segment distance has been covered and autoNext is enabled the segment should move to the next segment, or subsequent segment if polling interval means more than one segment has been passed, if there is one.

Wherever the RallyClock is shown it is always the operating system time adjusted for the rallyTimeOffset by adding it. All times should be shown in 24 hour format.

The application will have two display windows, one for the driver and one for the co-pilot, each window is a separate desktop window, the pilot’s display window has a single screen, while the co-pilots display has several screens, the default being the TwinMaster screen.


calculation of current speed, with too little time passed since a start any speed calc will be too inaccurate to be useful, therefor within the polling loop of the counters a count should be remembered for about 2 seconds, by placing it into an array[10] of counter values and time polled, simply done on first poll the time of the poll and the value of the poll should be stored in the first position of the array. Each poll if more than 0.2 second has passed since the time in the first position of the array, then the array should be push down one, the last value lost, and the current value and time stored in the first array position. The 10th value of the array count and time should be available via class properties, to be used to calculate the current speed compared to the most recent poll, and not the value in the array position 1. If the time of the 10th position of the array is zero / empty / blank then the current speed should be shown as “--.—". Give a 20% time tolerance when checking the 10th position of the array contains a time over 80% of the total sampling period (i.e., age >= 1280ms, which is 80% of 2s minus 20% tolerance). 


Drivers display Window

The drivers display window, will show the average speed since the last reset of the Total, the current speed calculated from approximately the last 10 seconds of driving, and the average speed since the last Trip reset, and the average speed since the start of the current segment. The target speed for the current segment and how many seconds ahead or behind target average speed by calculating how many counts difference there is between the actual count now and the count that it should be based upon the time since segment start and the target speed for the segment. Along with the ETA = remaining segment distance / (last-10s average speed) to the next segment. If there is no current segment defined or more than 1000m past end of the last segment, then display “--.—” for Seg. For the next segment line of the display hide it if there is no next segment and if last-10s speed = 0: '--.—'; negative remaining: 'Over by xx:xx:xx'.

 

Seconds ahead behind formula: ideal_counts = (time_ms_since_segment / 3600000.0) * target_counts_h; diff = actual - ideal; seconds = diff / (target_counts_h / 3600.0). positive numbers means travelling to fast.

Updates per second is the number of times this display has been updated in a second, Rolling count of driver display render/update calls over the last full second.


Current   Trip          Seg.    Total   (<units>)

xx.xx        xx.xx      xx.xx     xx.xx

target xx.xx  +- xxxxx seconds

next segment speed xx.xx in xxx,xxx m after hh:mm:ss
updates per second:xxx                                                             kph / mph


make sure the <units> displays the currently selected units, and that the header row aligns with the measures below it. 

Co-Pilots display window

The co-pilot display window will be where the 1) stage is setup, 2) the calibration of the distance travelled per revolution of the wheels or gearbox box sensor, 3) and the normal default screen used, while the car is moving, by the co-pilot to display the TwinMaster display, 4) Date and Time setup screen to allow adjustment of the RallyClock time to actual local time from the operating system.


Stage setup this will allow target speed, distance and AutoNext for multiple segments of a rally stage to be setup (buttons are underlined and in bold)
Target avg. speed           Distance(m)     AutoNext

xx.xx                    xxx,xxx                Y/N                     delete

xx.xx                    xxx,xxx                Y/N                     delete

add more                                                                   back

              The target speed is in KPH and the distance is in meters, there is no adjust for units here yet.

Counts per hour = (input_kph * 1000 * 3600) / (cal / 1000) (m/h to counts/h, since cal is mm/1000 counts → m/count = cal/1e6).

Changes in calibration have no effect og these values.

 


 

2) Calibration, will use the Total distance travelled using the current calibration and allow for the RallyCalibration distance to be entered and saved. When calibration is changed the stage segments are not updated with the new calibration, and neither is any already displayed value but any update will use the new calibration. (buttons are underlined and in bold, inputs are in bold)

              Total distance xxx,xxx m (xxx,xxx,xxx count)

              Input Rally distance actually covered xxxxxx meters

              save                                                                             back

The minimum number of meters actually covered allowed is 500, the maximum is 100,000. The conversion to the calibration is as defined in the calibration section above.


3) - the normal co-pilot display. This is a TwinMaster meter distance travelled display one with two lines of current data and several buttons. (buttons are underlined and in bold)

                                                                                              hh:mm:ss
                             Total xxx,xxx m from hhh:mm:ss ago    reset

                             Trip   xxx,xxx m from hhh:mm:ss ago   reset

Segment xx – next segment in xxx,xxx m
segments      next segment               calibration                           


The top line of the display is distance from the last total distance reset, the second line the distance from the last trip reset. Next segment always displays the distance to the end of the current segment, negative if past the end. The hh:mm:ss at the top left is the RallyClock.

segments button goes to screen 1)

next segment button increments to the next segment, sets the target average speed and resets the Trip as well.

calibration button goes to the calibration screen

reset resets the total or Trip start_counters and sets the appropriate start_time

4) Date time setup screen allows the RallyClock offset to be calculated. (buttons are underlined and in bold, inputs are in bold)

 

              System Clock   yyyy/mm/dd      hh:mm:ss

              RallyClock        yyyy/mm/dd    hh:mm:ss

              set and save                                          back

## Unit Tests

### I2C Counter Tests
- Read 32-bit value from CNTR_1 at address 0x70, register 0x07
- Read 32-bit value from CNTR_2 at address 0x71, register 0x07
- Verify big-endian to host byte order conversion is correct
- Verify polling returns same value if called within 5ms
- Verify polling returns new value if called after 5ms

### Config File Tests
- Load config from valid JSON file with all fields present
- Load config from JSON file with missing fields (defaults applied)
- Load config when file does not exist (all defaults applied)
- Load config with segments array containing multiple segments
- Load config with empty segments array
- Save config and verify all fields written correctly
- Save config with multiple segments and verify JSON structure
- Verify calibration defaults to 600000 when missing
- Verify units defaults to false (KPH) when missing
- Verify segment_current_number defaults to -1 when missing/empty segments

### Distance Calculation Tests
- Single counter mode: distance = CNTR_1 - start_cntr1
- Dual counter mode: distance = ((CNTR_1 - start_cntr1) + (CNTR_2 - start_cntr2)) / 2
- Verify integer math (no floating point until final display)
- Distance in meters = (count_diff * calibration) / 1000 / 1000
- Distance in centimetres = (count_diff * calibration) / 10000
- Handle 32-bit counter wrap-around correctly

### Calibration Tests
- Default calibration value is 600000
- New calibration = (input_meters * 1000 * 1000) / total_count_diff
- Minimum input distance is 500 meters
- Maximum input distance is 100,000 meters
- Calibration change does not affect stored segment target speeds
- Calibration change affects all subsequent distance/speed calculations

### Speed Calculation Tests
- Speed in counts/hour from counts and time_ms
- Convert counts/hour to KPH: (counts_per_hour * calibration) / 1e9
- Convert counts/hour to MPH: KPH * 100000 / 160934
- 100 KPH displays as "62.14" MPH after unit switch
- Average speed since Total reset
- Average speed since Trip reset
- Average speed since Segment start
- Speed displays "--.--" when time elapsed is zero

### Current Speed Rolling Average Tests
- First poll stores value in array position 0
- Array shifts down when >200ms since position 0 timestamp
- Array does not shift if <200ms since position 0 timestamp
- 10th position (index 9) available for speed calculation
- get10th() returns {0,0,0} when position 9 time is zero
- get10th() returns {0,0,0} when age < 1280ms (80% of 2s with 20% tolerance)
- get10th() returns valid data when age >= 1280ms
- getMostRecent() returns actual latest I2C read, not array position 0
- Current speed = (most_recent - 10th) counts / time difference
- Display "--.--" when 10th position is invalid

### Segment Management Tests
- Add segment with target_speed, distance, autoNext
- Delete segment from list
- Segment target_speed stored as counts per hour
- Counts per hour = (input_kph * 1000 * 3600) / (calibration / 1000)
- AutoNext=true advances segment when distance reached
- AutoNext=false requires manual next segment button
- Skip multiple segments if polling interval causes overshoot
- No current segment (index -1) shows "--.--" for Seg speed
- Past end of last segment by >1000m shows "--.--" for Seg speed

### Ahead/Behind Calculation Tests
- ideal_counts = (time_ms_since_segment / 3600000.0) * target_counts_per_hour
- diff = actual_counts - ideal_counts
- seconds = diff / (target_counts_per_hour / 3600.0)
- Positive seconds means travelling too fast (ahead)
- Negative seconds means travelling too slow (behind)
- Display "+xxxxx" for ahead, "-xxxxx" for behind

### ETA Calculation Tests
- ETA = remaining_segment_distance / current_speed
- Display "--.--" when current speed is zero
- Display "Over by hh:mm:ss" when past segment end (negative remaining)
- Format ETA as hh:mm:ss

### RallyClock Tests
- RallyClock = system_time + rallyTimeOffset_ms
- Default rallyTimeOffset_ms is 0
- Set and save updates rallyTimeOffset_ms correctly
- RallyClock displays in 24-hour format (hh:mm:ss)

### Reset Functionality Tests
- Total reset sets total_start_cntr1 = current CNTR_1
- Total reset sets total_start_cntr2 = current CNTR_2
- Total reset sets total_start_time_ms = current time
- Trip reset sets trip_start_cntr1/cntr2 and trip_start_time_ms
- Next segment resets Trip counters and time
- Next segment increments segment_current_number
- Next segment sets segment_start counters and time

### Unit Toggle Tests
- Toggle units from KPH to MPH
- Toggle units from MPH to KPH
- All speed displays update on next refresh after toggle
- Header row shows current unit selection

### Display Update Tests
- Updates per second counts render calls in last full second
- Rolling count resets each second
- Driver display updates all speed values each refresh
- Co-pilot TwinMaster updates distance and time values

### Time Formatting Tests
- Format milliseconds as hh:mm:ss
- Format milliseconds as hhh:mm:ss for durations over 99 hours
- All times display in 24-hour format
- Rally clock displays current time with offset applied

### Stage Setup Screen Tests
- Input target speed in KPH and verify stored as counts per hour
- Input distance in meters and verify stored as counts
- Toggle autoNext Y/N and verify boolean stored correctly
- Add new segment to end of list
- Delete segment from middle of list
- Delete last segment from list
- Verify segment list displays all segments with correct values
- Back button returns to TwinMaster without losing unsaved changes

### Calibration Screen Tests
- Display total distance in meters using current calibration
- Display total count difference (raw counter value)
- Input validation rejects values below 500 meters
- Input validation rejects values above 100,000 meters
- Save button calculates and stores new calibration
- Back button returns without saving changes
- Verify calibration update does not modify existing segment target speeds

### TwinMaster Screen Tests
- Display Total distance in meters with time elapsed (hhh:mm:ss ago)
- Display Trip distance in meters with time elapsed (hhh:mm:ss ago)
- Display current segment number and distance to segment end
- Display negative distance when past segment end
- Total reset button updates counters and time, saves to JSON
- Trip reset button updates counters and time, saves to JSON
- Segments button navigates to Stage Setup screen
- Next segment button advances segment and resets Trip
- Calibration button navigates to Calibration screen
- RallyClock displays at top in hh:mm:ss format

### Date/Time Setup Screen Tests
- Display system clock in yyyy/mm/dd hh:mm:ss format
- Display RallyClock (with current offset) in yyyy/mm/dd hh:mm:ss format
- Input fields accept valid date and time values
- Calculate rallyTimeOffset = input_rally_time_ms - system_time_ms
- Set and save button stores offset and returns to TwinMaster
- Back button returns without saving changes

### Rally-Specific Edge Cases
- Zero counts (stationary vehicle) - speed displays as 0.00 or "--.--"
- High speeds (200+ KPH) calculated and displayed correctly
- Long stages (hundreds of kilometres) without precision loss
- Short segments (2km minimum) handled correctly
- Timing accuracy to 0.1 seconds for ahead/behind calculation
- Rapid segment transitions when autoNext enabled at high speed

### Error Handling Tests
- I2C read failure returns previous valid value or error state
- Invalid JSON field types use default values
- Negative distance values rejected or handled gracefully
- Negative speed values display as 0.00
- Empty segment array during active rally (segment_current_number >= 0)
- System time jumps forward handled (recalculate elapsed times)
- System time jumps backward handled (prevent negative durations)
- Counter overflow at 32-bit boundary (wrap-around handling)

