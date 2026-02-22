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

// Segment structure (high precision)
struct Segment {
    double target_speed_counts_per_hour = 0.0;  // counts per hour (high precision)
    double distance_counts = 0.0;                // distance in counts (high precision)
    bool autoNext = false;
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
    GtkLabel* unitsLabel;  // Shows KPH or MPH in header
    GtkButton* unitToggleBtn;
    
    // Rally gauge
    GtkWidget* rallyGaugeDrawingArea;
    double aheadBehindSeconds = 0.0;
    int gaugeScale = 1;               // 0=±3s(green), 1=±10s(yellow), 2=±5min(red)
    int gaugePendingScale = -1;       // Scale we're debouncing toward (-1 = none)
    int64_t gaugeScaleChangeTime = 0; // Timestamp when pending scale was first requested
    
    // Co-pilot window
    GtkWidget* copilotWindow;
    GtkStack* copilotStack;  // Stack for multiple screens
    GtkLabel* copilotRallyClockLabel;
    
    // TwinMaster screen
    GtkWidget* twinMasterScreen;
    GtkLabel* totalDistLabel;
    GtkLabel* tripDistLabel;
    GtkLabel* segmentInfoLabel;
    
    // Stage setup screen
    GtkWidget* stageSetupScreen;
    GtkWidget* stageSetupMainBox;  // Main horizontal container
    GtkListBox* segmentListBox;
    GtkEntry* targetSpeedEntry;
    GtkEntry* distanceEntry;
    GtkCheckButton* autoNextCheck;
    GtkWidget* numericKeypad;      // Numeric keypad container
    GtkEntry* activeEntry;         // Currently focused entry for keypad input
    
    // Calibration screen
    GtkWidget* calibrationScreen;
    GtkWidget* calibrationMainBox;  // Main horizontal container for keypad
    GtkLabel* totalDistCalLabel;
    GtkLabel* totalCountCalLabel;
    GtkEntry* rallyDistEntry;
    GtkWidget* calibrationKeypad;   // Numeric keypad for calibration
    
    // Calibration baseline values (set when "start" is pressed)
    uint64_t cal_start_cntr1 = 0;
    uint64_t cal_start_cntr2 = 0;
    bool cal_started = false;  // True once "start" has been pressed
    
    // Date/Time setup screen
    GtkWidget* dateTimeScreen;
    GtkLabel* systemClockLabel;
    GtkLabel* rallyClockLabel;
    GtkEntry* dateEntry;
    GtkEntry* timeEntry;
    
    int updateCount = 0;
    int64_t lastUpdateCountTime_ms = 0;
};
#endif // RALLY_NO_GTK

#endif // RALLY_TYPES_H
