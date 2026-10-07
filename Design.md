## Environment
This environment is a Raspberry Pi 5 with 4GB memory connected to 2 LSI ls7866c 32-bit counters (CNTR_1, CNTR_2) on I2C bus 1 at addresses 0x70 and 0x71 The 32-bit counter register 0x07 and its value is big-endian. the application should be 
    GTK3-based GUI window application with two display windows. 
    Use high-resolution chrono timers for accurate measurement
    C++  version 20

## Displays

The application presents two windows, each intended for its own in-car display panel:

| Window | Purpose | Required resolution |
|---|---|---|
| **Co-pilot display** | TwinMaster, segment setup, calibration, date/time screens | Exactly 1280x400 (or 400x1280 rotated) |
| **Driver display** | Speed gauge and stage readouts | Exactly 800x480 (or 480x800 rotated). Never a 1280x400 or 400x1280 panel |

Definitions used throughout this section:
- **Small display**: a monitor whose resolution is exactly 1280x400, 400x1280, 800x480, or 480x800.
- **Co-pilot-capable display**: a small display of exactly 1280x400 (or 400x1280). The co-pilot window never uses an 800x480 panel.
- **Driver display**: a small display of exactly 800x480 (or 480x800). The driver window never uses a 1280x400 or 400x1280 panel.
- **Development setup**: an HDMI 2K/4K desktop monitor plus one or two small displays. When two small displays are attached the application must use them for the two windows and leave the desktop monitor free.

### Display Detection

1. Read DRM connector information from `/sys/class/drm/` (entries such as `card1-DSI-2`) to obtain each connector's name, connection status, and mode resolution.
2. Match each GDK monitor to a DRM connector by resolution.
3. Collect all small displays and sort them by connector priority: **DSI before HDMI, then by connector number, lowest first** (e.g. DSI-1, DSI-2, HDMI-A-1, HDMI-A-2). A monitor that cannot be matched to a connector sorts last.

### Display Assignment

Assignment happens at startup, in this order:

0. **Single-display mode**: if exactly one monitor exists and it is 1280x400, only the co-pilot window is shown, fullscreen on that monitor. The driver window is not shown at all. In this mode the TwinMaster screen's right-hand alarm panel is replaced by the embedded compact driver display (see "TwinMaster Screen - Single-Display Mode"). The remaining rules do not apply.
1. **Co-pilot window** takes the highest-priority **co-pilot-capable** display and opens fullscreen on it.
   - Fallback: if no co-pilot-capable display exists, it opens as a normal 1280x400 window (typically on the desktop monitor during development).
2. **Driver window** takes the highest-priority remaining 800x480 (or 480x800) display, never a 1280x400 or 400x1280 panel and never the one assigned to the co-pilot, and opens fullscreen on it.
   - Fallback: if no 800x480 display remains, it opens as an 800x480 window using the size remembered from the previous run (default 800x480), centered on the remembered monitor. A remembered size from the old wide layout (1280x400 or 400x1280, or any window at least 2.2 times as wide as it is tall) is discarded and 800x480 is used instead. If the remembered monitor no longer exists or is the co-pilot's monitor, the first other available monitor is used.

### Window Persistence (driver window fallback only)

- The driver window's size and monitor index are saved to the config file when the window is moved/resized and on shutdown.
- Window *position* cannot be saved/restored on Wayland due to compositor security limitations, hence centering on the saved monitor at startup.
- Nothing is persisted when both windows are fullscreen on small displays.

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

## User manual

A short user manual in Word format (`manual/HistoricRallyMeter-UserManual.docx`) describes each screen and dialog with a screenshot. It is generated by `manual/build_manual.py` (python3-docx) from the screenshots in `manual/screenshots/`, and reads the version from `version.txt`. Screenshots are taken with `grim -o DSI-2` for the co-pilot panel and `grim -o <driver output>` for the driver display; the phone web client shots come from a browser at phone width. When a screen changes, retake its screenshot and run the script again.

## Application Overview


The purpose of the Historic Car Regulation Rally meter is to enable drivers and co-pilots of classic cars to win regulation rallies, where the classic car must be driven over a multi-segment stage at set speeds to within 0.1 seconds of the measured time over distances varying from 2 km to hundreds of kilometres. All distances shown are calculated with the current calibration.


              The system will maintain several global variables backed by a local json file, loading them into global variables on start up and updating them and the global variables in memory via the various screens, sometimes with a save option and sometimes automatically such as Total / Trip resets. Functions should have all required global variables passed as parameters to enable unit testing simply. If the json file is missing or corrupted the values default as defined below or to the current count / time depending on the type of variable.

              Global variables: -

Boolean units – false = KPH (default), true = MPH
Boolean arrival_tone_enabled – true = play arrival.wav once when 500 m from the end of the current segment (see Segment arrival tone), default false. Set on the Setup screen.
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
long segment_current_number – the current segment of the stage so defining the current target average speed displayed. On startup/empty segments, segment_current_number defaults to -1; blank target and ahead/behind on the drivers display.
 
long rallyTimeOffset – ms offset of rally time to operating system time, defaults to 0
string bluetooth_audio_name – display name of the remembered Bluetooth speaker, empty when none is remembered
string bluetooth_audio_address – Bluetooth address of that speaker, empty when none is remembered. Pressing Remember bluetooth audio fills both from the Pi's current audio output when that output is a Bluetooth device, and clears both when it is not.
long ahead_behind_zero_offset_ms - ms offset (+ or -) of the drivers ahead/behind caculation,defaults to zero and on stage-go. Set in the twinmaster display.
long distance_offset_counts - counts (+ or -) added to the calculated counter CNTR_A for every distance measured since a start reading: total, trip and segment. A missed turn and the drive back are removed by making it more negative by that many counts. Defaults to zero, and is set to zero by a Total reset, stage go, and the counter power-loss reset. Set on the Adjust Distance Traveled screen.
string trip_history_m - the last six trip distances, in whole metres, recorded each time the trip is reset by any means (Trip button, stage go, next/prev, autoNext, the web Reset Trip command). Stored as a comma-separated string with the most recent first, e.g. "1234,5678,910"; a new reset is put at the front and the seventh oldest is dropped. Defaults to empty. The trip is recorded as it read at the moment of the reset, so it can be negative after a negative distance adjustment. The counter power-loss reset does not record anything, because the trip distance is not known at that point.
ulong auto_start_stage_at_rally_time - the date time in munites the stage should automatially be started in rally time, stored as an offset from 1/1/2020.
structure segment[]  - Stage segments contain target speed over distance segments of the stage, and if manual or automatic progression to the next segment is required. Defaults to no segments.
double target_speed_kph - the actual speed requested for this segment, this does not change when the calibration changes
double target_speed – stored in number of counts per hour via the calibration (high precision floating point). This is recaculated when calibration changes
double distance_m - the actual distance in meter entered, this does not change when the calibration changes
double distance – number of counts for the segment (high precision floating point).This is recaculated when calibration changes
Boolean autoNext – True =  when the distance of this segment has been reached the next segment is started automatically, changing the segment_current value and segment_start counter as well as resetting the trip counter values, false = the next segment button on the co-pilots TwinMaster display must be pressed to advance to the next segment, setting the segment_current, segment_counters and Trip counters
 

The system has two counters available CNTR_1 and CNTR_2, when configured to use one gearbox counter then the distance since total, trip or segment  will be the value of the current CNTR_1 minus the total / trip / segment start_cntr1. If configured to two wheel then the distance since total, trip or segment will be ( (CNTR_1 - the total / trip / segment start_cntr1) + (CNTR_2 - the total / trip / segment start_cntr2) ) then divided by 2. Use integer maths, this caculated counter should be called CNTR_A. The distance_offset_counts is then added to CNTR_A, so total, trip and segment, and every value derived from them (average speeds, ahead/behind, the tones, the alarm countdown and the web telemetry) all move together when it changes.

Two measurements bypass the offset, using a dont_apply_offset argument on the same calculation: the current speed, which is the difference between two counter readings about two seconds apart, and the calibration run, which is a raw pulse count over a measured distance. Neither is a distance since a start reading.

Because the offset is part of CNTR_A, a Trip reset and a segment start (next/prev, autoNext) record their start readings as the live count plus the offset, so the trip or segment reads zero at that moment. A Total reset sets the offset to zero and records the live count as the total start; the offset is removed from the trip and segment start readings at the same time so those two distances do not jump. Stage go sets the offset to zero and restarts all three from the live counts. After a Trip reset, a later negative adjustment takes the trip negative; it counts back up through zero as the car moves. The TwinMaster and the web show a negative trip with a leading minus, and show "--.--" for the trip average speed while the trip is negative.

The counter chips are simple pulse counters. The application does not change their count mode.

The counter chips stay powered for several minutes after the Pi has shut down, so they cannot lose power while the application is running. Each chip has a power-loss flag that is set only when the whole rally meter has been off for longer than that, in which case both counts have gone back to zero and the stored start readings no longer describe them. On startup the application reads the flag on both chips. The first I2C read of a chip after a cold boot has been seen to return 0x00 for the status register, with every later read correct, so the application reads the status register twice on each chip and uses the second reading (the first is discarded). Then:

- Neither flag set: the counts are continuous with the stored start readings. Leave total, trip and segment as they are, so distance covered while only the Pi was down stays included.
- Either flag set: the meter has been off. Set the total, trip and segment start readings on both chips to the live counts, set the three start times to now, set segment_current_number to -1, clear the distance alarm, save the config once, and clear the flag on both chips.

The application does not store the last count it saw, and does not write the config file on a timer or while the car is moving. The config is written only when a setting, reset or segment change is made, on the power-loss reset above, and on a clean shutdown. Each write goes to a temporary file in the same directory that is then renamed over rally_config.json, so a Pi power cut during a write cannot leave a truncated config.

Calibration, As the wheels / gearbox rotate and the car moves forward, the amount the car travels in meters per counter increment has to be set via calibration. It is expected that there could be as few as one counter increment per wheel revolution and as many as sixteen. To ensure accuracy the calibration will be stored as the number of millimetres travelled per 1000 counts. Meters travelled = (count_diff * calibration) / 1000 / 1000. Keeping the calibration as a larger number enables integer maths to calculate speeds and distance travelled in centimetres. When updating the calibration new_cal = (input_meters * 1000 * 1000) / total_count_diff (to match mm/1000 counts).

 

Calculation of speed and display of speed / target speed. Taking account of the formula for two wheel counting vs gearbox counting the difference in counted distance travelled from a (total / trip /segment start multiplied by the calibration) and divided by 10000 = centimetres travelled. centimetres travelled divided by the time taken since the start gives a speed, and allow for integer maths for all but the final conversion to KPH or MPH, all speeds will be calculated for display from the counts travelled / time taken and then converted to KPH or MPH. There are 100,000 cm in a kilometre and 160,934 cm in a Mile. if current speed is 100 km/h, switching to MHP will display on next update "62.14" mph


As the car is moving the counters will be polled regularly, but not more than every 5ms, if polled in less than 5ms the function will return the same result as last time. When a segment distance has been covered and autoNext is enabled the segment should move to the next segment, or subsequent segment if polling interval means more than one segment has been passed, if there is one.

Wherever the RallyClock is shown it is always the operating system time adjusted for the rallyTimeOffset by adding it. All times should be shown in 24 hour format.

The application will have two display windows, one for the driver and one for the co-pilot, each window is a separate desktop window, the pilot’s display window has a single screen, while the co-pilots display has several screens, the default being the TwinMaster screen.


calculation of current speed, with too little time passed since a start any speed calc will be too inaccurate to be useful, therefor within the polling loop of the counters a count should be remembered for about 2 seconds, by placing it into an array[10] of counter values and time polled, simply done on first poll the time of the poll and the value of the poll should be stored in the first position of the array. Each poll if more than 0.2 second has passed since the time in the first position of the array, then the array should be push down one, the last value lost, and the current value and time stored in the first array position. The 10th value of the array count and time should be available via class properties, to be used to calculate the current speed compared to the most recent poll, and not the value in the array position 1. If the time of the 10th position of the array is zero / empty / blank then the current speed should be shown as “--.—". Give a 20% time tolerance when checking the 10th position of the array contains a time over 80% of the total sampling period (i.e., age >= 1280ms, which is 80% of 2s minus 20% tolerance). 

when any button is pressed make a "soft beep" sound for feedback

**_Drivers display Window (800 x 480) - dark theme only_**

The drivers display window is 800x480 (or 480x800 when the panel is rotated). It is never placed on a 1280x400 or 400x1280 panel. It shows the average speed since the last reset of the Total, the current speed calculated from approximately the last 10 seconds of driving, the average speed since the last Trip reset, and the average speed since the start of the current segment. The target speed for the current segment and how many seconds ahead or behind target average speed by calculating how many counts difference there is between the actual count now and the count that it should be based upon the time since stage start taking account of the differing speeds in segments already completed and the target speed for the current segment, there is also an ahead_behind_zero_offset_ms value which is a simple addtion to the actual ahead/behind value. If there is no current segment defined or more than 1000m past end of the last segment, then display "--.--" for Seg.

Seconds ahead/behind formula (high precision): ideal_counts = (time_ms_since_segment / 3600000.0) * target_counts_h; diff = actual - ideal; seconds = diff / (target_counts_h / 3600.0). positive numbers means travelling too fast. All target speed calculations use high precision (double) floating point arithmetic throughout. If more than +- 0.1 ahead/behind then after the seconds ahead/behind value calculate the increase in speed needed (acceleration/deceleration) to exactly match the target in the next 500 meters. Less than 3 kph is one step, between 3 and 10 is two steps, and more than 10 is three steps.

Indicate that speed change with sound while within the stage. If driving to +- 0.1 seconds ahead / behind or greater than +-30 seconds emit no tone. Within the stage, one step is a 0.1 second tone with 0.1 second silence, two steps are 0.5/0.2 seconds, and three steps are 0.7/0.3 seconds.
The tones generated should be piano C6,C6,C6 when behind and F6,F6,F6 when ahead.The tone generator should apply a 5ms fade-in/fade-out envelope at every tone-to-silence and silence-to-tone transition.
The tones should sound from the stage start while within the stage. Once past the end of the last segment the tones should stop. While the TwinMaster mute button is muted, these ahead/behind tones do not sound. Button beeps and the distance alarm still sound. The mute is not stored. Stage go starts unmuted.

Updates per second is the number of times this display has been updated in a second, Rolling count of driver display render/update calls over the last full second.

If auto_start_stage_at_rally_time relative to rally time is in the future by less than 24 hours, then overlayed on the drivers display in 30px a count down clock "T- hh:mm:ss" with a thick white border, when zero seconds is reached the "stage go" rountine must be triggered once, as if the co-piliot had pressed and confirmed "stage go", and the stored auto start time is cleared. It must not start the stage again on the following updates. Ahead/behind tones then follow the normal stage-go rules from that start.

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
- The needle is a narrow triangle (white filled) tapering from a 24px-wide base at the hub to a sharp point at the tip, with a black 1.5px centre line running from hub to tip, and a subtle drop shadow offset by 2px.
  The Needle is used to convey a lot of information to the driver, both the scale and the distance to the end of the this segment. When in green scale there shoud be a signal arrow "^" pointing along the needle 48px-wide, 20% of the needle length from the central pin this is known as the topmost arrow. If in the yellow scale there should be a second arrow under the topmost arrow and if the red scale a third arrow under the second.
  If within a segment there should be a small perpedicular line 20% of the needles length from the top of the needle 48px wide, the "arrows" for the scale should move closer to the line based on the percentage of the current segment driven or stay at their start position if not in a segment.
- The digital readout box (ahead/behind value) is positioned just below the needle hub
- The gauge provides an intuitive visual indication - needle pointing right means slow down, needle pointing left means speed up


``` Layout notes for the driver display (800x480, or 480x800 rotated):
+-----------------------------------------+
|  {current}                   {trip dist}|
|         -10s ←───┬───→ +10s             |
|            ╱     │     ╲                |
|          ╱       │ {tot} ╲              |
|        ╱         │         ╲            |
|      ╱ {target}  ▲  {trip}   ╲          |
|    ╱             ●             ╲        |
|  fps:xxx       [±ss.s]           cpu:xxC|
+-----------------------------------------+
```
The gauge fills the window and is reactive to the screen size.
Current, target, total and trip are drawn inside the gauge area:
- {current} is the current speed, top-left with no label; it is right-aligned to a fixed anchor wide enough for "###.#" so the digits never shift as the value changes.
- {trip dist} is the current trip distance, top-right with no label, in the same font size as {current} (50px at full scale) and right-aligned to the panel edge. It is in whole metres with thousands separators and a trailing "m" (e.g. "1,234 m"), the same reading as the TwinMaster Trip row, and goes negative after a negative distance adjustment. It is drawn on the separate driver display only; in single-display mode the top-right corner keeps the rally clock.
- {tot} and {trip} are the values without labels, shown to two decimal places. Current and target stay at one decimal place.
- {target} sits left of the hub with a very small "Target" label above it, left-aligned with the value.
- Target, total and trip share the same font size (56px at full scale); current is slightly smaller (50px). All shrink with the gauge (scaled by gauge radius relative to a 256px full-scale radius). On the combined single display those three values are drawn a little smaller (48px at full scale); current stays at 50px. The driver display has no unit (KPH/MPH) toggle button.
- The ahead/behind readout box auto-sizes to its text (minimum 130px wide); the font stays at full size for sunlight legibility.
- The needle hub has a white ring matching the needle for contrast.

Compact gauge geometry (fills the available area):
- The gauge radius is width-driven: half the drawing-area width minus 25px (just enough margin for the 18px bezel ring), capped by height minus 95px.
- The gauge is centred horizontally; the needle hub sits low, 75px above the bottom edge, leaving just enough room below the hub for the ahead/behind readout box and the footer line.
- The top of the gauge arc may extend up into (cut into) the target line and value area; the target text and clock are drawn on top of the gauge graphics.
- fps (bottom-left) and cpu (bottom-right) are drawn with their baseline in line with the bottom of the ahead/behind readout box.

The number of digits displayed for any of the values should not affect their position; the decimal point should remain in the same place.



**_Co-Pilots display window (1280 x 400) - dark theme only_**

The co-pilot display window is wide (1280px) and shallow (400px). It has seven screens:
1) Stage setup
2) Calibration
3) TwinMaster display (default)
4) Date and Time setup
5) Auto Start setup
6) Setup
7) Adjust Distance Traveled

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
|  Speed(KPH)      Distance(m)      Auto       Time                 Mem Set   Recall                       |
|    xx.xx          xxx,xxx           [Y]     mm:ss [del]             [1]    [1]                           |
|    xx.xx          xxx,xxx           [Y]     mm:ss [del]             [2]    [2]                           |
|    xx.xx          xxx,xxx           [Y]     mm:ss [del]             [3]    [3]                           |
|    xx.xx          xxx,xxx           [Y]     mm:ss [del]             [4]    [4]                           |
|    xx.xx          xxx,xxx           [Y]     mm:ss [del]             [5]    [5]                           |
|    xx.xx          xxx,xxx           [Y]     mm:ss [del]                                                  |
|    xx.xx          xxx,xxx           [Y]     mm:ss [del]            [clear memory]                        |
+----------------------------------------------------------------------------------------------------------+
|  New segment:  Speed [______] KPH    Distance [________________] m    Auto [_]    [add]       [back]     |
+----------------------------------------------------------------------------------------------------------+
```
The exisiting segments should have editable values and scroll if there are more than 5 rows, The font should be 18px. The scrollbar should be touch-friendly: slider 20px wide, trough 24px wide.
When editing any value a numeric entry keyboard should be shown on the right of the screen with a ";" button, buttons 63x50 pixels.
The New line at the bottom should have fonts 18px, speed entry boxe 130x40 pixels and distance 300x40 pixels, and buttons 80x40 pixels.
The distance allows mutiple values seperated by ";" to be entered, each semi-colon seperated value creates a segment at the speed defined.


The target speed is in KPH and the distance is in meters.
Counts per hour = (input_kph * 1000 * 3600) / (cal / 1000)
Changes in calibration have no effect on stored segment distance and speed values, but do update the stored counts for distance and speed.
Time is display only and caculated as the mm:ss required to cover the distance at the speed for that segment.

The memory storage allows for upto five stage setups to be remembered and then recalled on request, pressing the set button for the memeory number should copy the current segment setup into that memory position in the configuration file, after a conformation dialog (30px with white border) if the memory position is not empty. Pressing recall and a memory number should copy that memory position from the config file into the current setup and configuration, updating the display. Memory clear, after a conformation dialog box, should remove the memory sections from the configuration file.Buttons should be 66x43 pixels. 
If a memory location has a segments stored then the recall button should be a white background and black text.


---

**2) Calibration Screen**

The screen is two columns. The left third shows the current readings in one 20px font: the live sensor counts under the title "Current Sensor counts", "Using Sensor 1 only" or "Using Sensor 1&2 adv.", then total distance, the calculated count and the two counter counts, then the current calibration as mm/1000p and meters per pulse. The right two thirds is the calibration run, also in 20px text: Start, Stop and Set in a vertical column, the counting distance, the actual-distance entry, the new-calibration entry, and the keypad. The next step is shown in white: Start until it is pressed, then Stop, then Set, then Start again after Set. Along the bottom of the screen, as on the other pages: Set sensor 1, set both sensors, reset to 1m per pulse, and back at the right.
Start remembers CNTR_1 and CNTR_2 and counts distance in metres and pulses up from zero. That distance is copied into the actual-distance entry while the run is going. Stop freezes that count. The rest of the rally meter keeps running. After Stop, the actual metres traveled can be edited, and the new calibration entry below it stays in step: changing the metres recalculates the calibration, and changing the calibration recalculates the metres. Set stores that calibration.
The display should update the distances and counters every 10 ms while this screen is shown, but not when it is not displayed. After Stop, the calibration-run count does not advance.
```
+------------------------------------------+----------------------------------------------------------------------------+
| LEFT (1/3) current info, from the top | RIGHT (2/3) change calibration                                               |
| Current Sensor counts                | CALIBRATION                                                                    |
| 1: xxxxxxxxxx                        | [Start]  Distance: 0 m    Pulses: 0                                          |
| 2: xxxxxxxxxx                        | [Stop]   Actual distance traveled [________] meters                          |
| Using Sensor 1 only / 1&2 adv.       | [Set]    New calibration [________] mm/1000p                                 |
|                                      |                              keypad                                          |
| Total distance: xxx,xxx m            |                                                                              |
| Counts calculated: CNTR_A            |                                                                              |
| 1: CNTR_1                            |                                                                              |
| 2: CNTR_2                            |                                                                              |
| Current Calibration                  |                                                                              |
| NNNNNN mm/1000p                      |                                                                              |
| meters per pulse                     |                                                                              |
| 0.NNNNNN m/pulse                     |                                                                              |
+------------------------------------------+----------------------------------------------------------------------------+
| [Set sensor 1]  [set both sensors and agv.]  [reset to 1m per pulse]              [back] |
+------------------------------------------------------------------------------------------+
```

Max input: 100,000m. There is no minimum distance.
new_cal = (input_meters * 1000 * 1000) / total_count_diff
When editing any value a numeric entry keyboard should be shown on the right of the screen, the same as the stage setup screen.
When [Set] is pressed the new calibration should be changed in the rally_config file as well as recaculating all target_speed and distance in the segments and memeory.
The config and caculations should be updated when [Set sensor 1] or [set both sensors and agv.] is selected. Each of those buttons, and [reset to 1m per pulse], first shows a confirmation dialog that explains the change. The dialog is only as large as that text, so the explanation and the Yes and No buttons fit on the co-pilot screen. Yes applies it and saves. No leaves the calibration and sensor mode as they are.
---

**3) TwinMaster Screen (Default)**

Two-column layout with bottom navigation row:

```
+-------------------------------------------------------------------+--------------------------------------+
| LEFT PANEL (70%)                                                  | RIGHT PANEL (30%)                    |
|                                                                   | [Connect speaker] [mute] hh:mm:ss    |
|  [Total]  xxx,xxx  m [Adj]                                        |  Alarm in km                         |
|  [stopwatch] mmm:ss                                               |  [2] [3] [4]                         |
|  [Trip]   xxx,xxx  m                                              |  [5] [6] [7]                         |
|  [stopwatch] mmm:ss                                               |  [8] [9] [10]                        |
|  [Next/prev]   xxx,xxx  m   xxx kph                               |  [11] [12] [13]                      |
|                                                                   |  x,xxx m to alarm  [clear]           |
+-------------------------------------------------------------------+--------------------------------------+
|   [stage go / abort stage]  [segments]   [Adj. driver Zero (xx.xxs)]   [calibration]     [date/time]  [cog] |
+----------------------------------------------------------------------------------------------------------+
```

Layout:
- GtkGrid for Total/Trip rows with aligned columns: heading | value | unit | speed. The elapsed time is the row under each of the Total and Trip buttons.
- The number of digits displayed for any of the values should not affect their position, the maximum number of meter to display is 999,999 
  before switching to km.
- Distances formatted with comma separators and fixed minimum width of 7 characters (e.g., "      0", "  1,234", "999,999")
- Time formatted as space-padded minutes (3 chars) + ":" + zero-padded seconds (2 chars), e.g., "  0:00", " 12:34", "120:00", 
    when more than four chars of minutes, switch to hours:minutes and if more than four chars of hours display "toolong".
- All fonts bold, all buttons have 2px solid white border for daylight visibility
- 5px border around the entire screen (all co-pilot screens)
- Two-column layout: left panel 70% width (~870px), right panel 30% width (~360px)
- Left panel:
  - Total/Trip in a GtkGrid (15px below segment info, 10px gap between rows):
    - Col 0: heading buttons "Total" / "Trip" / "Next" (48px bold monospace)
    - Col 1: distance value, right-aligned, 7-char width (88px bold monospace)
    - Col 2: unit "m" (48px bold monospace), bottom-aligned
    - Under the Total button, and under the Trip button, the elapsed time mmm:ss (36px monospace, light grey #CCCCCC) with a small white stopwatch icon to its left
    - Col 3 of the Total row: a thin vertical [Adj] button. The letters run downward (A, then d, then j). The button is twice as wide as the letter A and its border. It opens Adjust Distance Traveled.
    - Col 3 of the Next row: speed of the next segment on two lines, the number above its unit (e.g. "80" over "kph"), vertically centred, so the column is narrow
    - Col 4, spanning the three rows, between the speed and the right panel: "Trip history" (20px) with the last six recorded trip distances below it, most recent at the top, one per line, right-aligned in metres with comma separators (40px bold monospace, the largest that lets the heading and six lines fit the height of the three rows without moving the navigation row). Lines with no history yet are blank. This column is hidden in single-display mode.
- Segment info on the third line "Next" showing the distance to the next segment in meters and the speed of the next segment, if there are no segments the next line shows ---.--- and the speed shows ---. If past the end of the of the segments then the distance shows the negative meters past the end of the last segment and the speed shows "END". Next rounds up to the nearest meter so that Total/Trip are in sync to it as they round down.
  
- Right panel:
  - Rally clock (hh:mm:ss) at top, right-aligned (30px bold, minimum 8 chars wide)
  - Mute button immediately to the left of the clock, shown only while a current segment is selected. It is the same height as the 30px clock and uses the standard unmuted icon (audio-volume-high) or muted icon (audio-volume-muted). Pressing it toggles. The choice is not saved. Stage go sets it back to unmuted. While muted, the ahead/behind tones do not sound; button beeps and the distance alarm still sound.
  - [Connect speaker] is a single-line button on the top row, immediately to the left of the mute button. It is shown only when bluetooth_audio_address in the config is not empty. Pressing it reconnects that remembered Bluetooth speaker and makes it the audio output. When the config entry is empty the button is not shown.
  - "Alarm in km" (20px) sits above the keypad. The buttons are four rows of three with a 4px vertical gap: [2]-[4], [5]-[7], [8]-[10], [11]-[13] (22px font, 62x47px buttons)
  - Alarm countdown ("x,xxx m to alarm") and [clear] button below alarm buttons (28px white font #FFFFFF)



- Navigation buttons spread across full-width bottom row (20px font, 43px tall):

- stage go: shown when no current segment is selected (segment_current_number is -1, or does not point at a segment). Conformation dialog (with 30px text and buttons with at least 20px between buttons), with "Auto start" option,  with yes reseting Total, Trip, and Segment (counters + start time), sets the driver's display gauge to green, and zero's the ahead_behind_zero_offset_ms. Yes also clears any auto start time still in the future, so the stage starts immediately and that countdown does not fire later. "Auto start" option should go to the "Auto start setup screen. The dialog also has a button labelled with the soonest whole minute (hh:mm) that is at least 10 seconds ahead of rally time. Pressing it stores that minute as the auto start time, the same as Set on the Auto Start screen, and closes the dialog. If that minute is no longer at least 10 seconds ahead, the button is disabled. While a current segment is selected, the same button reads "abort stage". Its conformation dialog asks "Abort stage?"; Yes sets segment_current_number to -1 and saves, No leaves the stage running.
 
- segments: goes to Stage Setup

- Adj. driver Zero - Displays the current ahead_behind_zero_offset_ms in the label and at the point  pressed, the drivers/ahead behind value including the current   ahead_behind_zero_offset_ms is remembered so that it can be used in the conformation dialog (white border and 30px font, at least 20px between buttons) with the text "Adjust the ahead behind value by xx.xx seconds, currently xx.xx seconds" before being set in the ahead_behind_zero_offset_ms and changing the drivers ahead/behind guage. Along with Yes / No the dialog should have a "Reset to 0.0" option slighly distant from the Yes/No.
 
- calibration: goes to Calibration screen
 
- date/time: goes to Date/Time Setup screen

- setup: a cog button (the standard preferences-system icon) immediately to the right of date/time. It stays a fixed 43px square while the other navigation buttons share the remaining width. Opens the Setup screen.

- Reset buttons: [Total] / [Trip]  are reset buttons and reset their respective counters and start time only. Every trip reset, by whatever means, first records the trip distance at the front of trip_history_m (see global variables), so the Trip history column always shows what the trip read at each of the last six resets.

- [next/prev] button is only active when within 500m of the begining of a segment or the end of the segment, when it is within 500m of the end of a segement 
    the button displays "next", when it is within 500m of the start of a segment (not the first) it displays "prev" otherwise it displays "--->". The button allows the correction of distance of segment starts/ends, due to poor driving dicipline or mistakes in setting up the road book. When pressed within the 500m before a segment end then the segment distance should be reduced to the distance when the button was pressed. When press within 500m of the of the start of a segment (not the first) then the distance of the last segment should be extened to match the when the button was pressed.

- [clear] button is only visible when an alarm is active

- Distance alarm: co-pilot presses a km button (2-13) to set an alarm that many km ahead of the current total distance. The target is calculated in pulses and stored in the config file to survive Pi5 restart. When the total distance reaches the target, alarm.wav is played, then the alarm auto-clears after 5 seconds. The countdown ("x,xxx m to alarm") is shown in the right panel. The alarm check runs regardless of which co-pilot screen is visible. Press [clear] to cancel an active alarm.

- Segment arrival tone: when the Setup option "Arrival tone 500m before segment end" (`arrival_tone_enabled`) is on and a segment is current, arrival.wav (a two-note chime of about a second and a half) is played once per segment start, the first time the distance remaining in the segment is 500 m or less. A segment shorter than 500 m is already inside that distance when it starts, so the tone sounds as soon as it starts. The tone sounds again for the same segment only when its start reading is recorded again ([prev], or stage go). It is checked every update regardless of which co-pilot screen is visible, and, like the distance alarm, it is not silenced by the mute button. If the app starts with a segment already inside 500 m, that segment is treated as sounded, so a restart mid-segment stays quiet.

**TwinMaster Screen - Single-Display Mode**

When the application is in single-display mode (exactly one monitor, 1280x400 - see "Display Assignment"), the right-hand alarm panel is replaced by the compact driver display:

- The right panel is widened to 430px (from 360px) so the gauge is height-limited rather than width-limited; the left panel gives up its fixed 870px minimum width and takes whatever width remains.
- The right panel contains the compact driver gauge layout, identical to the 800x480 driver layout: the gauge with needle and digital readout, with target, current, total and trip values drawn inside the gauge area, fonts scaled down with the gauge size, and the same compact gauge geometry (gauge fills the panel width, arc top may cut into the target line, fps/cpu in line with the readout box bottom). Target, total and trip are drawn a little smaller than on the standalone driver gauge (48px at full scale instead of 56px); current stays at 50px.
- The rally clock (hh:mm:ss) is kept, drawn in the top-right corner of the gauge area at 28px scaled with the gauge (minimum 20px). The connect speaker control, when a speaker is remembered, is the same small square ("cnet" / "spkr") centred under that clock. The mute button stays to the left of the clock.
- Alarm buttons are unavailable in this mode, so new alarms cannot be set. An alarm persisted in the config from a previous run still fires: alarm.wav is played when the target is reached and the alarm auto-clears after 5 seconds; no countdown or [clear] button is shown.
- The left panel has the total/trip times hidden and the next target speed and Trip history column hidden inorder to allow the gauage to be bigger, bottom navigation row are unchanged.



---

**4) Date/Time Setup Screen**

```
On entry pre fill the date and time entry box's with the current date and time.
All fonts to be 20px

+----------------------------------------------------------------------------------------------------------+
|  DATE/TIME SETUP                                                                                        |
+----------------------------------------------------------------------------------------------------------+
|  System Clock:  yyyy/mm/dd  hh:mm:ss                                               7    8    9            |
|  Rally  Clock:  yyyy/mm/dd  hh:mm:ss                                               4    5    6            |
|  [+1h][+10m][+1m][+10s][+1s][+10ms][Zero][-10ms][-1s][-10s][-1m][-10m][-1h]       1    2    3            |
|  Set Rally Clk: [yyyy/mm/dd] [hh:mm:ss] [Set]                                      /    0    :            |
|                                                                                 [C]  [   <--   ]          |
+----------------------------------------------------------------------------------------------------------+
|                              [NTP time sync]                              [back]                         |
+----------------------------------------------------------------------------------------------------------+
```
Display a numeric keypad for entry on the right, it is a different keypad to other screens as it has "/" and ":" 
on it, but no ";" and ".".

Under Rally Clock, a row of buttons adjusts `rallyTimeOffset_ms` and saves it. [+1h] [+10m] [+1m] [+10s] [+1s] [+10ms] add that much to the offset. [-10ms] [-1s] [-10s] [-1m] [-10m] [-1h] subtract it. [Zero] sets the offset to 0. The rally clock updates immediately.

[Set] sits immediately to the right of the time entry. It stores the rally clock offset and returns to TwinMaster. The bottom row buttons use the same 20px font and 43px height as the TwinMaster navigation buttons, and share the width of the row.

[NTP time sync] tells the Pi to set the system clock from NTP immediately by restarting systemd-timesyncd. The button is enabled only while NetworkManager reports a full internet connection. It stays disabled when there is no link, or the connection is limited or a captive portal.

- Display options, remembered Bluetooth audio, Wi-Fi, and phone web access are on the Setup screen.

**5) Auto Start Setup Screen**

```
On entry, if an auto start time is saved and still in the future, prefill the time entry with that time. If it is blank or in the past, prefill the entry with the soonest whole minute that is at least 30 seconds ahead of rally time.
All fonts to be 20px

+----------------------------------------------------------------------------------------------------------+
|  AUTO START SETUP                                                                                       |
+----------------------------------------------------------------------------------------------------------+
|   Rally  Clock:  yyyy/mm/dd  hh:mm:ss        30px                                                        |
|                                                                                                          |
|   Auto Start:    yyyy/mm/dd  hh:mm:ss        30px  (Blank date time if not set)                          |
+----------------------------------------------------------------------------------------------------------+
|   Set Auto Start time within 3 Hours: [__________]    30px                                               |
+----------------------------------------------------------------------------------------------------------+
|  [Clear]                              [set]                                             [back]           |
+----------------------------------------------------------------------------------------------------------+
```
The bottom row buttons use the same 20px font and 43px height as the TwinMaster navigation buttons.
Display a numeric keypad for entry on the right, it is the same keypad layout as the Date/Time setup screen.

Only allow time to be entered in the 24 hour clock, display an error and don't allow the time to be set if more than 3 hours in advance.

Clear - sets the auto_start time to 0, making it in the past and therefor it has no further effect. The time entry is then filled with the soonest whole minute at least 30 seconds ahead, the same as a blank entry on opening the screen.
Set - sets the auto start time in the config file etc. recording the offeset as defined, the screen is updated to show the new values.

**6) Setup Screen**

Opened from the cog button to the right of date/time on TwinMaster. Fonts are 16px so the screen fits the 400px height. The display section at the top is split two thirds / one third. The left two thirds start with the SETUP title, then on the same line "version" and the installed release tag, then "hostname" and the Pi's hostname, all in the same 22px title font. The attached displays follow, then [Reset layout] with force single display mode immediately to its right. The right third shows the phone web address above its QR code. The address is 24px and wraps onto more than one line when it is wider than that third. The QR code is centered in the third. Two thin horizontal lines run under that section so the split is visible. [back] is the same 20px, 43px-tall navigation button as on the other screens, sized to its label, and sits at the right. [Exit & Check for updates] sits immediately to its left, and [exit app] sits immediately to the left of that. [back] returns to TwinMaster. [exit app] saves and closes the application. [Exit & Check for updates] saves, closes the meter, and opens a terminal in the project directory running the `update` script. The terminal stays open until Enter is pressed so the result can be read.

```
+----------------------------------------------------------------------------------------------------------+
|  SETUP  version v1.2.0  hostname raspberrypi       |  historicrallymeter.local:          |
|  [DSI-2]     0 px    [HDMI-A-2]    0 px    [HDMI-A-1] |  8080                               |
|  1280x400              800x480              2560x1440  |  [QR]                               |
|  left                  normal               normal     |                                     |
|  [rotate] [KB]         [rotate] [KB]        [rotate] [KB] |                                  |
|  [Reset layout]   force single display mode ( o)  |                                     |
|----------------------------------------------------------------------------------------------------------|
|----------------------------------------------------------------------------------------------------------|
|  Options                                            |  Wi-Fi / Bluetooth                                  |
|  speed units  [ KPH ]                               |  [Remember bluetooth audio]  name  [Connect] [Disconnect] |
|  Arrival tone 500m before segment end  ( o)         |  [Hotspot: hostname]   [Join WiFi4hostname]         |
|                                                     |  status line                                        |
|                                [exit app]  [Exit & Check for updates]  [back]                          |
+----------------------------------------------------------------------------------------------------------+
```

Below the two lines the screen is split into two equal columns. The left column is headed "Options" and holds the option controls, one per row: speed units, then the Arrival tone switch. The right column is headed "Wi-Fi / Bluetooth" and holds the Bluetooth audio row, the Wi-Fi row and the status line, which wraps within that column. The navigation row stays across the bottom.

The update script writes the checked-out release tag (vX.Y.Z) to `version.txt` in the project directory when it fetches the update. The Setup title reads that file. If the file is missing, the version is shown as unknown. The hostname is the Pi's hostname, without a domain suffix.

The phone address sits above the QR code and wraps onto further lines when it does not fit the right third. Both are shown only when `web_enabled` is true. The address is the web client URL without the `http://` scheme (host `historicrallymeter.local`, or the device's LAN address if mDNS is unavailable, and the port from `web_port`). The QR code encodes the full URL. Both are omitted, and the right third says the web server is disabled, when `web_enabled` is false. [Reset layout] is in the display section, under the attached screens and above the dividing lines. Force single display mode sits immediately to its right.

Displays attached to the Pi are read from `wlr-randr`. Each connected output is shown in left-to-right order with its logical size in pixels (width and height swap for left, right, and the matching flipped orientations), its orientation (normal, left, inverted, right), and the horizontal gap in pixels to the next display (`next x − (x + width)`).

[Reset layout] places the co-pilot panel on the left when one is attached. That panel is the output whose modes include 400×1280 or 1280×400. Its mode is set to 400×1280 when that mode exists, with the left orientation (transform 90), which makes the logical size 1280×400. If it only has a 1280×400 mode, that mode is used in the normal orientation so the logical size stays 1280×400. The driver panel, the output whose modes include 800×480 or 480×800 and which is not the co-pilot panel, is placed immediately to its right in the normal orientation using the 800×480 mode when that mode exists. Every other attached screen keeps its current mode and orientation and is placed further right. Every gap is 0 px and every screen is top-aligned. The layout is applied immediately with `wlr-randr` and written into the kanshi profile (`~/.config/kanshi/config`) whose outputs are exactly the screens attached now. That profile is placed first so it is the one kanshi applies on the next login. Other profiles are left as they are.

Under each display, a small button with the rotate-right icon cycles orientation indefinitely: normal → left → inverted → right → normal. Only that output's orientation changes; positions are left as they are, then the same kanshi profile is updated. Beside that rotate button is the on-screen keyboard control for that display. It reads "Enable KB on DSI-2" when the keyboard is off or pinned to another display, and "Disable KB on DSI-2" when squeekboard is enabled on that display. Enable writes `SQUEEKBOARD_PREFERRED_OUTPUT` to the display name, turns the keyboard on for the next login, and starts squeekboard as a Wayland client on that output so the keyboard is actually shown there. An X11 copy of squeekboard ignores the output name, which is why Desktop Preferences can show a blank location. Disable turns it off. The choice is kept for the next login.

The app places its windows from the monitor list it read at startup, so a layout change is on the desktop immediately but the meter windows follow it after the app is started again. The status line says so after a successful change.

Options moved here from Date/Time:

- force single display mode is a toggle that sets `force_single_display` in the rally config and forces single-display mode even when more than one screen exists. It takes effect the next time the app starts.
- speed units is a button showing the current unit ("KPH" or "MPH"). Pressing it toggles `units` and saves. This is the only place the units are changed. Driver, co-pilot, and web speeds follow it.
- Arrival tone 500m before segment end is a switch that sets `arrival_tone_enabled` and saves. See "Segment arrival tone" under TwinMaster for what it does.
- [Remember bluetooth audio] looks at the Pi's current audio output. If that output is a Bluetooth device, its name and address are written to `bluetooth_audio_name` and `bluetooth_audio_address` and the name is shown beside the button. If the Pi is not using a Bluetooth speaker, both entries are saved empty, the name is blank, and `bluetooth_audio_autoconnect` is cleared. The TwinMaster connect-speaker control is shown only while an address is stored.

[Connect] and [Disconnect] are shown only when a Bluetooth address is recorded. Connect reconnects that speaker, makes it the audio output, trusts it in BlueZ, and sets `bluetooth_audio_autoconnect` so the app connects it again on startup. Disconnect disconnects it, moves the default output off that speaker, untrusts it so BlueZ does not reconnect it at login, and clears `bluetooth_audio_autoconnect`. The remembered name and address stay, so the buttons remain. The TwinMaster connect-speaker control still connects immediately and does not change the saved autoconnect choice.

Wi-Fi uses NetworkManager on `wlan0`. [Hotspot: hostname] starts an open access point whose SSID is the Pi's hostname, with no password, shared IPv4, and autoconnect on. [Join WiFi4hostname] joins the open network whose SSID is `WiFi4` followed by the hostname (for hostname `raspberrypi`, `WiFi4raspberrypi`). The chosen connection is set to autoconnect and other Wi-Fi connections are set not to autoconnect, so the choice is what NetworkManager brings up after reboot. The status line shows the result.

**7) Adjust Distance Traveled**

Opened from the vertical [Adj] button on the Total row of TwinMaster. It sets `distance_offset_counts`, entered in metres and converted with the current calibration. The readings line and the entry line are 28px, the readings in the monospace clock font; the nudges are 16px and the keypad is the standard keypad. [back] is the same 20px, 43px-tall navigation button as on the other screens, sized to its label, and sits at the bottom right. It returns to TwinMaster.

```
+----------------------------------------------------------------------------------------------------------+
|  ADJUST DISTANCE TRAVELED                                                                                |
+----------------------------------------------------------------------------------------------------------+
|  Total:  xxx,xxx m   Trip:  xxx,xxx m   Offset:  xxx,xxx m                         7    8    9            |
|  Adjust by / total: [__________] m   [clear]                [apply]                4    5    6            |
|                                                                                    1    2    3            |
|  [-1km] [-100m] [-10m] [-1m]  [+1m] [+10m] [+100m] [+1km]                          +    0    -            |
|                                                                                 [C]  [   <--   ]          |
|  [Late to the start] [ Missed turn ] [ Missed turn ]                                                     |
|  [ minus -1,234 m  ] [ 2x -1,234 m ] [ 2x -2,345 m ]                                                     |
+----------------------------------------------------------------------------------------------------------+
|                                                                                              [back]      |
+----------------------------------------------------------------------------------------------------------+
```

The first line shows the live total, trip and current offset in metres, refreshed with the TwinMaster values while this screen is visible. Negative values have a leading minus. There is a clear gap between the entry line and the nudge line.

The entry box holds whole metres and takes its input from the keypad on the right and from the nudge row. The keypad is the numeric keypad with "+" and "-" in place of ";" and ".". Pressing "+" or "-" puts that sign at the front of the entry, replacing any sign already there. The entry has three meanings, decided by its first character when [apply] is pressed:

- Starts with "+" or "-": add that many metres to the current offset, or subtract them. So after a 350 m missed turn and the drive back, "-350" [apply] removes it.
- A number with no sign: make the total distance read that many metres. The offset becomes the current offset plus (entered metres − current total), so the total shows the entered value and trip, segment and everything derived move by the same amount. This is how the total is matched to a roadbook distance at a known point.
- "0" on its own: set the offset to zero.

An empty entry or one that is not a number does nothing. [apply] converts the metres to counts with metersToCounts, stores the offset, saves the config, clears the entry, and returns to TwinMaster.

[clear] sits to the left of [apply] with a wide gap between them so one is not pressed for the other. It sets the offset to zero, saves, clears the entry and returns to the TwinMaster display. Nothing else on the screen changes the offset.

The nudge buttons change only the entry box. [-1km] [-100m] [-10m] [-1m] subtract from the value in the box and [+1m] [+10m] [+100m] [+1km] add to it. An empty box is treated as "+0", so the first nudge produces a signed entry such as "+10" or "-100" that [apply] will add to the offset. A signed entry stays signed as it passes through zero ("+5" then [-10m] gives "-5"). An unsigned entry stays unsigned, and is held at 0 rather than going below it. The nudge buttons use the same 16px style as the Date/Time clock nudges.

**Quick adjustment buttons.** Below the nudge row, after a gap, three two-line buttons sit in the left half of the area left of the keypad. They are on the left because they all subtract distance, like the minus end of the nudges, and the value on each caption carries a minus sign for the same reason. Each applies at once: it subtracts the metres in its caption from the offset exactly as "-N" [apply] would, saves, clears the entry and returns to TwinMaster. The captions are refreshed with the readings while the screen is visible, so they always name the distance they will remove:

- [Late to the start / minus -xxx m] — xxx is the current trip. For when the trip was started (by stage go or the Trip button) before the car actually reached the start: it takes the total back to what it read when the trip started.
- [Missed turn / 2x -xxx m] — xxx is the current trip. For when the trip was reset at the missed turn: the trip is the distance driven past it, and twice that is the drive out and back.
- [Missed turn / 2x -yyy m] — yyy is the most recent entry in the trip history. For when the trip was reset again on turning round: the previous trip is the distance driven past the turn, and twice that is the drive out and back.

A button is disabled when its distance is not positive (trip at or below zero, or no trip history yet), so it cannot add distance or do nothing.

## Remote Web Access (mobile phones)

The application serves a small web application so that ordinary browsers on mobile phones, joined to the same subnet as the HistoricRallyMeter device, can view live rally data and control the meter. Multiple phones may connect at once.

Phones can:
- Receive streamed live updates of trip/total distances, current and average speeds, target speed, and the ahead/behind figure.
- Reset the trip and total distances.
- Use the **next/prev** segment correction control (same rules and behaviour as the TwinMaster `[next/prev]` button).
- View and edit the segment (stage) setup, including storing and recalling memory setups.

### Transport decision

- **No UDP.** Browsers cannot receive raw UDP datagrams, so UDP broadcast is not an option for a browser-based client. On a local subnet the latency advantage of UDP over TCP for small ~10 Hz payloads is negligible (dominated by WiFi airtime and browser render), so nothing is lost by not using it.
- **WebSocket** is used as the single bidirectional channel. Live telemetry is pushed from the device to every connected phone; control commands (reset, segment edits) travel back up the same connection. WebSocket is reliable and ordered, which is required for control operations that must not be silently dropped.
- Static assets (HTML/CSS/JS) are delivered over plain HTTP by the same embedded server.

### Server architecture

- The web server is embedded in the existing application and integrated into the GTK/GLib main loop (libsoup, which shares the `GMainContext` with GTK). This avoids a separate networking thread and, crucially, means incoming commands are handled on the same thread that owns `AppData`/`RallyState`.
- **Thread-safety rule:** all rally state (`RallyState`, including the `segments` vector, trip/total start counters, `segment_current_number`) is single-threaded and lock-free by design. Network commands MUST NOT mutate this state directly from any other thread. Every command is executed on the GLib main loop and routed through the **same functions the on-screen controls already use** (e.g. the trip/total reset handlers and the segment setters in `callbacks.cpp`), so validation, calibration recalculation, and config persistence stay identical to the physical UI. If a non-GLib server library is ever used instead, commands must be marshalled onto the main loop with `g_idle_add()`/`g_main_context_invoke()`.
- Telemetry is broadcast at a throttled **10 Hz** (decoupled from the 100 Hz internal poll loop) — ample for a human-readable phone display and light on WiFi.
- After any state change (from a phone or from the on-device screens), the current authoritative state is broadcast to all connected clients so every phone and the dash stay in sync. Conflicts between multiple editors resolve as last-writer-wins.

### Network discovery

- The server listens on a fixed TCP port (default `8080`, configurable).
- The device advertises itself over mDNS/Avahi (e.g. `historicrallymeter.local`) so phones need not know the IP address.
- The **Date/Time Setup screen** displays the connection URL as text and as a **QR code** so co-drivers can scan it with a phone camera to open the web client (see "Date/Time Setup Screen").

### Directory layout

The server-side integration and the browser client are kept in separate directories:

- `webserver/` — the C++ code that embeds the HTTP/WebSocket server, serialises telemetry to JSON, and dispatches incoming commands onto the main loop through the existing control functions. One class per file, consistent with the rest of the project.
- `webclient/` — the static web application served to and executed on the phones (`index.html`, CSS, JS, and any assets). This directory contains no device code; it is a self-contained browser client that the server publishes as static files. (A future option is to compile the C++ gauge rendering to WebAssembly and place the `.wasm` artifact here, but the initial client is plain HTML/CSS/JS with no build step.)

### Message protocol (JSON over WebSocket)

**Telemetry (device → phone), ~10 Hz:**
```json
{
  "type": "telemetry",
  "rally_clock": "14:53:07",
  "trip_m": 537,
  "total_m": 855053,
  "cur_kph": 38.0,
  "trip_avg_kph": 41.2,
  "total_avg_kph": 42.1,
  "target_kph": 100.0,
  "ahead_behind_s": -1.8,
  "segment_number": 2,
  "segment_count": 3,
  "next_enabled": true,
  "prev_enabled": false,
  "units": "kph",
  "tone": { "tone_ms": 500, "silence_ms": 200, "freq_hz": 1046.5, "wave": "sine" },
  "beep": null
}
```

- `next_enabled` is true when the TwinMaster button would read "next" (within 500 m of the current segment end with a following segment); `prev_enabled` when it would read "prev" (within 500 m of the current segment start and not on the first segment). Both false otherwise.
- Speeds are already in the display units; `units` is `"kph"` or `"mph"`.
- `tone` is the ahead/behind cadence the meter's speaker is playing at that moment, so the phone can play the same one: `tone_ms` 0 is silence. It is the same three numbers passed to the tone generator, including silence when muted, when past the stage end, or when no stage is running.
- `beep` is always `null`: this version has no Beep Assist. The client ignores a null beep.

**State (device → phone)** — sent on connect and after any change, so clients can render/refresh the segment editor:
```json
{
  "type": "state",
  "segment_current_number": 1,
  "total_reset_asks": false,
  "segments": [
    { "target_speed_kph": 75.0, "distance_m": 1330, "autoNext": true }
  ],
  "autostart": "none",
  "memory_populated": [true, false, false, false, false]
}
```

- `total_reset_asks` is always false in this version: a Reset Total from the phone is applied at once, as the Total button on TwinMaster is.
- `autostart` is `"none"` or the stored auto start time as hh:mm in rally time.
- `memory_populated` says which of the five memory slots hold a stage, so the phone marks them as the Stage Setup screen does.

**Commands (phone → device):**
```json
{ "type": "reset_trip" }
{ "type": "reset_total" }
{ "type": "next" }
{ "type": "prev" }
{ "type": "distance_adjust", "delta_m": -10 }
{ "type": "distance_set", "meters": 4200 }
{ "type": "segment_set", "index": 0, "target_speed_kph": 75.0, "distance_m": 1330, "autoNext": true }
{ "type": "segment_add", "target_speed_kph": 75.0, "distance_m": 1000, "autoNext": true }
{ "type": "segment_delete", "index": 2 }
{ "type": "memory_store", "slot": 1 }
{ "type": "memory_recall", "slot": 1 }
```

- Distances are entered/displayed in metres and speeds in KPH (calibration-independent), matching the stage setup screen; the device recomputes counts against the current calibration.
- `next` and `prev` invoke the same logic as the TwinMaster `[next/prev]` button (`on_next_prev_segment` in `callbacks.cpp`): when within 500 m of segment end, shortens the current segment to the distance travelled and advances; when within 500 m of segment start (not the first segment), extends the previous segment and resets the current segment start. No effect if neither condition applies (same as a disabled button on the co-pilot display).
- `distance_adjust` and `distance_set` are the phone's Adjust row and change `distance_offset_counts` exactly as the Adjust Distance Traveled screen does: `delta_m` (−1000 to 1000, not 0) is "+N"/"−N", and `meters` (0 to 999,999) is the unsigned entry that makes the total read that many metres (0 clears the adjustment, as "0" does on the screen). Both save and are reflected on every display.
- `reset_total_capture`, `reset_total_cancel`, `beep_set` and `tone_set` are sent by other builds of the client and are ignored here.
- The device validates every command and ignores malformed or out-of-range ones. Invalid entries produce no state change.

### Web client design

The client is a single responsive page (`index.html`, `app.css`, `app.js`, `gauge.js`, no build step) that works in portrait or landscape on a phone browser, styled for legibility (large bold values, high contrast). It connects to the WebSocket on load, reconnects automatically if the connection drops, and rebuilds the connection when the page comes back to the foreground after telemetry has stopped. It has three views selectable by a tab bar: Live, Setup and Driver.

The header has no title. It holds a per-phone sound button and the connection state. The phone plays the meter's ahead/behind tone itself from the `tone` field in telemetry, with the same cadence and pitch as the meter's speaker, and clicks on every button pressed on the page. Browsers allow sound only after the page has been touched, so the button reads "Tap for sound" until then, "Sound is on" while playing, and "Sound is off" when that phone has muted itself. The choice is remembered on that phone only and does not affect the meter. The tone stops within three seconds if telemetry stops arriving.

**1) Live view (default)**

```
+------------------------------------------+
|  Rally Clock            14:53:07         |
+------------------------------------------+
|  [ prev ]              [ next ]          |   <- each enabled from telemetry
+------------------------------------------+
|  -1.8 s        | Total 855,053 m         |   <- ahead/behind colour-coded
|                |   avg 42.1   kph 38.0   |      Total/Trip boxes reset when pressed
|                | Trip      537 m         |
|                |   avg 41.2   tgt 100.0  |
+------------------------------------------+
|  Adjust  [-10] [ Total m ] [set] [+10]   |
+------------------------------------------+
|  Segment 2 of 3                          |
+------------------------------------------+
|  Autostart: none                         |
|  KPH   | Distance (m) | cumulative       |   <- read-only stage panel
|  75.00 |   1,330      |   1,330          |
+------------------------------------------+
```

- The ahead/behind value is colour-coded (green when ahead, red when behind) for a glance read.
- `prev` and `next` are enabled from `prev_enabled`/`next_enabled` and send `prev`/`next`, mirroring the TwinMaster `[next/prev]` button rules (500 m window at segment start or end).
- The Total box shows the total, its average speed and the current speed; the Trip box shows the trip, its average and the target speed. Pressing the Total box asks "Reset total distance?" and sends `reset_total`; pressing the Trip box sends `reset_trip` at once, as the TwinMaster Trip button does.
- Distances use the same auto-formatting as the TwinMaster (metres, switching to km with a unit label for large values).
- The Adjust row is the phone's version of Adjust Distance Traveled: `-10` and `+10` send `distance_adjust` with ±10 m; typing a figure and pressing `set` sends `distance_set` so the total reads that figure. An empty entry does nothing.
- The stage panel lists the segments as set, each one's speed in the display units, its length and the running total from the stage start, with the stored auto start time above it.

**2) Setup view**

```
+------------------------------------------+
|  # | Target kph | Distance m | Auto | x  |
|  1 |   75.0     |   1330     | [x]  |[del]|
|  2 |   75.0     |   1000     | [x]  |[del]|
|  3 |  100.0     |    250     | [ ]  |[del]|
+------------------------------------------+
|  [ + Add segment ]                       |
+------------------------------------------+
|  Store   [1][2][3][4][5]                 |   <- populated slots highlighted
|  Recall  [1][2][3][4][5]                 |
+------------------------------------------+
|  [ Reset Trip ]     [ Reset Total ]      |
```

- Each row edits one segment (target speed, distance, autoNext) and sends a `segment_set` command on change; `+ Add segment` and per-row delete send `segment_add`/`segment_delete`.
- Store and Recall each have their own row of slot buttons, highlighted when `memory_populated` says the slot holds a stage. Store asks before overwriting a populated slot; Recall asks before replacing the segments on screen and does nothing for an empty slot. They send `memory_store`/`memory_recall`.
- The editor is populated and kept current from the `state` message, so edits made on the device or on another phone appear here.
- This version has no Beep Assist or tone panel on the Setup view; the view ends with the Reset Trip and Reset Total buttons.

**3) Driver view**

A canvas drawing of the compact driver gauge (needle, coloured scale band, digital readout, current and target speed, rally clock), drawn from the same telemetry fields as the Live view and only while this tab is showing. Under it are the same Total and Trip boxes (press to reset) and the same Adjust row as the Live view.

### Configuration

Two keys are added to `rally_config.json`:
- `web_enabled` (bool) — enables the embedded web server (default true).
- `web_port` (int) — TCP port to listen on (default 8080).

### Build requirements (addition)

- HTTP/WebSocket server integrated with the GLib main loop (GIO sockets; static files from `webclient/`).
- libqrencode (runtime) for QR code on the Date/Time screen (loaded dynamically).

### Security note

The server is an unauthenticated service intended for a private, in-car subnet. Because the control surface can reset distances and alter segments, it should only be exposed on a trusted network. No credentials or transport encryption are provided by default.

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
- Save writes a temporary file and renames it over the config, so the config is either the old or the new content and never partial
- The config is not written on a timer or from the display update; it is written on settings changes, resets, the power-loss reset and clean shutdown

### Distance Calculation Tests
- Single counter mode: distance = CNTR_1 - start_cntr1
- Dual counter mode: distance = ((CNTR_1 - start_cntr1) + (CNTR_2 - start_cntr2)) / 2
- Verify integer math (no floating point until final display)
- Distance in meters = (count_diff * calibration) / 1000 / 1000
- Distance in centimetres = (count_diff * calibration) / 10000
- The pulse/metre conversion lives only in calculations.cpp: countsToCentimeters() (integer, for display), countsToMeters() (double, for segment distances) and its inverse metersToCounts() = (metres * 1e6) / calibration. No other file repeats the formula.
- metersToCounts() and countsToMeters() round-trip: countsToMeters(metersToCounts(m)) == m within 1e-6
- Handle 32-bit counter wrap-around correctly
- distance_offset_counts is added to CNTR_A in single and dual counter mode; with dont_apply_offset it is not
- A Trip reset with a non-zero offset records start = live + offset so the trip reads 0; a following negative adjustment takes the trip below zero and it counts back up through zero
- A Total reset zeroes the offset and leaves the running trip and segment distances unchanged

### Calibration Tests
- Default calibration value is 600000
- New calibration = (input_meters * 1000 * 1000) / total_count_diff
- No minimum input distance
- Maximum input distance is 100,000 meters
- Calibration change does not affect stored segment target speeds
- Calibration change affects all subsequent distance/speed calculations

### Speed Calculation Tests
- Speed in counts/hour from counts and time_ms
- Convert counts/hour to KPH: (counts_per_hour * calibration) / 1e9 (high precision double)
- Convert counts/hour to MPH: KPH * 100000 / 160934
- kphToCountsPerHour() returns double for high precision
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
- Counter power-loss flag at startup resets total, trip and segment start counts to the live counts, start times to now, segment_current_number to -1, clears the alarm and zeroes distance_offset_counts; no flag leaves them unchanged
- The status register is read twice per chip at startup and the second reading is the one used, so a first read of 0x00 after a cold boot does not hide a set flag
- Stage go zeroes distance_offset_counts

### Unit Toggle Tests
- Toggle units from KPH to MPH
- Toggle units from MPH to KPH
- All speed displays update on next refresh after toggle
- Header row shows current unit selection

### Display Update Tests
- Updates per second counts render calls in last full second
- Rolling count resets each second
- Driver display updates all speed values each refresh
- Driver display trip distance text matches the TwinMaster trip reading in whole metres, e.g. 1234 m gives "1,234 m" and -70 m gives "-70 m"
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
- Input validation accepts any distance above zero
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
- Trip history: a reset puts the trip distance in metres at the front of trip_history_m; the seventh entry is dropped; a negative trip is recorded with its sign
- Trip history string round-trips through the config file and parses "" as no entries and "1234,56" as two
- Calibration button navigates to Calibration screen
- RallyClock displays at top in hh:mm:ss format
- The vertical Adj button on the Total row opens Adjust Distance Traveled; back there returns to TwinMaster

### Adjust Distance Traveled Tests
- Entry "+350" then apply adds 350 m of counts to the offset; "-350" subtracts; "5000" changes the offset so the total reads 5,000 m; "0" zeroes it; empty or non-numeric does nothing
- Apply and clear both return to TwinMaster; clear zeroes the offset
- Nudge from an empty entry gives "+10" for [+10m] and "-100" for [-100m]; a signed entry passes through zero ("+5" [-10m] gives "-5"); an unsigned entry stops at "0"
- Nudges never change the offset; only apply does
- Keypad "+" and "-" set the leading sign of the entry, replacing an existing one
- Quick adjustment text: trip 1234 gives "-1234" for late start and "-2468" for missed turn; most recent history 2345 gives "-4690"; a trip or history of 0 or negative, or empty history, gives an empty text (button disabled)
- Pressing a quick adjustment button subtracts that many metres of counts from the offset, saves and returns to TwinMaster
- Stage Go dialog offers the next whole minute at least 10 seconds ahead, labelled hh:mm; pressing it stores that auto start time the same way as Auto Start Set, and the button disables once that minute is under 10 seconds away

### Date/Time Setup Screen Tests
- Display system clock in yyyy/mm/dd hh:mm:ss format
- Display RallyClock (with current offset) in yyyy/mm/dd hh:mm:ss format
- Input fields accept valid date and time values
- Calculate rallyTimeOffset = input_rally_time_ms - system_time_ms
- Set, immediately to the right of the time entry, stores the offset and returns to TwinMaster
- The row under Rally Clock adds or subtracts 1h, 10m, 1m, 10s, 1s, or 10ms from the rally offset and saves it; Zero sets the offset to 0
- Bottom row buttons are 20px and 43px tall, the same as TwinMaster
- NTP time sync is enabled only when the internet connection is full, and pressing it syncs the system clock from NTP
- Back button returns without saving changes

### Setup Screen Tests
- Cog button on TwinMaster opens the Setup screen; back is on the right and returns to TwinMaster
- [exit app], to the left of Exit & Check for updates, saves and closes the application
- Exit & Check for updates, to the left of back, closes the app and runs the update script in a terminal
- The display section shows SETUP, then version and the installed release tag, then hostname and the Pi hostname, the attached screens, and Reset layout with force single display mode to its right on the left two thirds, and the web address above the QR code on the right third, with two thin lines under that section
- When `web_enabled` is true, the right third shows the web client address and a QR code for that URL; both are omitted when `web_enabled` is false
- Attached displays show logical pixel size, orientation, and the gap to the next display
- Reset layout puts a 400×1280 panel on the left in the left orientation and an 800×480 panel to its right in the normal orientation, with other panels further right and no gaps, and writes that kanshi profile
- Rotate cycles normal, left, inverted, right
- Enable KB on a display pins squeekboard to that output, starts it on Wayland, and shows the keyboard there; Disable KB turns it off. The label follows the current display
- Force single display, speed units, and Remember bluetooth audio behave as they did on Date/Time and are no longer on that screen
- Below the two lines, the Options column on the left holds speed units and the Arrival tone switch; the Wi-Fi / Bluetooth column on the right holds the Bluetooth, Wi-Fi and status rows
- The Arrival tone switch sets `arrival_tone_enabled` and saves; it round-trips through the config file
- Arrival tone is due once per segment start the first time remaining is 500 m or less; not due again for the same start; due immediately for a segment shorter than 500 m; a segment already inside 500 m on the first check is marked sounded without sounding
- Connect and Disconnect appear only when a Bluetooth address is saved; Connect sets autoconnect, Disconnect clears it and keeps the address
- Hotspot uses the hostname as an open SSID; Join uses WiFi4 plus the hostname; the chosen NetworkManager connection autoconnects and the other Wi-Fi connections do not

### Remote Web Access Tests
- WebSocket telemetry includes `next_enabled` and `prev_enabled` consistent with TwinMaster button state
- `next` and `prev` commands apply the same segment correction as the co-pilot `[next/prev]` button; no-op when not in the 500 m window
- Telemetry `tone` carries the cadence last passed to the tone generator: silent as `tone_ms` 0, otherwise the same tone_ms, silence_ms and frequency; `beep` is null
- `distance_adjust` with delta_m ±10 moves `distance_offset_counts` by 10 m of counts; `distance_set` with meters N makes the total read N; delta_m 0 or beyond ±1000 and meters outside 0–999,999 are ignored
- State carries `autostart` as "none" or hh:mm and `memory_populated` for the five slots
- Multiple connected phones receive broadcast state after `next`/`prev`, reset, distance adjustment, or segment edit

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

