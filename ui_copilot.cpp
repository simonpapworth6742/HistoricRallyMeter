#include "ui_copilot.h"
#include "calculations.h"
#include "rally_types.h"
#include "rally_state.h"
#include "counter_poller.h"
#include "callbacks.h"
#include "config_file.h"
#include "calculations.h"
#include <sstream>
#include <iomanip>
#include <ctime>

void updateCopilotDisplay(AppData* data) {
    auto current_poll = data->poller->getMostRecent();
    auto current_time_ms = getRallyTime_ms(*data->state);
    
    // Rally clock
    std::string rally_time = formatTime(current_time_ms);
    gtk_label_set_text(data->copilotRallyClockLabel, rally_time.c_str());
    
    // Only update if TwinMaster screen is visible
    GtkWidget* visible_child = gtk_stack_get_visible_child(data->copilotStack);
    if (visible_child != data->twinMasterScreen) {
        return;
    }
    
    // Total distance
    int64_t total_count_diff = calculateDistanceCounts(*data->state,
        current_poll.cntr1, current_poll.cntr2,
        data->state->total_start_cntr1, data->state->total_start_cntr2);
    long total_m = countsToCentimeters(total_count_diff, data->state->calibration) / 100;
    int64_t total_duration_ms = current_time_ms - data->state->total_start_time_ms;
    std::string total_duration = formatDuration(total_duration_ms);
    
    std::stringstream ss;
    ss << "Total " << total_m << " m from " << total_duration << " ago";
    gtk_label_set_text(data->totalDistLabel, ss.str().c_str());
    
    // Trip distance
    int64_t trip_count_diff = calculateDistanceCounts(*data->state,
        current_poll.cntr1, current_poll.cntr2,
        data->state->trip_start_cntr1, data->state->trip_start_cntr2);
    long trip_m = countsToCentimeters(trip_count_diff, data->state->calibration) / 100;
    int64_t trip_duration_ms = current_time_ms - data->state->trip_start_time_ms;
    std::string trip_duration = formatDuration(trip_duration_ms);
    
    ss.str("");
    ss << "Trip " << trip_m << " m from " << trip_duration << " ago";
    gtk_label_set_text(data->tripDistLabel, ss.str().c_str());
    
    // Segment info
    if (data->state->segment_current_number >= 0 && 
        data->state->segment_current_number < static_cast<long>(data->state->segments.size())) {
        const Segment& seg = data->state->segments[data->state->segment_current_number];
        int64_t seg_count_diff = calculateDistanceCounts(*data->state,
            current_poll.cntr1, current_poll.cntr2,
            data->state->segment_start_cntr1, data->state->segment_start_cntr2);
        int64_t remaining_counts = seg.distance_counts - seg_count_diff;
        long remaining_m = countsToCentimeters(remaining_counts, data->state->calibration) / 100;
        
        ss.str("");
        ss << "Segment " << (data->state->segment_current_number + 1) 
           << " – next segment in " << remaining_m << " m";
        gtk_label_set_text(data->segmentInfoLabel, ss.str().c_str());
    } else {
        gtk_label_set_text(data->segmentInfoLabel, "No active segment");
    }
}

// Create TwinMaster screen
GtkWidget* createTwinMasterScreen(AppData* data) {
    GtkWidget* screen = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(screen), 20);
    
    // Total distance
    GtkWidget* totalBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(screen), totalBox, FALSE, FALSE, 0);
    
    data->totalDistLabel = GTK_LABEL(gtk_label_new("Total 0 m from 00:00:00 ago"));
    GtkWidget* totalResetBtn = gtk_button_new_with_label("reset");
    g_signal_connect(totalResetBtn, "clicked", G_CALLBACK(on_total_reset), data);
    
    gtk_box_pack_start(GTK_BOX(totalBox), GTK_WIDGET(data->totalDistLabel), TRUE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(totalBox), totalResetBtn, FALSE, FALSE, 0);
    
    // Trip distance
    GtkWidget* tripBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(screen), tripBox, FALSE, FALSE, 0);
    
    data->tripDistLabel = GTK_LABEL(gtk_label_new("Trip 0 m from 00:00:00 ago"));
    GtkWidget* tripResetBtn = gtk_button_new_with_label("reset");
    g_signal_connect(tripResetBtn, "clicked", G_CALLBACK(on_trip_reset), data);
    
    gtk_box_pack_start(GTK_BOX(tripBox), GTK_WIDGET(data->tripDistLabel), TRUE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(tripBox), tripResetBtn, FALSE, FALSE, 0);
    
    // Segment info
    data->segmentInfoLabel = GTK_LABEL(gtk_label_new("No active segment"));
    gtk_box_pack_start(GTK_BOX(screen), GTK_WIDGET(data->segmentInfoLabel), FALSE, FALSE, 0);
    
    // Buttons
    GtkWidget* buttonBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(screen), buttonBox, FALSE, FALSE, 10);
    
    GtkWidget* segmentsBtn = gtk_button_new_with_label("segments");
    GtkWidget* nextSegBtn = gtk_button_new_with_label("next segment");
    GtkWidget* calBtn = gtk_button_new_with_label("calibration");
    GtkWidget* datetimeBtn = gtk_button_new_with_label("date/time");
    
    g_signal_connect(segmentsBtn, "clicked", G_CALLBACK(on_show_segments), data);
    g_signal_connect(nextSegBtn, "clicked", G_CALLBACK(on_next_segment), data);
    g_signal_connect(calBtn, "clicked", G_CALLBACK(on_show_calibration), data);
    g_signal_connect(datetimeBtn, "clicked", G_CALLBACK(on_show_datetime), data);
    
    gtk_box_pack_start(GTK_BOX(buttonBox), segmentsBtn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(buttonBox), nextSegBtn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(buttonBox), calBtn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(buttonBox), datetimeBtn, FALSE, FALSE, 0);
    
    return screen;
}

// Create Stage Setup screen
GtkWidget* createStageSetupScreen(AppData* data) {
    GtkWidget* screen = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(screen), 20);
    
    // Header
    GtkWidget* headerBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(screen), headerBox, FALSE, FALSE, 0);
    
    GtkWidget* header1 = gtk_label_new("Target avg. speed");
    GtkWidget* header2 = gtk_label_new("Distance(m)");
    GtkWidget* header3 = gtk_label_new("AutoNext");
    GtkWidget* header4 = gtk_label_new("");
    
    gtk_box_pack_start(GTK_BOX(headerBox), header1, TRUE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(headerBox), header2, TRUE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(headerBox), header3, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(headerBox), header4, FALSE, FALSE, 0);
    
    // List box for segments
    data->segmentListBox = GTK_LIST_BOX(gtk_list_box_new());
    gtk_box_pack_start(GTK_BOX(screen), GTK_WIDGET(data->segmentListBox), TRUE, TRUE, 0);
    
    // Refresh segment list
    // (Will be called when screen is shown)
    
    // Add new segment controls
    GtkWidget* addBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(screen), addBox, FALSE, FALSE, 0);
    
    data->targetSpeedEntry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(data->targetSpeedEntry, "KPH");
    data->distanceEntry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(data->distanceEntry, "meters");
    data->autoNextCheck = GTK_CHECK_BUTTON(gtk_check_button_new_with_label("Auto"));
    
    GtkWidget* addBtn = gtk_button_new_with_label("add more");
    g_signal_connect(addBtn, "clicked", G_CALLBACK(on_add_segment), data);
    
    gtk_box_pack_start(GTK_BOX(addBox), GTK_WIDGET(data->targetSpeedEntry), TRUE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(addBox), GTK_WIDGET(data->distanceEntry), TRUE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(addBox), GTK_WIDGET(data->autoNextCheck), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(addBox), addBtn, FALSE, FALSE, 0);
    
    // Back button
    GtkWidget* backBtn = gtk_button_new_with_label("back");
    g_signal_connect(backBtn, "clicked", G_CALLBACK(on_show_twinmaster), data);
    gtk_box_pack_start(GTK_BOX(screen), backBtn, FALSE, FALSE, 0);
    
    return screen;
}

// Create Calibration screen
GtkWidget* createCalibrationScreen(AppData* data) {
    GtkWidget* screen = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(screen), 20);
    
    // Total distance display
    data->totalDistCalLabel = GTK_LABEL(gtk_label_new("Total distance 0 m (0 count)"));
    gtk_box_pack_start(GTK_BOX(screen), GTK_WIDGET(data->totalDistCalLabel), FALSE, FALSE, 0);
    
    data->totalCountCalLabel = GTK_LABEL(gtk_label_new(""));
    gtk_box_pack_start(GTK_BOX(screen), GTK_WIDGET(data->totalCountCalLabel), FALSE, FALSE, 0);
    
    // Input field
    GtkWidget* inputBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(screen), inputBox, FALSE, FALSE, 0);
    
    GtkWidget* inputLabel = gtk_label_new("Input Rally distance actually covered");
    data->rallyDistEntry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(data->rallyDistEntry, "meters");
    GtkWidget* unitLabel = gtk_label_new("meters");
    
    gtk_box_pack_start(GTK_BOX(inputBox), inputLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inputBox), GTK_WIDGET(data->rallyDistEntry), TRUE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inputBox), unitLabel, FALSE, FALSE, 0);
    
    // Buttons
    GtkWidget* buttonBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(screen), buttonBox, FALSE, FALSE, 10);
    
    GtkWidget* saveBtn = gtk_button_new_with_label("save");
    GtkWidget* backBtn = gtk_button_new_with_label("back");
    
    g_signal_connect(saveBtn, "clicked", G_CALLBACK(on_save_calibration), data);
    g_signal_connect(backBtn, "clicked", G_CALLBACK(on_show_twinmaster), data);
    
    gtk_box_pack_start(GTK_BOX(buttonBox), saveBtn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(buttonBox), backBtn, FALSE, FALSE, 0);
    
    return screen;
}

// Create Date/Time Setup screen
GtkWidget* createDateTimeScreen(AppData* data) {
    GtkWidget* screen = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(screen), 20);
    
    // System clock display
    GtkWidget* sysBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(screen), sysBox, FALSE, FALSE, 0);
    
    GtkWidget* sysLabel = gtk_label_new("System Clock");
    data->systemClockLabel = GTK_LABEL(gtk_label_new(""));
    gtk_box_pack_start(GTK_BOX(sysBox), sysLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sysBox), GTK_WIDGET(data->systemClockLabel), TRUE, FALSE, 0);
    
    // Rally clock display
    GtkWidget* rallyBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(screen), rallyBox, FALSE, FALSE, 0);
    
    GtkWidget* rallyLabel = gtk_label_new("RallyClock");
    data->rallyClockLabel = GTK_LABEL(gtk_label_new(""));
    gtk_box_pack_start(GTK_BOX(rallyBox), rallyLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(rallyBox), GTK_WIDGET(data->rallyClockLabel), TRUE, FALSE, 0);
    
    // Input fields
    GtkWidget* inputBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(screen), inputBox, FALSE, FALSE, 10);
    
    GtkWidget* dateLabel = gtk_label_new("Date (yyyy/mm/dd):");
    data->dateEntry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(data->dateEntry, "yyyy/mm/dd");
    
    GtkWidget* timeLabel = gtk_label_new("Time (hh:mm:ss):");
    data->timeEntry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(data->timeEntry, "hh:mm:ss");
    
    gtk_box_pack_start(GTK_BOX(inputBox), dateLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inputBox), GTK_WIDGET(data->dateEntry), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inputBox), timeLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inputBox), GTK_WIDGET(data->timeEntry), FALSE, FALSE, 0);
    
    // Buttons
    GtkWidget* buttonBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(screen), buttonBox, FALSE, FALSE, 10);
    
    GtkWidget* saveBtn = gtk_button_new_with_label("set and save");
    GtkWidget* backBtn = gtk_button_new_with_label("back");
    
    g_signal_connect(saveBtn, "clicked", G_CALLBACK(on_save_datetime), data);
    g_signal_connect(backBtn, "clicked", G_CALLBACK(on_show_twinmaster), data);
    
    gtk_box_pack_start(GTK_BOX(buttonBox), saveBtn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(buttonBox), backBtn, FALSE, FALSE, 0);
    
    return screen;
}

GtkWidget* createCopilotWindow(AppData* data) {
    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Co-Pilot Display");
    // Default size matches waveshare 400x1280 display; may be overridden in main.cpp
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 1280);
    
    GtkWidget* mainBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), mainBox);
    
    // Rally clock (top right)
    GtkWidget* clockBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(clockBox), 10);
    gtk_box_pack_start(GTK_BOX(mainBox), clockBox, FALSE, FALSE, 0);
    
    data->copilotRallyClockLabel = GTK_LABEL(gtk_label_new("00:00:00"));
    gtk_widget_set_halign(GTK_WIDGET(data->copilotRallyClockLabel), GTK_ALIGN_END);
    gtk_box_pack_end(GTK_BOX(clockBox), GTK_WIDGET(data->copilotRallyClockLabel), FALSE, FALSE, 0);
    
    // Create stack for screens
    data->copilotStack = GTK_STACK(gtk_stack_new());
    gtk_box_pack_start(GTK_BOX(mainBox), GTK_WIDGET(data->copilotStack), TRUE, TRUE, 0);
    
    // Create all screens
    data->twinMasterScreen = createTwinMasterScreen(data);
    data->stageSetupScreen = createStageSetupScreen(data);
    data->calibrationScreen = createCalibrationScreen(data);
    data->dateTimeScreen = createDateTimeScreen(data);
    
    // Add screens to stack
    gtk_stack_add_named(data->copilotStack, data->twinMasterScreen, "twinmaster");
    gtk_stack_add_named(data->copilotStack, data->stageSetupScreen, "stagesetup");
    gtk_stack_add_named(data->copilotStack, data->calibrationScreen, "calibration");
    gtk_stack_add_named(data->copilotStack, data->dateTimeScreen, "datetime");
    
    // Show TwinMaster by default
    gtk_stack_set_visible_child_name(data->copilotStack, "twinmaster");
    
    return window;
}
