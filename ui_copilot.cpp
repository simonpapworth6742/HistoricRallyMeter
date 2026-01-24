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

// Apply CSS styling
static void applyCopilotCSS() {
    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        ".title-label { font-size: 20px; font-weight: bold; }"
        ".info-label { font-size: 18px; }"
        ".clock-label { font-size: 24px; font-weight: bold; }"
        ".distance-label { font-size: 22px; font-weight: bold; }"
        ".segment-label { font-size: 18px; }",
        -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

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
    ss << "Total: " << total_m << " m  from " << total_duration << " ago";
    gtk_label_set_text(data->totalDistLabel, ss.str().c_str());
    
    // Trip distance
    int64_t trip_count_diff = calculateDistanceCounts(*data->state,
        current_poll.cntr1, current_poll.cntr2,
        data->state->trip_start_cntr1, data->state->trip_start_cntr2);
    long trip_m = countsToCentimeters(trip_count_diff, data->state->calibration) / 100;
    int64_t trip_duration_ms = current_time_ms - data->state->trip_start_time_ms;
    std::string trip_duration = formatDuration(trip_duration_ms);
    
    ss.str("");
    ss << "Trip: " << trip_m << " m  from " << trip_duration << " ago";
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
           << "  -  next segment in " << remaining_m << " m";
        gtk_label_set_text(data->segmentInfoLabel, ss.str().c_str());
    } else {
        gtk_label_set_text(data->segmentInfoLabel, "No active segment");
    }
}

// Create TwinMaster screen - horizontal layout for 1280x400
GtkWidget* createTwinMasterScreen(AppData* data) {
    GtkWidget* screen = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(screen), 15);
    
    // Row 1: Total and Trip side by side with reset buttons
    GtkWidget* distanceRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 40);
    gtk_box_pack_start(GTK_BOX(screen), distanceRow, TRUE, TRUE, 0);
    
    // Total section
    GtkWidget* totalBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    data->totalDistLabel = GTK_LABEL(gtk_label_new("Total: 0 m  from 00:00:00 ago"));
    GtkWidget* totalResetBtn = gtk_button_new_with_label("reset");
    g_signal_connect(totalResetBtn, "clicked", G_CALLBACK(on_total_reset), data);
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->totalDistLabel)), "distance-label");
    gtk_box_pack_start(GTK_BOX(totalBox), GTK_WIDGET(data->totalDistLabel), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(totalBox), totalResetBtn, FALSE, FALSE, 0);
    
    // Trip section
    GtkWidget* tripBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    data->tripDistLabel = GTK_LABEL(gtk_label_new("Trip: 0 m  from 00:00:00 ago"));
    GtkWidget* tripResetBtn = gtk_button_new_with_label("reset");
    g_signal_connect(tripResetBtn, "clicked", G_CALLBACK(on_trip_reset), data);
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->tripDistLabel)), "distance-label");
    gtk_box_pack_start(GTK_BOX(tripBox), GTK_WIDGET(data->tripDistLabel), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(tripBox), tripResetBtn, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(distanceRow), totalBox, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(distanceRow), tripBox, TRUE, TRUE, 0);
    
    // Row 2: Segment info
    data->segmentInfoLabel = GTK_LABEL(gtk_label_new("No active segment"));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->segmentInfoLabel)), "segment-label");
    gtk_widget_set_halign(GTK_WIDGET(data->segmentInfoLabel), GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(screen), GTK_WIDGET(data->segmentInfoLabel), FALSE, FALSE, 10);
    
    // Row 3: Navigation buttons spread across
    GtkWidget* buttonBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_box_pack_end(GTK_BOX(screen), buttonBox, FALSE, FALSE, 0);
    
    GtkWidget* segmentsBtn = gtk_button_new_with_label("segments");
    GtkWidget* nextSegBtn = gtk_button_new_with_label("next segment");
    GtkWidget* calBtn = gtk_button_new_with_label("calibration");
    GtkWidget* datetimeBtn = gtk_button_new_with_label("date/time");
    
    g_signal_connect(segmentsBtn, "clicked", G_CALLBACK(on_show_segments), data);
    g_signal_connect(nextSegBtn, "clicked", G_CALLBACK(on_next_segment), data);
    g_signal_connect(calBtn, "clicked", G_CALLBACK(on_show_calibration), data);
    g_signal_connect(datetimeBtn, "clicked", G_CALLBACK(on_show_datetime), data);
    
    gtk_box_pack_start(GTK_BOX(buttonBox), segmentsBtn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(buttonBox), nextSegBtn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(buttonBox), calBtn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(buttonBox), datetimeBtn, TRUE, TRUE, 0);
    
    return screen;
}

// Create Stage Setup screen - horizontal layout for 1280x400
GtkWidget* createStageSetupScreen(AppData* data) {
    GtkWidget* screen = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(screen), 15);
    
    // Title
    GtkWidget* titleLabel = gtk_label_new("STAGE SETUP");
    gtk_style_context_add_class(gtk_widget_get_style_context(titleLabel), "title-label");
    gtk_box_pack_start(GTK_BOX(screen), titleLabel, FALSE, FALSE, 0);
    
    // Header row
    GtkWidget* headerBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(screen), headerBox, FALSE, FALSE, 5);
    
    GtkWidget* header1 = gtk_label_new("Speed (KPH)");
    GtkWidget* header2 = gtk_label_new("Distance (m)");
    GtkWidget* header3 = gtk_label_new("Auto");
    GtkWidget* header4 = gtk_label_new("Action");
    
    gtk_widget_set_size_request(header1, 150, -1);
    gtk_widget_set_size_request(header2, 150, -1);
    gtk_widget_set_size_request(header3, 80, -1);
    gtk_widget_set_size_request(header4, 80, -1);
    
    gtk_box_pack_start(GTK_BOX(headerBox), header1, FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(headerBox), header2, FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(headerBox), header3, FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(headerBox), header4, FALSE, FALSE, 10);
    
    // Scrollable list for segments
    GtkWidget* scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), 
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(screen), scrolled, TRUE, TRUE, 0);
    
    data->segmentListBox = GTK_LIST_BOX(gtk_list_box_new());
    gtk_container_add(GTK_CONTAINER(scrolled), GTK_WIDGET(data->segmentListBox));
    
    // Add new segment row - spread horizontally
    GtkWidget* addBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(screen), addBox, FALSE, FALSE, 10);
    
    GtkWidget* newLabel = gtk_label_new("New:");
    data->targetSpeedEntry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(data->targetSpeedEntry, "Speed KPH");
    gtk_widget_set_size_request(GTK_WIDGET(data->targetSpeedEntry), 120, -1);
    
    data->distanceEntry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(data->distanceEntry, "Distance m");
    gtk_widget_set_size_request(GTK_WIDGET(data->distanceEntry), 120, -1);
    
    data->autoNextCheck = GTK_CHECK_BUTTON(gtk_check_button_new_with_label("Auto"));
    
    GtkWidget* addBtn = gtk_button_new_with_label("add");
    g_signal_connect(addBtn, "clicked", G_CALLBACK(on_add_segment), data);
    
    GtkWidget* backBtn = gtk_button_new_with_label("back");
    g_signal_connect(backBtn, "clicked", G_CALLBACK(on_show_twinmaster), data);
    
    gtk_box_pack_start(GTK_BOX(addBox), newLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(addBox), GTK_WIDGET(data->targetSpeedEntry), FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(addBox), GTK_WIDGET(data->distanceEntry), FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(addBox), GTK_WIDGET(data->autoNextCheck), FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(addBox), addBtn, FALSE, FALSE, 10);
    gtk_box_pack_end(GTK_BOX(addBox), backBtn, FALSE, FALSE, 10);
    
    return screen;
}

// Create Calibration screen - horizontal layout for 1280x400
GtkWidget* createCalibrationScreen(AppData* data) {
    GtkWidget* screen = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_container_set_border_width(GTK_CONTAINER(screen), 15);
    
    // Title
    GtkWidget* titleLabel = gtk_label_new("CALIBRATION");
    gtk_style_context_add_class(gtk_widget_get_style_context(titleLabel), "title-label");
    gtk_box_pack_start(GTK_BOX(screen), titleLabel, FALSE, FALSE, 0);
    
    // Row 1: Distance and counts side by side
    GtkWidget* infoRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 40);
    gtk_box_pack_start(GTK_BOX(screen), infoRow, TRUE, TRUE, 0);
    
    data->totalDistCalLabel = GTK_LABEL(gtk_label_new("Total distance: 0 m"));
    data->totalCountCalLabel = GTK_LABEL(gtk_label_new("Total counts: 0"));
    
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->totalDistCalLabel)), "info-label");
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->totalCountCalLabel)), "info-label");
    
    gtk_box_pack_start(GTK_BOX(infoRow), GTK_WIDGET(data->totalDistCalLabel), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(infoRow), GTK_WIDGET(data->totalCountCalLabel), TRUE, TRUE, 0);
    
    // Row 2: Input field spread across
    GtkWidget* inputRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(screen), inputRow, FALSE, FALSE, 10);
    
    GtkWidget* inputLabel = gtk_label_new("Actual distance covered:");
    data->rallyDistEntry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(data->rallyDistEntry, "meters");
    gtk_widget_set_size_request(GTK_WIDGET(data->rallyDistEntry), 200, -1);
    GtkWidget* unitLabel = gtk_label_new("meters");
    
    gtk_box_pack_start(GTK_BOX(inputRow), inputLabel, FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(inputRow), GTK_WIDGET(data->rallyDistEntry), FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(inputRow), unitLabel, FALSE, FALSE, 0);
    
    // Row 3: Buttons
    GtkWidget* buttonBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_box_pack_end(GTK_BOX(screen), buttonBox, FALSE, FALSE, 0);
    
    GtkWidget* saveBtn = gtk_button_new_with_label("save");
    GtkWidget* backBtn = gtk_button_new_with_label("back");
    
    g_signal_connect(saveBtn, "clicked", G_CALLBACK(on_save_calibration), data);
    g_signal_connect(backBtn, "clicked", G_CALLBACK(on_show_twinmaster), data);
    
    gtk_box_pack_start(GTK_BOX(buttonBox), saveBtn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(buttonBox), backBtn, TRUE, TRUE, 0);
    
    return screen;
}

// Create Date/Time Setup screen - horizontal layout for 1280x400
GtkWidget* createDateTimeScreen(AppData* data) {
    GtkWidget* screen = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_container_set_border_width(GTK_CONTAINER(screen), 15);
    
    // Title
    GtkWidget* titleLabel = gtk_label_new("DATE/TIME SETUP");
    gtk_style_context_add_class(gtk_widget_get_style_context(titleLabel), "title-label");
    gtk_box_pack_start(GTK_BOX(screen), titleLabel, FALSE, FALSE, 0);
    
    // Row 1: System and Rally clock side by side
    GtkWidget* clockRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 40);
    gtk_box_pack_start(GTK_BOX(screen), clockRow, TRUE, TRUE, 0);
    
    // System clock section
    GtkWidget* sysBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget* sysLabel = gtk_label_new("System Clock:");
    data->systemClockLabel = GTK_LABEL(gtk_label_new(""));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->systemClockLabel)), "info-label");
    gtk_box_pack_start(GTK_BOX(sysBox), sysLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sysBox), GTK_WIDGET(data->systemClockLabel), TRUE, TRUE, 0);
    
    // Rally clock section
    GtkWidget* rallyBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget* rallyLabel = gtk_label_new("Rally Clock:");
    data->rallyClockLabel = GTK_LABEL(gtk_label_new(""));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->rallyClockLabel)), "info-label");
    gtk_box_pack_start(GTK_BOX(rallyBox), rallyLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(rallyBox), GTK_WIDGET(data->rallyClockLabel), TRUE, TRUE, 0);
    
    gtk_box_pack_start(GTK_BOX(clockRow), sysBox, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(clockRow), rallyBox, TRUE, TRUE, 0);
    
    // Row 2: Input fields spread horizontally
    GtkWidget* inputRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_box_pack_start(GTK_BOX(screen), inputRow, FALSE, FALSE, 10);
    
    GtkWidget* setLabel = gtk_label_new("Set Rally Time:");
    GtkWidget* dateLabel = gtk_label_new("Date:");
    data->dateEntry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(data->dateEntry, "yyyy/mm/dd");
    gtk_widget_set_size_request(GTK_WIDGET(data->dateEntry), 150, -1);
    
    GtkWidget* timeLabel = gtk_label_new("Time:");
    data->timeEntry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(data->timeEntry, "hh:mm:ss");
    gtk_widget_set_size_request(GTK_WIDGET(data->timeEntry), 120, -1);
    
    gtk_box_pack_start(GTK_BOX(inputRow), setLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inputRow), dateLabel, FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(inputRow), GTK_WIDGET(data->dateEntry), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inputRow), timeLabel, FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(inputRow), GTK_WIDGET(data->timeEntry), FALSE, FALSE, 0);
    
    // Row 3: Buttons
    GtkWidget* buttonBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_box_pack_end(GTK_BOX(screen), buttonBox, FALSE, FALSE, 0);
    
    GtkWidget* saveBtn = gtk_button_new_with_label("set and save");
    GtkWidget* backBtn = gtk_button_new_with_label("back");
    
    g_signal_connect(saveBtn, "clicked", G_CALLBACK(on_save_datetime), data);
    g_signal_connect(backBtn, "clicked", G_CALLBACK(on_show_twinmaster), data);
    
    gtk_box_pack_start(GTK_BOX(buttonBox), saveBtn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(buttonBox), backBtn, TRUE, TRUE, 0);
    
    return screen;
}

GtkWidget* createCopilotWindow(AppData* data) {
    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Co-Pilot Display");
    gtk_window_set_default_size(GTK_WINDOW(window), 1280, 400);
    
    applyCopilotCSS();
    
    GtkWidget* mainBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), mainBox);
    
    // Top bar with rally clock on right
    GtkWidget* topBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(topBar), 10);
    gtk_box_pack_start(GTK_BOX(mainBox), topBar, FALSE, FALSE, 0);
    
    data->copilotRallyClockLabel = GTK_LABEL(gtk_label_new("00:00:00"));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->copilotRallyClockLabel)), "clock-label");
    gtk_widget_set_halign(GTK_WIDGET(data->copilotRallyClockLabel), GTK_ALIGN_END);
    gtk_box_pack_end(GTK_BOX(topBar), GTK_WIDGET(data->copilotRallyClockLabel), FALSE, FALSE, 0);
    
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
