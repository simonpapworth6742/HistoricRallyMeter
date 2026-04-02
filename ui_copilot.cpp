#include "ui_copilot.h"
#include "calculations.h"
#include "rally_types.h"
#include "rally_state.h"
#include "counter_poller.h"
#include "callbacks.h"
#include "config_file.h"
#include "calculations.h"
#include <sstream>
#include <cstdlib>
#include <iomanip>
#include <ctime>
#include <string>

// Apply CSS styling
static void applyCopilotCSS() {
    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        "window, .background { background-color: #000000; }"
        "label { color: #FFFFFF; }"
        "button { background-color: #333333; color: #FFFFFF; }"
        "entry { background-color: #222222; color: #FFFFFF; }"
        ".title-label { font-size: 20px; font-weight: bold; }"
        ".info-label { font-size: 18px; }"
        ".clock-label { font-size: 24px; font-weight: bold; }"
        ".total-label { font-size: 48px; font-weight: bold; font-family: monospace; }"
        ".trip-label { font-size: 48px; font-weight: bold; font-family: monospace; }"
        ".alarm-label { font-size: 18px; }"
        ".alarm-button { font-size: 20px; }"
        ".alarm-countdown { font-size: 24px; font-weight: bold; color: #FF6600; font-family: monospace; }"
        ".nav-button { font-size: 18px; }"
        ".segment-label { font-size: 18px; }"
        ".segment-row entry, .segment-row button, .segment-row checkbutton { font-size: 18px; }"
        ".new-segment-row label, .new-segment-row entry, .new-segment-row button, .new-segment-row checkbutton { font-size: 18px; }",
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
    
    // Alarm check runs regardless of which screen is visible
    int64_t total_count_diff = calculateDistanceCounts(*data->state,
        current_poll.cntr1, current_poll.cntr2,
        data->state->total_start_cntr1, data->state->total_start_cntr2);
    
    if (data->state->alarm_distance_km > 0) {
        gtk_widget_show(data->alarmClearBtn);
        int64_t remaining_counts = data->state->alarm_target_counts - total_count_diff;
        long remaining_m = countsToCentimeters(remaining_counts, data->state->calibration) / 100;
        
        if (remaining_m <= 0) {
            if (data->alarmSoundStartTime == 0) {
                data->alarmSoundStartTime = current_time_ms;
                system("aplay alarm.wav &");
            }
            gtk_label_set_text(data->alarmCountdownLabel, "ALARM!");
            
            if (current_time_ms - data->alarmSoundStartTime > 5000) {
                data->state->alarm_distance_km = 0;
                data->state->alarm_target_counts = 0;
                data->alarmSoundStartTime = 0;
                gtk_label_set_text(data->alarmCountdownLabel, "");
                gtk_widget_hide(data->alarmClearBtn);
                ConfigFile::save(*data->state);
            }
        } else {
            std::stringstream alarmSs;
            alarmSs << remaining_m << " m to alarm";
            gtk_label_set_text(data->alarmCountdownLabel, alarmSs.str().c_str());
        }
    } else {
        gtk_label_set_text(data->alarmCountdownLabel, "");
        gtk_widget_hide(data->alarmClearBtn);
    }
    
    // Check which screen is visible
    GtkWidget* visible_child = gtk_stack_get_visible_child(data->copilotStack);
    
    // Update calibration screen if visible (continuous updates per design)
    if (visible_child == data->calibrationScreen) {
        updateCalibrationDisplay(data);
        return;
    }
    
    // Update date/time screen if visible (for live clock display)
    if (visible_child == data->dateTimeScreen) {
        updateDateTimeDisplay(data);
        return;
    }
    
    // Only update TwinMaster if it's visible
    if (visible_child != data->twinMasterScreen) {
        return;
    }
    
    // Total distance
    long total_m = countsToCentimeters(total_count_diff, data->state->calibration) / 100;
    int64_t total_duration_ms = current_time_ms - data->state->total_start_time_ms;
    int total_secs = static_cast<int>(total_duration_ms / 1000);
    
    std::stringstream ss;
    ss << "Total: " << total_m << " m in "
       << std::setw(2) << std::setfill('0') << (total_secs / 60) << ":"
       << std::setw(2) << std::setfill('0') << (total_secs % 60);
    gtk_label_set_text(data->totalDistLabel, ss.str().c_str());
    
    // Trip distance
    int64_t trip_count_diff = calculateDistanceCounts(*data->state,
        current_poll.cntr1, current_poll.cntr2,
        data->state->trip_start_cntr1, data->state->trip_start_cntr2);
    long trip_m = countsToCentimeters(trip_count_diff, data->state->calibration) / 100;
    int64_t trip_duration_ms = current_time_ms - data->state->trip_start_time_ms;
    int trip_secs = static_cast<int>(trip_duration_ms / 1000);
    
    ss.str("");
    ss << std::setfill(' ') << "Trip:  " << trip_m << " m in "
       << std::setw(2) << std::setfill('0') << (trip_secs / 60) << ":"
       << std::setw(2) << std::setfill('0') << (trip_secs % 60);
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
    GtkWidget* screen = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(screen), 15);
    
    // Row 1: Segment info on left, rally clock on right
    GtkWidget* topRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(screen), topRow, FALSE, FALSE, 0);
    
    data->segmentInfoLabel = GTK_LABEL(gtk_label_new("No active segment"));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->segmentInfoLabel)), "segment-label");
    gtk_widget_set_halign(GTK_WIDGET(data->segmentInfoLabel), GTK_ALIGN_START);
    
    gtk_box_pack_start(GTK_BOX(topRow), GTK_WIDGET(data->segmentInfoLabel), TRUE, TRUE, 0);
    gtk_label_set_width_chars(data->copilotRallyClockLabel, 8);
    gtk_label_set_xalign(data->copilotRallyClockLabel, 1.0);
    gtk_box_pack_end(GTK_BOX(topRow), GTK_WIDGET(data->copilotRallyClockLabel), FALSE, FALSE, 10);
    
    // Middle section: Total + Trip grouped together, positioned in upper portion
    GtkWidget* middleSection = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_top(middleSection, 40);
    gtk_box_pack_start(GTK_BOX(screen), middleSection, FALSE, FALSE, 0);
    
    // Row 2: Total area - left side has Total label + reset, right side has alarm buttons in two rows
    GtkWidget* totalArea = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
    gtk_box_pack_start(GTK_BOX(middleSection), totalArea, FALSE, FALSE, 0);
    
    // Left: Total + reset (vertically centered)
    GtkWidget* totalLeft = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
    gtk_widget_set_valign(totalLeft, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(totalArea), totalLeft, FALSE, FALSE, 0);
    
    data->totalDistLabel = GTK_LABEL(gtk_label_new("Total: 0 m in 00:00"));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->totalDistLabel)), "total-label");
    gtk_label_set_width_chars(data->totalDistLabel, 22);
    gtk_label_set_xalign(data->totalDistLabel, 0.0);
    gtk_box_pack_start(GTK_BOX(totalLeft), GTK_WIDGET(data->totalDistLabel), FALSE, FALSE, 0);
    
    GtkWidget* totalResetBtn = gtk_button_new_with_label("reset");
    gtk_style_context_add_class(gtk_widget_get_style_context(totalResetBtn), "alarm-button");
    g_signal_connect(totalResetBtn, "clicked", G_CALLBACK(on_total_reset), data);
    gtk_box_pack_start(GTK_BOX(totalLeft), totalResetBtn, FALSE, FALSE, 5);
    
    // Right: Alarm buttons in two rows
    GtkWidget* alarmBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_valign(alarmBox, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(totalArea), alarmBox, FALSE, FALSE, 15);
    
    // First row: "Alarm in" + [2]-[7]
    GtkWidget* alarmRow1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
    gtk_box_pack_start(GTK_BOX(alarmBox), alarmRow1, FALSE, FALSE, 0);
    
    GtkWidget* alarmLabel = gtk_label_new("Alarm in");
    gtk_style_context_add_class(gtk_widget_get_style_context(alarmLabel), "alarm-label");
    gtk_box_pack_start(GTK_BOX(alarmRow1), alarmLabel, FALSE, FALSE, 3);
    
    for (int km = 2; km <= 7; km++) {
        GtkWidget* btn = gtk_button_new_with_label(std::to_string(km).c_str());
        gtk_style_context_add_class(gtk_widget_get_style_context(btn), "alarm-button");
        gtk_widget_set_size_request(btn, 48, 36);
        g_object_set_data(G_OBJECT(btn), "km", GINT_TO_POINTER(km));
        g_signal_connect(btn, "clicked", G_CALLBACK(on_alarm_set), data);
        gtk_box_pack_start(GTK_BOX(alarmRow1), btn, FALSE, FALSE, 2);
    }
    
    // Second row: [8]-[13] aligned under the buttons
    GtkWidget* alarmRow2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
    gtk_box_pack_start(GTK_BOX(alarmBox), alarmRow2, FALSE, FALSE, 0);
    
    GtkWidget* alarmSpacer = gtk_label_new("");
    gtk_style_context_add_class(gtk_widget_get_style_context(alarmSpacer), "alarm-label");
    gtk_widget_set_size_request(alarmSpacer, 60, -1);
    gtk_box_pack_start(GTK_BOX(alarmRow2), alarmSpacer, FALSE, FALSE, 3);
    
    for (int km = 8; km <= 13; km++) {
        GtkWidget* btn = gtk_button_new_with_label(std::to_string(km).c_str());
        gtk_style_context_add_class(gtk_widget_get_style_context(btn), "alarm-button");
        gtk_widget_set_size_request(btn, 48, 36);
        g_object_set_data(G_OBJECT(btn), "km", GINT_TO_POINTER(km));
        g_signal_connect(btn, "clicked", G_CALLBACK(on_alarm_set), data);
        gtk_box_pack_start(GTK_BOX(alarmRow2), btn, FALSE, FALSE, 2);
    }
    
    // Row 3: Trip with reset, alarm countdown, and clear button
    GtkWidget* tripRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
    gtk_widget_set_margin_top(tripRow, 60);
    gtk_box_pack_start(GTK_BOX(middleSection), tripRow, FALSE, FALSE, 0);
    
    data->tripDistLabel = GTK_LABEL(gtk_label_new("Trip:  0 m in 00:00"));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->tripDistLabel)), "trip-label");
    gtk_label_set_width_chars(data->tripDistLabel, 22);
    gtk_label_set_xalign(data->tripDistLabel, 0.0);
    gtk_box_pack_start(GTK_BOX(tripRow), GTK_WIDGET(data->tripDistLabel), FALSE, FALSE, 0);
    
    GtkWidget* tripResetBtn = gtk_button_new_with_label("reset");
    gtk_style_context_add_class(gtk_widget_get_style_context(tripResetBtn), "alarm-button");
    g_signal_connect(tripResetBtn, "clicked", G_CALLBACK(on_trip_reset), data);
    gtk_box_pack_start(GTK_BOX(tripRow), tripResetBtn, FALSE, FALSE, 5);
    
    data->alarmCountdownLabel = GTK_LABEL(gtk_label_new(""));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->alarmCountdownLabel)), "alarm-countdown");
    gtk_box_pack_start(GTK_BOX(tripRow), GTK_WIDGET(data->alarmCountdownLabel), FALSE, FALSE, 15);
    
    data->alarmClearBtn = gtk_button_new_with_label("clear");
    gtk_style_context_add_class(gtk_widget_get_style_context(data->alarmClearBtn), "alarm-button");
    g_signal_connect(data->alarmClearBtn, "clicked", G_CALLBACK(on_alarm_clear), data);
    gtk_box_pack_start(GTK_BOX(tripRow), data->alarmClearBtn, FALSE, FALSE, 3);
    gtk_widget_set_no_show_all(data->alarmClearBtn, TRUE);
    
    // Row 4: Navigation buttons spread across (with stage go)
    GtkWidget* buttonBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    gtk_box_pack_end(GTK_BOX(screen), buttonBox, FALSE, FALSE, 0);
    
    GtkWidget* stageGoBtn = gtk_button_new_with_label("stage go");
    GtkWidget* segmentsBtn = gtk_button_new_with_label("segments");
    GtkWidget* nextSegBtn = gtk_button_new_with_label("next segment");
    GtkWidget* calBtn = gtk_button_new_with_label("calibration");
    GtkWidget* datetimeBtn = gtk_button_new_with_label("date/time");
    
    gtk_style_context_add_class(gtk_widget_get_style_context(stageGoBtn), "nav-button");
    gtk_style_context_add_class(gtk_widget_get_style_context(segmentsBtn), "nav-button");
    gtk_style_context_add_class(gtk_widget_get_style_context(nextSegBtn), "nav-button");
    gtk_style_context_add_class(gtk_widget_get_style_context(calBtn), "nav-button");
    gtk_style_context_add_class(gtk_widget_get_style_context(datetimeBtn), "nav-button");
    
    g_signal_connect(stageGoBtn, "clicked", G_CALLBACK(on_stage_go), data);
    g_signal_connect(segmentsBtn, "clicked", G_CALLBACK(on_show_segments), data);
    g_signal_connect(nextSegBtn, "clicked", G_CALLBACK(on_next_segment), data);
    g_signal_connect(calBtn, "clicked", G_CALLBACK(on_show_calibration), data);
    g_signal_connect(datetimeBtn, "clicked", G_CALLBACK(on_show_datetime), data);
    
    gtk_box_pack_start(GTK_BOX(buttonBox), stageGoBtn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(buttonBox), segmentsBtn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(buttonBox), nextSegBtn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(buttonBox), calBtn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(buttonBox), datetimeBtn, TRUE, TRUE, 0);
    
    return screen;
}

// Create Stage Setup screen - horizontal layout for 1280x400
GtkWidget* createStageSetupScreen(AppData* data) {
    GtkWidget* screen = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(screen), 10);
    
    // Title
    GtkWidget* titleLabel = gtk_label_new("STAGE SETUP");
    gtk_style_context_add_class(gtk_widget_get_style_context(titleLabel), "title-label");
    gtk_box_pack_start(GTK_BOX(screen), titleLabel, FALSE, FALSE, 0);
    
    // Main horizontal container: left side for segments, right side for keypad
    data->stageSetupMainBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(screen), data->stageSetupMainBox, TRUE, TRUE, 0);
    
    // Left side: segments list
    GtkWidget* leftBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_size_request(leftBox, 500, -1);
    gtk_box_pack_start(GTK_BOX(data->stageSetupMainBox), leftBox, FALSE, FALSE, 0);
    
    // Header row
    GtkWidget* headerBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(leftBox), headerBox, FALSE, FALSE, 0);
    
    GtkWidget* header1 = gtk_label_new("Speed (KPH)");
    GtkWidget* header2 = gtk_label_new("Distance (m)");
    GtkWidget* header3 = gtk_label_new("Auto");
    
    gtk_style_context_add_class(gtk_widget_get_style_context(header1), "segment-label");
    gtk_style_context_add_class(gtk_widget_get_style_context(header2), "segment-label");
    gtk_style_context_add_class(gtk_widget_get_style_context(header3), "segment-label");
    
    gtk_box_pack_start(GTK_BOX(headerBox), header1, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(headerBox), header2, FALSE, FALSE, 80);
    gtk_box_pack_start(GTK_BOX(headerBox), header3, FALSE, FALSE, 5);
    
    // Scrollable list for segments
    GtkWidget* scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), 
                                   GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolled), 150);
    gtk_box_pack_start(GTK_BOX(leftBox), scrolled, TRUE, TRUE, 0);
    
    data->segmentListBox = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_selection_mode(data->segmentListBox, GTK_SELECTION_NONE);
    gtk_container_add(GTK_CONTAINER(scrolled), GTK_WIDGET(data->segmentListBox));
    
    // Memory columns (Set and Recall) - vertical layout between segments and keypad
    GtkWidget* memBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(data->stageSetupMainBox), memBox, FALSE, FALSE, 10);
    
    // Header row for memory columns
    GtkWidget* memHeaderRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(memBox), memHeaderRow, FALSE, FALSE, 0);
    GtkWidget* setHeader = gtk_label_new("Set");
    GtkWidget* recallHeader = gtk_label_new("Recall");
    gtk_widget_set_size_request(setHeader, 66, -1);
    gtk_widget_set_size_request(recallHeader, 66, -1);
    gtk_box_pack_start(GTK_BOX(memHeaderRow), setHeader, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(memHeaderRow), recallHeader, FALSE, FALSE, 0);
    
    // Buttons [1]-[5] in two vertical columns
    for (int i = 1; i <= 5; i++) {
        GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_box_pack_start(GTK_BOX(memBox), row, FALSE, FALSE, 0);
        
        GtkWidget* setBtn = gtk_button_new_with_label(std::to_string(i).c_str());
        gtk_widget_set_size_request(setBtn, 66, 43);
        g_signal_connect(setBtn, "clicked", G_CALLBACK(on_memory_set), data);
        g_object_set_data(G_OBJECT(setBtn), "slot", GINT_TO_POINTER(i));
        gtk_box_pack_start(GTK_BOX(row), setBtn, FALSE, FALSE, 0);
        
        GtkWidget* recallBtn = gtk_button_new_with_label(std::to_string(i).c_str());
        gtk_widget_set_size_request(recallBtn, 66, 43);
        g_signal_connect(recallBtn, "clicked", G_CALLBACK(on_memory_recall), data);
        g_object_set_data(G_OBJECT(recallBtn), "slot", GINT_TO_POINTER(i));
        gtk_box_pack_start(GTK_BOX(row), recallBtn, FALSE, FALSE, 0);
    }
    
    GtkWidget* clearMemBtn = gtk_button_new_with_label("clear memory");
    g_signal_connect(clearMemBtn, "clicked", G_CALLBACK(on_memory_clear), data);
    gtk_box_pack_start(GTK_BOX(memBox), clearMemBtn, FALSE, FALSE, 5);
    
    // Add new segment row (30% larger fonts and buttons)
    GtkWidget* addBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(addBox), "new-segment-row");
    gtk_box_pack_start(GTK_BOX(leftBox), addBox, FALSE, FALSE, 5);
    
    GtkWidget* newLabel = gtk_label_new("New:");
    data->targetSpeedEntry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(data->targetSpeedEntry, "KPH");
    gtk_widget_set_size_request(GTK_WIDGET(data->targetSpeedEntry), 100, 40);
    g_signal_connect(data->targetSpeedEntry, "focus-in-event", G_CALLBACK(on_entry_focus), data);
    
    data->distanceEntry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(data->distanceEntry, "meters");
    gtk_widget_set_size_request(GTK_WIDGET(data->distanceEntry), 100, 40);
    g_signal_connect(data->distanceEntry, "focus-in-event", G_CALLBACK(on_entry_focus), data);
    
    data->autoNextCheck = GTK_CHECK_BUTTON(gtk_check_button_new_with_label("Auto"));
    
    GtkWidget* addBtn = gtk_button_new_with_label("add");
    gtk_widget_set_size_request(addBtn, 60, 40);
    g_signal_connect(addBtn, "clicked", G_CALLBACK(on_add_segment), data);
    
    GtkWidget* backBtn = gtk_button_new_with_label("back");
    gtk_widget_set_size_request(backBtn, 60, 40);
    g_signal_connect(backBtn, "clicked", G_CALLBACK(on_show_twinmaster), data);
    
    gtk_box_pack_start(GTK_BOX(addBox), newLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(addBox), GTK_WIDGET(data->targetSpeedEntry), FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(addBox), GTK_WIDGET(data->distanceEntry), FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(addBox), GTK_WIDGET(data->autoNextCheck), FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(addBox), addBtn, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(addBox), backBtn, FALSE, FALSE, 20);
    
    // Right side: numeric keypad (initially hidden, shown when entry focused)
    data->numericKeypad = createNumericKeypad(data);
    gtk_box_pack_end(GTK_BOX(data->stageSetupMainBox), data->numericKeypad, FALSE, FALSE, 10);
    
    data->activeEntry = NULL;
    
    return screen;
}

// Create Calibration screen - horizontal layout for 1280x400
GtkWidget* createCalibrationScreen(AppData* data) {
    GtkWidget* screen = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(screen), 15);
    
    // Title
    GtkWidget* titleLabel = gtk_label_new("CALIBRATION");
    gtk_style_context_add_class(gtk_widget_get_style_context(titleLabel), "title-label");
    gtk_box_pack_start(GTK_BOX(screen), titleLabel, FALSE, FALSE, 0);
    
    // Main horizontal container: left side for info, right side for keypad
    data->calibrationMainBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(screen), data->calibrationMainBox, TRUE, TRUE, 0);
    
    // Left side: info and input
    GtkWidget* leftBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_pack_start(GTK_BOX(data->calibrationMainBox), leftBox, TRUE, TRUE, 0);
    
    // Row 1: Distance with counter breakdown
    // Format: "Total distance: xxx,xxx m  (counts calculated: CNTR_A  1: CNTR_1  2: CNTR_2)"
    data->totalDistCalLabel = GTK_LABEL(gtk_label_new("Total distance: 0 m  (counts calculated: 0   1: 0   2: 0)"));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->totalDistCalLabel)), "info-label");
    gtk_widget_set_halign(GTK_WIDGET(data->totalDistCalLabel), GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(leftBox), GTK_WIDGET(data->totalDistCalLabel), FALSE, FALSE, 5);
    
    // Row 2: Input field
    GtkWidget* inputRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(leftBox), inputRow, FALSE, FALSE, 10);
    
    GtkWidget* inputLabel = gtk_label_new("Actual distance covered:");
    data->rallyDistEntry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(data->rallyDistEntry, "500-100000");
    gtk_widget_set_size_request(GTK_WIDGET(data->rallyDistEntry), 150, -1);
    g_signal_connect(data->rallyDistEntry, "focus-in-event", G_CALLBACK(on_entry_focus), data);
    GtkWidget* unitLabel = gtk_label_new("meters");
    
    GtkWidget* resetBtn = gtk_button_new_with_label("reset to 1m per pulse");
    g_signal_connect(resetBtn, "clicked", G_CALLBACK(on_reset_calibration_1m), data);

    gtk_box_pack_start(GTK_BOX(inputRow), inputLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inputRow), GTK_WIDGET(data->rallyDistEntry), FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(inputRow), unitLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inputRow), resetBtn, FALSE, FALSE, 20);
    
    // Row 3: Sensor mode selection
    GtkWidget* sensorRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(leftBox), sensorRow, FALSE, FALSE, 10);
    
    data->sensorModeLabel = GTK_LABEL(gtk_label_new(
        data->state->counters ? "Currently set to both sensors" : "Currently set to sensor 1"));
    gtk_widget_set_halign(GTK_WIDGET(data->sensorModeLabel), GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(sensorRow), GTK_WIDGET(data->sensorModeLabel), FALSE, FALSE, 0);
    
    GtkWidget* sensor1Btn = gtk_button_new_with_label("Set sensor 1");
    g_signal_connect(sensor1Btn, "clicked", G_CALLBACK(on_set_sensor_1), data);
    gtk_box_pack_start(GTK_BOX(sensorRow), sensor1Btn, FALSE, FALSE, 20);
    
    GtkWidget* sensorBothBtn = gtk_button_new_with_label("Set both sensors and avg.");
    g_signal_connect(sensorBothBtn, "clicked", G_CALLBACK(on_set_sensor_both), data);
    gtk_box_pack_start(GTK_BOX(sensorRow), sensorBothBtn, FALSE, FALSE, 5);
    
    // Row 4: Buttons (start, save, back)
    GtkWidget* buttonBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_box_pack_start(GTK_BOX(leftBox), buttonBox, FALSE, FALSE, 10);
    
    GtkWidget* startBtn = gtk_button_new_with_label("start");
    GtkWidget* saveBtn = gtk_button_new_with_label("save");
    GtkWidget* backBtn = gtk_button_new_with_label("back");
    
    g_signal_connect(startBtn, "clicked", G_CALLBACK(on_calibration_start), data);
    g_signal_connect(saveBtn, "clicked", G_CALLBACK(on_save_calibration), data);
    g_signal_connect(backBtn, "clicked", G_CALLBACK(on_show_twinmaster), data);
    
    gtk_box_pack_start(GTK_BOX(buttonBox), startBtn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(buttonBox), saveBtn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(buttonBox), backBtn, TRUE, TRUE, 0);
    
    // Right side: numeric keypad
    data->calibrationKeypad = createNumericKeypad(data);
    gtk_box_pack_end(GTK_BOX(data->calibrationMainBox), data->calibrationKeypad, FALSE, FALSE, 10);
    
    // Hide totalCountCalLabel - we're using totalDistCalLabel for everything
    data->totalCountCalLabel = GTK_LABEL(gtk_label_new(""));
    
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
    
    // Create rally clock label first (used by TwinMaster screen)
    data->copilotRallyClockLabel = GTK_LABEL(gtk_label_new("00:00:00"));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->copilotRallyClockLabel)), "clock-label");
    
    // Create stack for screens
    data->copilotStack = GTK_STACK(gtk_stack_new());
    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(data->copilotStack));
    
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
