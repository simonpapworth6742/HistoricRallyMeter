#ifndef RALLY_TYPES_H
#define RALLY_TYPES_H

#include <cstdint>
#include <vector>

#ifndef RALLY_NO_GTK
#include <gtk/gtk.h>
#endif

// Forward declarations
class I2CCounter;
class RallyState;
class CounterPoller;
class ToneGenerator;
class RallyWebServer;

// Segment structure (high precision)
struct Segment {
    double target_speed_kph = 0.0;               // user-entered speed in KPH (calibration-independent)
    double target_speed_counts_per_hour = 0.0;   // counts per hour (recalculated on calibration change)
    double distance_m = 0.0;                      // user-entered distance in meters (calibration-independent)
    double distance_counts = 0.0;                 // distance in counts (recalculated on calibration change)
    bool autoNext = true;
};

// Counter polling data with 10-second rolling average
struct CounterPoll {
    uint64_t cntr1;
    uint64_t cntr2;
    int64_t time_ms;
};

#ifndef RALLY_NO_GTK
// Application data structure (requires GTK)
struct AppData {
    I2CCounter* counter1;
    I2CCounter* counter2;
    uint8_t register_addr;
    RallyState* state;
    CounterPoller* poller;
    
    // Driver window
    GtkWidget* driverWindow;
    GtkLabel* currentSpeedLabel;
    GtkLabel* tripSpeedLabel;
    GtkLabel* totalSpeedLabel;
    GtkLabel* targetSpeedLabel;
    GtkLabel* gaugeTargetLabel;    // target speed shown below gauge
    GtkLabel* aheadBehindLabel;
    GtkLabel* speedAdjustArrowsLabel;
    GtkLabel* nextSegLabel;
    GtkLabel* updatesPerSecLabel;
    GtkLabel* cpuTempLabel;
    GtkLabel* unitsLabel;  // Shows KPH or MPH in header
    GtkButton* unitToggleBtn;
    
    // Rally gauge
    GtkWidget* rallyGaugeDrawingArea;
    double aheadBehindSeconds = 0.0;
    double smoothedSpeed = -1.0;      // EMA-filtered current speed for display
    int gaugeScale = 1;               // 0=±3s(green), 1=±10s(yellow), 2=±5min(red)
    int64_t gaugeScaleChangeTime = 0; // Timestamp of last scale change (2s cooldown)
    double segmentProgress = 0.0;     // fraction (0..1) of the current segment driven
    bool inSegment = false;           // true while within the current segment's distance
    
    // Compact (800x480-style) driver layout: values drawn inside the gauge
    GtkWidget* driverSpeedsBox = nullptr;  // left pane, hidden in compact mode
    bool driverCompactMode = false;

    // Single-display mode: one 1280x400 monitor only; driver window hidden,
    // compact driver display embedded in the TwinMaster right panel
    bool singleDisplayMode = false;
    GtkWidget* copilotGaugeArea = nullptr;
    
    // Co-pilot window
    GtkWidget* copilotWindow;
    GtkStack* copilotStack;  // Stack for multiple screens
    GtkLabel* copilotRallyClockLabel;
    
    // TwinMaster screen
    GtkWidget* twinMasterScreen;
    GtkLabel* totalDistLabel;
    GtkLabel* totalUnitLabel;
    GtkLabel* totalTimeLabel;
    GtkLabel* tripDistLabel;
    GtkLabel* tripUnitLabel;
    GtkLabel* tripTimeLabel;
    GtkLabel* segmentInfoLabel;
    GtkLabel* nextDistLabel;
    GtkLabel* nextUnitLabel;
    GtkLabel* nextSpeedLabel;
    GtkWidget* nextPrevBtn;
    GtkWidget* stageGoBtn = nullptr;
    GtkWidget* toneMuteBtn = nullptr;
    bool tonesMuted = false;          // ahead/behind tones only; not saved
    GtkWidget* adjZeroBtn;
    GtkLabel* alarmCountdownLabel;
    GtkWidget* alarmClearBtn;
    int64_t alarmSoundStartTime = 0;    // when doorbell started (0 = not ringing)

    GtkLabel* webUrlLabel = nullptr;
    GtkWidget* webQrArea = nullptr;
    RallyWebServer* webServer = nullptr;
    
    // Stage setup screen
    GtkWidget* stageSetupScreen;
    GtkWidget* stageSetupMainBox;  // Main horizontal container
    GtkListBox* segmentListBox;
    GtkEntry* targetSpeedEntry;
    GtkEntry* distanceEntry;
    GtkCheckButton* autoNextCheck;
    GtkWidget* numericKeypad;      // Numeric keypad container
    GtkEntry* activeEntry;         // Currently focused entry for keypad input
    GtkWidget* memoryRecallBtns[5] = {};  // Recall buttons for memory slots
    
    // Calibration screen
    GtkWidget* calibrationScreen;
    GtkWidget* calibrationMainBox;  // Main horizontal container for keypad
    GtkLabel* totalDistCalLabel;
    GtkLabel* totalCountCalLabel;
    GtkLabel* sensorCountsLabel = nullptr;  // live CNTR_1 / CNTR_2 above Total distance
    GtkEntry* rallyDistEntry;
    GtkEntry* calNewEntry = nullptr;    // Calculated calibration, linked to the metres entry
    GtkLabel* calRunLabel = nullptr;    // Distance and pulses for the calibration run
    GtkWidget* calStartBtn = nullptr;
    GtkWidget* calStopBtn = nullptr;
    GtkWidget* calSetBtn = nullptr;
    GtkWidget* calibrationKeypad;   // Numeric keypad for calibration
    GtkLabel* sensorModeLabel;      // "Using Sensor 1 only" or "Using Sensor 1&2 adv."
    GtkLabel* calibrationValueLabel = nullptr;
    
    // Calibration run. Start counts up from zero; Stop freezes that count.
    uint64_t cal_start_cntr1 = 0;
    uint64_t cal_start_cntr2 = 0;
    uint64_t cal_frozen_cntr1 = 0;
    uint64_t cal_frozen_cntr2 = 0;
    int64_t cal_pulse_count = 0;
    bool cal_started = false;
    bool cal_running = false;
    bool cal_stopped = false;
    bool cal_syncing = false;  // Programmatic entry updates must not retrigger the other box
    
    // Date/Time setup screen
    GtkWidget* dateTimeScreen;
    GtkWidget* datetimeKeypad;
    GtkLabel* systemClockLabel;
    GtkLabel* systemTimeLabel;
    GtkLabel* rallyClockLabel;
    GtkLabel* rallyTimeLabel;
    GtkEntry* dateEntry;
    GtkEntry* timeEntry;
    
    // Auto Start setup screen
    GtkWidget* autoStartScreen;
    GtkLabel* autoStartRallyClockLabel;
    GtkLabel* autoStartTimeLabel;
    GtkEntry* autoStartTimeEntry;
    GtkWidget* autoStartKeypad;
    
    // Driver countdown overlay
    GtkWidget* countdownOverlay;
    GtkLabel* countdownLabel;
    bool autoStartTriggered = false;
    
    // Tone generator for speed adjustment alerts
    ToneGenerator* toneGen = nullptr;
    
    int updateCount = 0;
    int64_t lastUpdateCountTime_ms = 0;
};
#endif // RALLY_NO_GTK

#endif // RALLY_TYPES_H
