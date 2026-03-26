## Environment
This environment is a Raspberry Pi 5 with 4GB memory connected to 3 LSI ls7866c 32-bit counters (CNTR_1, CNTR_2) on I2C bus 1 at addresses 0x70 and 0x71 The 32-bit counter register 0x07 and its value is big-endian. the application should be 
    GTK3-based GUI window application with two display windows. 
    Use high-resolution chrono timers for accurate measurement
    C++  version 20

The application is meant to run on dual 1280x400 screens (wide and shallow), search for the two displays by matching the size. During development it has an HDMI 4K screen and two 7inch (1280x400) screen connected. The co-pilot display window opens fullscreen on DSI-2, or if not found opens as a 1280x400 window. The driver display remembers its size and which monitor it is on.

### Display Detection Implementation
The application reads DRM connector information from `/sys/class/drm/` to identify the DSI-2 connector. It then matches GDK monitors to connectors by resolution (1280x400 or 400x1280 depending on rotation).

**Co-pilot display:**
- Detects the the display by scanning DSI and then HDMI using the lowest value display for the co-pilot (dSI is lower than HDMI) and opens fullscreen on that monitor
- If DSI-2 not found, opens as a 1280x400 window

**Driver display:**
- Default size: 1280x400 (matching screen dimensions)
- Remembers window size and which monitor it was on in if there are not two 1280x400 displays
- Window position cannot be saved/restored on Wayland due to compositor security limitations
- Window is centered on the saved monitor at startup

### Preventing Inactive Window Dimming

By default, GTK3 applies a "backdrop" state to unfocused windows, which dims the content and can make text harder to read. Since this application uses two windows that need to be visible simultaneously, this dimming effect is undesirable.

**Solution: GTK CSS Override**

Create `~/.config/gtk-3.0/gtk.css`:
```css
/* Prevent GTK from changing unfocused (backdrop) window appearance */
/* Keep the same colors as when focused */

.background:backdrop {
    background-color: @theme_bg_color;
}

*:backdrop {
    color: @theme_fg_color;
    -gtk-icon-effect: none;
}

label:backdrop,
entry:backdrop,
button:backdrop {
    color: @theme_fg_color;
}
```

This CSS override tells GTK to use the normal theme colors (foreground and background) for windows in the backdrop state, rather than the dimmed variants. The change takes effect immediately for newly launched GTK3 applications.

An example file `gtk-example-gtk.css` is included in this project - copy it to `~/.config/gtk-3.0/gtk.css`.

**Optional: Labwc Titlebar Theme**

The labwc compositor also styles inactive window titlebars differently. To make them match active windows, create `~/.config/labwc/themerc-override` with settings that copy active window colors to inactive. After changing, run `labwc --reconfigure`.

### Debug Configuration
VS Code/Cursor debug configuration is provided in `.vscode/`:
- `launch.json` - Debug configurations for main app and unit tests
- `tasks.json` - Build tasks (debug, release, test, clean)
- `c_cpp_properties.json` - IntelliSense configuration

Use `make debug` to build with debug symbols (`-g -O0`), producing `HistoricRallyMeter_debug`.

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
double target_speed_kph - the actual speed requested for this segment, this does not change when the calibration changes
double target_speed – stored in number of counts per hour via the calibration (high precision floating point). This is recaculated when calibration changes
double distance_m - the actual distance in meter entered, this does not change when the calibration changes
double distance – number of counts for the segment (high precision floating point).This is recaculated when calibration changes
Boolean autoNext – True =  when the distance of this segment has been reached the next segment is started automatically, changing the segment_current value and segment_start counter as well as resetting the trip counter values, false = the next segment button on the co-pilots TwinMaster display must be pressed to advance to the next segment, setting the segment_current, segment_counters and Trip counters
 

The system has two counters available CNTR_1 and CNTR_2, when configured to use one gearbox counter then the distance since total, trip or segment  will be the value of the current CNTR_1 minus the total / trip / segment start_cntr1. If configured to two wheel then the distance since total, trip or segment will be ( (CNTR_1 - the total / trip / segment start_cntr1) + (CNTR_2 - the total / trip / segment start_cntr2) ) then divided by 2. Use integer maths, this caculated counter should be called CNTR_A

Calibration, As the wheels / gearbox rotate and the car moves forward, the amount the car travels in meters per counter increment has to be set via calibration. It is expected that there could be as few as one counter increment per wheel revolution and as many as sixteen. To ensure accuracy the calibration will be stored as the number of millimetres travelled per 1000 counts. Meters travelled = (count_diff * calibration) / 1000 / 1000. Keeping the calibration as a larger number enables integer maths to calculate speeds and distance travelled in centimetres. When updating the calibration new_cal = (input_meters * 1000 * 1000) / total_count_diff (to match mm/1000 counts).

 

Calculation of speed and display of speed / target speed. Taking account of the formula for two wheel counting vs gearbox counting the difference in counted distance travelled from a (total / trip /segment start multiplied by the calibration) and divided by 10000 = centimetres travelled. centimetres travelled divided by the time taken since the start gives a speed, and allow for integer maths for all but the final conversion to KPH or MPH, all speeds will be calculated for display from the counts travelled / time taken and then converted to KPH or MPH. There are 100,000 cm in a kilometre and 160,934 cm in a Mile. if current speed is 100 km/h, switching to MHP will display on next update "62.14" mph


As the car is moving the counters will be polled regularly, but not more than every 5ms, if polled in less than 5ms the function will return the same result as last time. When a segment distance has been covered and autoNext is enabled the segment should move to the next segment, or subsequent segment if polling interval means more than one segment has been passed, if there is one.

Wherever the RallyClock is shown it is always the operating system time adjusted for the rallyTimeOffset by adding it. All times should be shown in 24 hour format.

The application will have two display windows, one for the driver and one for the co-pilot, each window is a separate desktop window, the pilot’s display window has a single screen, while the co-pilots display has several screens, the default being the TwinMaster screen.


calculation of current speed, with too little time passed since a start any speed calc will be too inaccurate to be useful, therefor within the polling loop of the counters a count should be remembered for about 2 seconds, by placing it into an array[10] of counter values and time polled, simply done on first poll the time of the poll and the value of the poll should be stored in the first position of the array. Each poll if more than 0.2 second has passed since the time in the first position of the array, then the array should be push down one, the last value lost, and the current value and time stored in the first array position. The 10th value of the array count and time should be available via class properties, to be used to calculate the current speed compared to the most recent poll, and not the value in the array position 1. If the time of the 10th position of the array is zero / empty / blank then the current speed should be shown as “--.—". Give a 20% time tolerance when checking the 10th position of the array contains a time over 80% of the total sampling period (i.e., age >= 1280ms, which is 80% of 2s minus 20% tolerance). 



**_Drivers display Window (1280 x 400) - dark theme only_**

The drivers display window is wide (1280px) and shallow (400px). It shows the average speed since the last reset of the Total, the current speed calculated from approximately the last 10 seconds of driving, the average speed since the last Trip reset, and the average speed since the start of the current segment. The target speed for the current segment and how many seconds ahead or behind target average speed by calculating how many counts difference there is between the actual count now and the count that it should be based upon the time since stage start taking account of the differing speeds in segments already completed and the target speed for the current segment. Along with the ETA = remaining segment distance / (last-10s average speed) to the next segment. If there is no current segment defined or more than 1000m past end of the last segment, then display "--.--" for Seg. For the next segment line of the display hide it if there is no next segment and if last-10s speed = 0: '--.--'; negative remaining: 'Over by xx:xx:xx'.

Seconds ahead/behind formula (high precision): ideal_counts = (time_ms_since_segment / 3600000.0) * target_counts_h; diff = actual - ideal; seconds = diff / (target_counts_h / 3600.0). positive numbers means travelling too fast. All target speed and ETA calculations use high precision (double) floating point arithmetic throughout. If more than +- 0.1 ahead/behind then after the seconds ahead/behind value calculate the increase in speed needed (acceleration/deceleration) to exactly match the target in the next 500 meters. Use up to 3 green up arrows to indicate the requirement to speed up, and up to 3 red down arrows to show the requirement to slow down next to the Current Speed. If speed adjustment needed is less than 3 kph show one arrow, between 3 and 10 show two arrows, and more than 10 show 3 arrows. 

Indicate the change in acceleration required green / red arrows to the driver with sound as well as the visual indicators, if driving to +- 0.1 seconds ahead / behind or greater than +-30 seconds emit no tone. Using the same three acceleration brackets as the red / green arrows make 0.1 second tone with 0.1 second no tone, moving to 0.5/0.2 seconds and lastly 0.7/0.3 seconds.
The tones generated should be piano C6,F6,A6 when behind and C4,A3,F3 when ahead.The tone generator should apply a 5ms fade-in/fade-out envelope at every tone-to-silence and silence-to-tone transition

Updates per second is the number of times this display has been updated in a second, Rolling count of driver display render/update calls over the last full second.

**Rally Gauge Display:**
The ahead/behind timing is displayed as a 180-degree semicircular gauge (rally gauge style):
look at the example guage in gaugepilot-rallymaster-display.png
- Zero (on target) at the top center (12 o'clock position)
- adjust the scale on the guage based upon the current number of seconds you are ahead/ behind, have three scales +- 5 minutes (red), +-10 seconds (yellow), +- 3 seconds (green)
- The amount ahead/behind should be large white text withing a white outlined box.
- red should have a red semi circle on the guage, and the amount ahead/behind should be shown as +-hhh:mm:ss
- yellow should have a yellow semi circle on the guage, and the amount ahead/behind should be shown as +-ss.s
- green should have a green semi circle on the guage, and the amount ahead/behind should be shown as +-ss.s
- the scale on the guage should not change too often, and after changing should wait two seconds before changing again, in effect debouncing.
- A needle/indicator shows the current ahead/behind position
- The gauge provides an intuitive visual indication - needle pointing right means slow down, needle pointing left means speed up


```
+----------------------------------------------------------------------------------------------------------+
|   Current↑↑↑↓↓↓         Total                       |                 RALLY GAUGE                 [KPH]  |
|    xx.x                 xx.x                        |            -10s ←───┬───→ +10s                     |
|                                                     |                 ╱   │   ╲                          |
|   Target                Trip                        |               ╱     |    ╲                         |
|    xx.x                 xx.x                        |             ╱       │     ╲                        |
|                                                     |            ╱        ●       ╲                      |
|   fps: xxx  cpu: xxC   next: xx.xx in xxxm - mm:ss  |           ╱         │         ╲                    |
+----------------------------------------------------------------------------------------------------------+
```

Layout notes for 1280x400 (wide, shallow display):
- Left side: Four speed values Current, Target, Trip, Total. Trip, Total with large fonts, Current with extra large font and target 70% of the size of Trip.
- Right side: Rally gauge with semicircular dial, target speed, and timing info
- Bottom row: Updates counter on left, next segment info in center, unit toggle on right
- The rally gauge should be prominently displayed as a graphical element
- Use large fonts for speed values as they are primary information for driver
- All elements arranged to maximize visibility for the driver
- Total and Trip should vertially align
- The speed up /slow down arrows should not effect the Total label position and should not effect the Current label position
- The number of digits displayed for any of the values should not effect their position the decimal point should remain the in same place.

**_Co-Pilots display window (1280 x 400) - dark theme only_**

The co-pilot display window is wide (1280px) and shallow (400px), same as the driver display. It has four screens:
1) Stage setup
2) Calibration
3) TwinMaster display (default)
4) Date and Time setup

Layout notes for 1280x400 (wide, shallow display):
- All layouts use horizontal arrangement to maximize width
- Buttons arranged in rows across the bottom
- Information displayed in columns or horizontal sections

---

**1) Stage Setup Screen**

Allows target speed, distance and AutoNext for multiple segments of a rally stage to be setup. 

```
+----------------------------------------------------------------------------------------------------------+
|                                        STAGE SETUP                                                       |
+----------------------------------------------------------------------------------------------------------+
|  Speed(KPH)      Distance(m)      AutoNext            Mem Set   Recall                                   |
|    xx.xx          xxx,xxx           [Y] [del]             [1]    [1]                                     |
|    xx.xx          xxx,xxx           [Y] [del]             [2]    [2]                                     |
|    xx.xx          xxx,xxx           [Y] [del]             [3]    [3]                                     |
|    xx.xx          xxx,xxx           [Y] [del]             [4]    [4]                                     |
|    xx.xx          xxx,xxx           [Y] [del]             [5]    [5]                                     |
|    xx.xx          xxx,xxx           [Y] [del]                                                            |
|    xx.xx          xxx,xxx           [Y] [del]         [clear memory]                                     |
+----------------------------------------------------------------------------------------------------------+
|  New segment:  Speed [______] KPH    Distance [________] m    Auto [_]    [add]               [back]     |
+----------------------------------------------------------------------------------------------------------+
```
The exisiting segments should have editable values and scroll if there are more than 5 rows, The font should be 18px.
When editing any value a numeric entry keyboard should be shown on the right of the screen, buttons 72x58 pixels.
The New line at the bottom should have fonts 18px, entry boxes 130x40 pixels and buttons 80x40 pixels, there should be 100px paddingbefore the back button.

The target speed is in KPH and the distance is in meters.
Counts per hour = (input_kph * 1000 * 3600) / (cal / 1000)
Changes in calibration have no effect on stored segment values.

The memory storage allows for upto five stage setups to be remembered and then recalled on request, pressing the set button for the memeory number should copy the current segment setup into that memory position in the configuration file. pressing recall and a memory number should copy that memory position from the config file into the current setup and configuration, updating the display. Memory clear, after a conformation dialog box, should remove the memory sections from the configuration file.Buttons should be 66x43 pixels.

---

**2) Calibration Screen**

The start button should zero the counts and total distance covered values on the display, and remember the actual CNTR_1 and CNTR_2 values
The display should update the distances and counters every 10 ms while this screen is shown, but not when it is not displayed.
save button should update the stored calibration as defined above.
```
+----------------------------------------------------------------------------------------------------------+
|                                        CALIBRATION                                                       |
+----------------------------------------------------------------------------------------------------------+
|   Total distance: xxx,xxx m  (counts caluated :CNTR_A   1:CNTR_1   2:CNTR_2)                             |
+----------------------------------------------------------------------------------------------------------+
|   Actual distance covered:  [______________] meters       [reset to 1m per pulse]                        |
+----------------------------------------------------------------------------------------------------------+
|     [start]                         [save]                                                      [back]   |
+----------------------------------------------------------------------------------------------------------+
```

Min input: 500m, Max input: 100,000m.
new_cal = (input_meters * 1000 * 1000) / total_count_diff
When editing any value a numeric entry keyboard should be shown on the right of the screen, the same as the stage setup screen.
When [save] is pressed the new calibaration should be changed in the rally_config file as well as recaculating all target_speed and distance in the segments and memeory.
---

**3) TwinMaster Screen (Default)**

```
+----------------------------------------------------------------------------------------------------------+
| Segment xx  -  next segment in xxx,xxx m                                                   hh:mm:ss      |
+----------------------------------------------------------------------------------------------------------+
|                Total:  xxx,xxx m   from hhh:mm:ss.s ago   [reset]                                        |  48px font
+----------------------------------------------------------------------------------------------------------+
|                Trip:   xxx,xxx m   from hhh:mm:ss.s ago   [reset]                                        |  48px font
+----------------------------------------------------------------------------------------------------------+
|   [stage go]      [segments]       [next segment]       [calibration]       [date/time]                  | 18px font
+----------------------------------------------------------------------------------------------------------+
```

- RallyClock (hh:mm:ss) displayed at top right
- Total row displayed prominently with large font, distance and elapsed time, reset button after.
- Trip row below Total with same large font, distance and elapsed time, reset button after. 
- The Trip distance value should align with the total distace value, and the hhh:mm:ss should right align on the seconds, without large gaps and 
    the reset button should follow closely the ago.
- Segment info on top left
- Navigation buttons spread across bottom row:
  - stage go: resets Total (counters + start time), Trip (counters + start time), and Segment (counters + start time), and sets the drivers display guage to green - use at start of a rally stage
  - segments: goes to Stage Setup
  - next segment: advances segment, resets Trip
  - calibration: goes to Calibration screen
  - date/time: goes to Date/Time Setup screen
- reset buttons: reset respective counters and start time only

---

**4) Date/Time Setup Screen**

```
+----------------------------------------------------------------------------------------------------------+
|                                       DATE/TIME SETUP                                                    |
+----------------------------------------------------------------------------------------------------------+
|   System Clock:  yyyy/mm/dd  hh:mm:ss          |   Rally Clock:  yyyy/mm/dd  hh:mm:ss                    |
+----------------------------------------------------------------------------------------------------------+
|   Set Rally Time:    Date: [__________]    Time: [__________]                                            |
+----------------------------------------------------------------------------------------------------------+
|                                [set and save]                          [back]                            |
+----------------------------------------------------------------------------------------------------------+
```

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
- Segment target_speed_counts_per_hour and distance_counts saved as double with 6 decimal places
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
- Convert counts/hour to KPH: (counts_per_hour * calibration) / 1e9 (high precision double)
- Convert counts/hour to MPH: KPH * 100000 / 160934
- kphToCountsPerHour() returns double for high precision
- countsToMeters() returns double for high precision distance calculations
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
- Segment target_speed stored as counts per hour (double, high precision)
- Segment distance stored as counts (double, high precision)
- Config file saves segment values with 6 decimal places
- Counts per hour = kphToCountsPerHour(input_kph, calibration) - returns double
- AutoNext=true advances segment when distance reached
- AutoNext=false requires manual next segment button
- Skip multiple segments if polling interval causes overshoot
- No current segment (index -1) shows "--.--" for Seg speed
- Past end of last segment by >1000m shows "--.--" for Seg speed

### Ahead/Behind Calculation Tests
- All calculations use high precision (double) floating point
- ideal_counts = (time_ms_since_segment / 3600000.0) * target_counts_per_hour
- diff = actual_counts - ideal_counts
- seconds = diff / (target_counts_per_hour / 3600.0)
- Positive seconds means travelling too fast (ahead)
- Negative seconds means travelling too slow (behind)
- Display "+xxxxx" for ahead, "-xxxxx" for behind

### ETA Calculation Tests
- All calculations use high precision (double) floating point
- ETA = remaining_segment_distance / current_speed
- remaining_segment_distance calculated in meters using countsToMeters()
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
- Input target speed in KPH (decimal allowed) and verify stored as counts per hour (double)
- Input distance in meters (decimal allowed) and verify stored as counts (double)
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
- Long stages (hundreds of kilometres) without precision loss (high precision double throughout)
- Short segments (2km minimum) handled correctly
- Timing accuracy to 0.1 seconds for ahead/behind calculation (high precision)
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

