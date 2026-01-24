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

// Segment structure
struct Segment {
    long target_speed_counts_per_hour = 0;  // counts per hour
    long distance_counts = 0;               // distance in counts
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
    GtkLabel* segSpeedLabel;
    GtkLabel* totalSpeedLabel;
    GtkLabel* targetSpeedLabel;
    GtkLabel* aheadBehindLabel;
    GtkLabel* nextSegLabel;
    GtkLabel* updatesPerSecLabel;
    GtkLabel* unitsLabel;  // Shows KPH or MPH in header
    GtkButton* unitToggleBtn;
    
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
    GtkListBox* segmentListBox;
    GtkEntry* targetSpeedEntry;
    GtkEntry* distanceEntry;
    GtkCheckButton* autoNextCheck;
    
    // Calibration screen
    GtkWidget* calibrationScreen;
    GtkLabel* totalDistCalLabel;
    GtkLabel* totalCountCalLabel;
    GtkEntry* rallyDistEntry;
    
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
