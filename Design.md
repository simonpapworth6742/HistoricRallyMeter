## Environment
This environment is a Raspberry Pi 5 with 4GB memory connected to 3 LSI ls7866c 32-bit counters (CNTR_1, CNTR_2, CNTR_3) on I2C bus 1 at addresses 0x70, 0x71 and 0x72 The 32-bit counter register 0x07 and its value is big-endian. the application should be 
    GTK3-based GUI window application with two display windows. 
    Use high-resolution chrono timers for accurate measurement
    C++  version 20

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

                             Trip   xxx,xxx m from hhh:mm:ss ago     reset

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

 
