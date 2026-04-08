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

static std::string formatDistance(long meters, int width = 7) {
    bool negative = meters < 0;
    std::string num = std::to_string(negative ? -meters : meters);
    std::string result;
    int count = 0;
    for (int i = static_cast<int>(num.size()) - 1; i >= 0; i--) {
        if (count > 0 && count % 3 == 0) result = ',' + result;
        result = num[i] + result;
        count++;
    }
    if (negative) result = '-' + result;
    while (static_cast<int>(result.size()) < width)
        result = ' ' + result;
    return result;
}

// Apply CSS styling
static void applyCopilotCSS() {
    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        "window, .background { background-color: #000000; }"
        "label { color: #FFFFFF; font-weight: bold; }"
        "button { background-color: #333333; color: #FFFFFF; font-weight: bold; border: 2px solid #FFFFFF; }"
        "entry { background-color: #222222; color: #FFFFFF; font-weight: bold; }"
        ".title-label { font-size: 22px; }"
        ".info-label { font-size: 20px; }"
        ".clock-label { font-size: 30px; font-family: monospace; }"
        ".dist-heading { font-size: 48px; font-weight: bold; font-family: monospace; }"
        ".dist-value { font-size: 88px; font-weight: bold; font-family: monospace; }"
        ".dist-unit { font-size: 48px; font-weight: bold; font-family: monospace; }"
        ".time-label { font-size: 36px; font-family: monospace; color: #CCCCCC; }"
        ".alarm-label { font-size: 20px; }"
        ".alarm-button { font-size: 22px; }"
        ".reset-button { font-size: 36px; }"
        ".alarm-countdown { font-size: 28px; color: #FFFFFF; font-family: monospace; }"
        ".nav-button { font-size: 20px; }"
        ".segment-label { font-size: 18px; }"
        ".segment-row entry, .segment-row button, .segment-row checkbutton { font-size: 18px; }"
        ".new-segment-row label, .new-segment-row entry, .new-segment-row button, .new-segment-row checkbutton { font-size: 18px; }"
        "scrollbar slider { min-width: 20px; min-height: 20px; }"
        "scrollbar.vertical slider { min-width: 20px; }"
        "scrollbar trough { min-width: 24px; }",
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
            alarmSs << formatDistance(remaining_m, 6) << " to alarm";
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
    ss << formatDistance(total_m, 7);
    gtk_label_set_text(data->totalDistLabel, ss.str().c_str());
    
    ss.str("");
    ss << " " << std::setw(3) << std::setfill(' ') << (total_secs / 60)
       << ":" << std::setw(2) << std::setfill('0') << (total_secs % 60);
    gtk_label_set_text(data->totalTimeLabel, ss.str().c_str());
    
    // Trip distance
    int64_t trip_count_diff = calculateDistanceCounts(*data->state,
        current_poll.cntr1, current_poll.cntr2,
        data->state->trip_start_cntr1, data->state->trip_start_cntr2);
    long trip_m = countsToCentimeters(trip_count_diff, data->state->calibration) / 100;
    int64_t trip_duration_ms = current_time_ms - data->state->trip_start_time_ms;
    int trip_secs = static_cast<int>(trip_duration_ms / 1000);
    
    ss.str("");
    ss << formatDistance(trip_m, 7);
    gtk_label_set_text(data->tripDistLabel, ss.str().c_str());
    
    ss.str("");
    ss << " " << std::setw(3) << std::setfill(' ') << (trip_secs / 60)
       << ":" << std::setw(2) << std::setfill('0') << (trip_secs % 60);
    gtk_label_set_text(data->tripTimeLabel, ss.str().c_str());
    
    // Next segment info: distance remaining in current segment + speed of next segment
    if (data->state->segment_current_number >= 0 &&
        data->state->segment_current_number < static_cast<long>(data->state->segments.size())) {
        const Segment& cur_seg = data->state->segments[data->state->segment_current_number];
        int64_t seg_count_diff = calculateDistanceCounts(*data->state,
            current_poll.cntr1, current_poll.cntr2,
            data->state->segment_start_cntr1, data->state->segment_start_cntr2);
        int64_t remaining_counts = cur_seg.distance_counts - seg_count_diff;
        long remaining_m = countsToCentimeters(remaining_counts, data->state->calibration) / 100;
        long travelled_m = countsToCentimeters(seg_count_diff, data->state->calibration) / 100;
        
        ss.str("");
        ss << formatDistance(remaining_m, 7);
        gtk_label_set_text(data->nextDistLabel, ss.str().c_str());
        
        long next_seg_idx = data->state->segment_current_number + 1;
        if (next_seg_idx < static_cast<long>(data->state->segments.size())) {
            const Segment& next_seg = data->state->segments[next_seg_idx];
            ss.str("");
            ss << std::fixed << std::setprecision(0) << next_seg.target_speed_kph << " kph";
            gtk_label_set_text(data->nextSpeedLabel, ss.str().c_str());
        } else {
            gtk_label_set_text(data->nextSpeedLabel, "---");
        }
        
        // next/prev button: active within 500m of segment end or start
        bool near_end = (remaining_m >= 0 && remaining_m <= 500) &&
                        (next_seg_idx < static_cast<long>(data->state->segments.size()));
        bool near_start = (travelled_m >= 0 && travelled_m <= 500) &&
                          (data->state->segment_current_number > 0);
        if (near_end) {
            gtk_button_set_label(GTK_BUTTON(data->nextPrevBtn), "next");
            gtk_widget_set_sensitive(data->nextPrevBtn, TRUE);
        } else if (near_start) {
            gtk_button_set_label(GTK_BUTTON(data->nextPrevBtn), "prev");
            gtk_widget_set_sensitive(data->nextPrevBtn, TRUE);
        } else {
            gtk_button_set_label(GTK_BUTTON(data->nextPrevBtn), "--->");
            gtk_widget_set_sensitive(data->nextPrevBtn, FALSE);
        }
    } else {
        gtk_label_set_text(data->nextDistLabel, "---.---");
        gtk_label_set_text(data->nextSpeedLabel, "---");
        gtk_button_set_label(GTK_BUTTON(data->nextPrevBtn), "--->");
        gtk_widget_set_sensitive(data->nextPrevBtn, FALSE);
    }
}

// Create TwinMaster screen - two-column layout for 1280x400
GtkWidget* createTwinMasterScreen(AppData* data) {
    GtkWidget* screen = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(screen), 15);
    
    // Main area: two columns (left 70%, right 30%)
    GtkWidget* mainArea = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(screen), mainArea, TRUE, TRUE, 0);
    
    // ── LEFT PANEL (70%): segment info, total, trip ──
    GtkWidget* leftPanel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_size_request(leftPanel, 870, -1);
    gtk_box_pack_start(GTK_BOX(mainArea), leftPanel, TRUE, TRUE, 0);
    
    // Grid layout for Total/Trip/Next: columns = heading | value | unit | reset/arrow | time/speed
    GtkWidget* distGrid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(distGrid), 8);
    gtk_grid_set_row_spacing(GTK_GRID(distGrid), 2);
    gtk_box_pack_start(GTK_BOX(leftPanel), distGrid, FALSE, FALSE, 0);
    
    // Row 0: Total distance
    GtkWidget* totalHeading = gtk_label_new("Total");
    gtk_style_context_add_class(gtk_widget_get_style_context(totalHeading), "dist-heading");
    gtk_label_set_xalign(GTK_LABEL(totalHeading), 0.0);
    gtk_grid_attach(GTK_GRID(distGrid), totalHeading, 0, 0, 1, 1);
    
    data->totalDistLabel = GTK_LABEL(gtk_label_new("0"));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->totalDistLabel)), "dist-value");
    gtk_label_set_xalign(data->totalDistLabel, 1.0);
    gtk_label_set_width_chars(data->totalDistLabel, 7);
    gtk_grid_attach(GTK_GRID(distGrid), GTK_WIDGET(data->totalDistLabel), 1, 0, 1, 1);
    
    GtkWidget* totalUnit = gtk_label_new("m");
    gtk_style_context_add_class(gtk_widget_get_style_context(totalUnit), "dist-unit");
    gtk_widget_set_valign(totalUnit, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(distGrid), totalUnit, 2, 0, 1, 1);
    
    GtkWidget* totalResetBtn = gtk_button_new_with_label("reset");
    gtk_style_context_add_class(gtk_widget_get_style_context(totalResetBtn), "reset-button");
    gtk_widget_set_valign(totalResetBtn, GTK_ALIGN_CENTER);
    g_signal_connect(totalResetBtn, "clicked", G_CALLBACK(on_total_reset), data);
    gtk_grid_attach(GTK_GRID(distGrid), totalResetBtn, 3, 0, 1, 1);
    
    // Col 4: Total time (same row)
    data->totalTimeLabel = GTK_LABEL(gtk_label_new("   0:00"));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->totalTimeLabel)), "time-label");
    gtk_label_set_xalign(data->totalTimeLabel, 0.0);
    gtk_widget_set_valign(GTK_WIDGET(data->totalTimeLabel), GTK_ALIGN_CENTER);
    gtk_grid_attach(GTK_GRID(distGrid), GTK_WIDGET(data->totalTimeLabel), 4, 0, 1, 1);
    
    // Row 1: Trip distance
    GtkWidget* tripHeading = gtk_label_new("Trip");
    gtk_style_context_add_class(gtk_widget_get_style_context(tripHeading), "dist-heading");
    gtk_label_set_xalign(GTK_LABEL(tripHeading), 0.0);
    gtk_grid_attach(GTK_GRID(distGrid), tripHeading, 0, 1, 1, 1);
    
    data->tripDistLabel = GTK_LABEL(gtk_label_new("0"));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->tripDistLabel)), "dist-value");
    gtk_label_set_xalign(data->tripDistLabel, 1.0);
    gtk_label_set_width_chars(data->tripDistLabel, 7);
    gtk_grid_attach(GTK_GRID(distGrid), GTK_WIDGET(data->tripDistLabel), 1, 1, 1, 1);
    
    GtkWidget* tripUnit = gtk_label_new("m");
    gtk_style_context_add_class(gtk_widget_get_style_context(tripUnit), "dist-unit");
    gtk_widget_set_valign(tripUnit, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(distGrid), tripUnit, 2, 1, 1, 1);
    
    GtkWidget* tripResetBtn = gtk_button_new_with_label("reset");
    gtk_style_context_add_class(gtk_widget_get_style_context(tripResetBtn), "reset-button");
    gtk_widget_set_valign(tripResetBtn, GTK_ALIGN_CENTER);
    g_signal_connect(tripResetBtn, "clicked", G_CALLBACK(on_trip_reset), data);
    gtk_grid_attach(GTK_GRID(distGrid), tripResetBtn, 3, 1, 1, 1);
    
    // Col 4: Trip time (same row)
    data->tripTimeLabel = GTK_LABEL(gtk_label_new("   0:00"));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->tripTimeLabel)), "time-label");
    gtk_label_set_xalign(data->tripTimeLabel, 0.0);
    gtk_widget_set_valign(GTK_WIDGET(data->tripTimeLabel), GTK_ALIGN_CENTER);
    gtk_grid_attach(GTK_GRID(distGrid), GTK_WIDGET(data->tripTimeLabel), 4, 1, 1, 1);
    
    // Row 2: Next segment distance and speed
    GtkWidget* nextHeading = gtk_label_new("Next");
    gtk_style_context_add_class(gtk_widget_get_style_context(nextHeading), "dist-heading");
    gtk_label_set_xalign(GTK_LABEL(nextHeading), 0.0);
    gtk_grid_attach(GTK_GRID(distGrid), nextHeading, 0, 2, 1, 1);
    
    data->nextDistLabel = GTK_LABEL(gtk_label_new("---.---"));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->nextDistLabel)), "dist-value");
    gtk_label_set_xalign(data->nextDistLabel, 1.0);
    gtk_label_set_width_chars(data->nextDistLabel, 7);
    gtk_grid_attach(GTK_GRID(distGrid), GTK_WIDGET(data->nextDistLabel), 1, 2, 1, 1);
    
    GtkWidget* nextUnit = gtk_label_new("m");
    gtk_style_context_add_class(gtk_widget_get_style_context(nextUnit), "dist-unit");
    gtk_widget_set_valign(nextUnit, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(distGrid), nextUnit, 2, 2, 1, 1);
    
    data->nextPrevBtn = gtk_button_new_with_label("--->");
    gtk_style_context_add_class(gtk_widget_get_style_context(data->nextPrevBtn), "reset-button");
    gtk_widget_set_valign(data->nextPrevBtn, GTK_ALIGN_CENTER);
    gtk_widget_set_sensitive(data->nextPrevBtn, FALSE);
    g_signal_connect(data->nextPrevBtn, "clicked", G_CALLBACK(on_next_prev_segment), data);
    gtk_grid_attach(GTK_GRID(distGrid), data->nextPrevBtn, 3, 2, 1, 1);
    
    data->nextSpeedLabel = GTK_LABEL(gtk_label_new("---"));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->nextSpeedLabel)), "time-label");
    gtk_label_set_xalign(data->nextSpeedLabel, 0.0);
    gtk_widget_set_valign(GTK_WIDGET(data->nextSpeedLabel), GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start(GTK_WIDGET(data->nextSpeedLabel), 10);
    gtk_grid_attach(GTK_GRID(distGrid), GTK_WIDGET(data->nextSpeedLabel), 4, 2, 1, 1);
    
    // ── RIGHT PANEL (30%): clock, alarm buttons, countdown ──
    GtkWidget* rightPanel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_size_request(rightPanel, 360, -1);
    gtk_box_pack_start(GTK_BOX(mainArea), rightPanel, FALSE, FALSE, 0);
    
    // Top row: rally clock
    GtkWidget* topRightRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(rightPanel), topRightRow, FALSE, FALSE, 0);

    gtk_label_set_width_chars(data->copilotRallyClockLabel, 8);
    gtk_label_set_xalign(data->copilotRallyClockLabel, 1.0);
    gtk_box_pack_end(GTK_BOX(topRightRow), GTK_WIDGET(data->copilotRallyClockLabel), FALSE, FALSE, 0);
    
    // Alarm buttons: 3 rows of 4
    GtkWidget* alarmBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_top(alarmBox, 10);
    gtk_box_pack_start(GTK_BOX(rightPanel), alarmBox, FALSE, FALSE, 0);
    
    // Alarm buttons: 4 rows of 3, 30% larger (62x47)
    auto addAlarmRow = [&](GtkWidget* parent, int from, int to, bool hasLabel) {
        GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
        gtk_box_pack_start(GTK_BOX(parent), row, FALSE, FALSE, 0);
        if (hasLabel) {
            GtkWidget* lbl = gtk_label_new("Alarm in");
            gtk_style_context_add_class(gtk_widget_get_style_context(lbl), "alarm-label");
            gtk_box_pack_start(GTK_BOX(row), lbl, FALSE, FALSE, 3);
        } else {
            GtkWidget* spacer = gtk_label_new("");
            gtk_style_context_add_class(gtk_widget_get_style_context(spacer), "alarm-label");
            gtk_widget_set_size_request(spacer, 70, -1);
            gtk_box_pack_start(GTK_BOX(row), spacer, FALSE, FALSE, 3);
        }
        for (int km = from; km <= to; km++) {
            GtkWidget* btn = gtk_button_new_with_label(std::to_string(km).c_str());
            gtk_style_context_add_class(gtk_widget_get_style_context(btn), "alarm-button");
            gtk_widget_set_size_request(btn, 62, 47);
            g_object_set_data(G_OBJECT(btn), "km", GINT_TO_POINTER(km));
            g_signal_connect(btn, "clicked", G_CALLBACK(on_alarm_set), data);
            gtk_box_pack_start(GTK_BOX(row), btn, FALSE, FALSE, 2);
        }
    };
    
    addAlarmRow(alarmBox, 2, 4, true);
    addAlarmRow(alarmBox, 5, 7, false);
    addAlarmRow(alarmBox, 8, 10, false);
    addAlarmRow(alarmBox, 11, 13, false);
    
    // Alarm countdown + clear button
    GtkWidget* countdownRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_margin_top(countdownRow, 8);
    gtk_box_pack_start(GTK_BOX(rightPanel), countdownRow, FALSE, FALSE, 0);
    
    data->alarmCountdownLabel = GTK_LABEL(gtk_label_new(""));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->alarmCountdownLabel)), "alarm-countdown");
    gtk_box_pack_start(GTK_BOX(countdownRow), GTK_WIDGET(data->alarmCountdownLabel), FALSE, FALSE, 0);
    
    data->alarmClearBtn = gtk_button_new_with_label("clear");
    gtk_style_context_add_class(gtk_widget_get_style_context(data->alarmClearBtn), "alarm-button");
    g_signal_connect(data->alarmClearBtn, "clicked", G_CALLBACK(on_alarm_clear), data);
    gtk_box_pack_start(GTK_BOX(countdownRow), data->alarmClearBtn, FALSE, FALSE, 5);
    gtk_widget_set_no_show_all(data->alarmClearBtn, TRUE);
    
    // ── BOTTOM ROW: Navigation buttons (20% taller) ──
    GtkWidget* buttonBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    gtk_box_pack_end(GTK_BOX(screen), buttonBox, FALSE, FALSE, 0);
    
    GtkWidget* stageGoBtn = gtk_button_new_with_label("stage go");
    GtkWidget* segmentsBtn = gtk_button_new_with_label("segments");
    GtkWidget* calBtn = gtk_button_new_with_label("calibration");
    GtkWidget* datetimeBtn = gtk_button_new_with_label("date/time");
    
    GtkWidget* navBtns[] = {stageGoBtn, segmentsBtn, calBtn, datetimeBtn};
    for (auto* btn : navBtns) {
        gtk_style_context_add_class(gtk_widget_get_style_context(btn), "nav-button");
        gtk_widget_set_size_request(btn, -1, 43);
        gtk_box_pack_start(GTK_BOX(buttonBox), btn, TRUE, TRUE, 0);
    }
    
    g_signal_connect(stageGoBtn, "clicked", G_CALLBACK(on_stage_go), data);
    g_signal_connect(segmentsBtn, "clicked", G_CALLBACK(on_show_segments), data);
    g_signal_connect(calBtn, "clicked", G_CALLBACK(on_show_calibration), data);
    g_signal_connect(datetimeBtn, "clicked", G_CALLBACK(on_show_datetime), data);
    
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
    GtkWidget* header4 = gtk_label_new("Time");
    
    gtk_style_context_add_class(gtk_widget_get_style_context(header1), "segment-label");
    gtk_style_context_add_class(gtk_widget_get_style_context(header2), "segment-label");
    gtk_style_context_add_class(gtk_widget_get_style_context(header3), "segment-label");
    gtk_style_context_add_class(gtk_widget_get_style_context(header4), "segment-label");
    
    gtk_box_pack_start(GTK_BOX(headerBox), header1, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(headerBox), header2, FALSE, FALSE, 80);
    gtk_box_pack_start(GTK_BOX(headerBox), header3, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(headerBox), header4, FALSE, FALSE, 5);
    
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
    gtk_entry_set_placeholder_text(data->distanceEntry, "meters (;  separated)");
    gtk_widget_set_size_request(GTK_WIDGET(data->distanceEntry), 300, 40);
    g_signal_connect(data->distanceEntry, "focus-in-event", G_CALLBACK(on_entry_focus), data);
    
    data->autoNextCheck = GTK_CHECK_BUTTON(gtk_check_button_new_with_label("Auto"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->autoNextCheck), TRUE);
    
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
    GtkWidget* screen = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_style_context_add_class(gtk_widget_get_style_context(screen), "calibration-screen");
    gtk_container_set_border_width(GTK_CONTAINER(screen), 5);
    
    // Title
    GtkWidget* titleLabel = gtk_label_new("CALIBRATION");
    gtk_style_context_add_class(gtk_widget_get_style_context(titleLabel), "title-label");
    gtk_box_pack_start(GTK_BOX(screen), titleLabel, FALSE, FALSE, 0);
    
    // Main horizontal container: left side for info, right side for keypad
    data->calibrationMainBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(screen), data->calibrationMainBox, TRUE, TRUE, 0);
    
    // Left side: info and input
    GtkWidget* leftBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(data->calibrationMainBox), leftBox, TRUE, TRUE, 0);
    
    // Row 1: Distance with counter breakdown
    // Format: "Total distance: xxx,xxx m  (counts calculated: CNTR_A  1: CNTR_1  2: CNTR_2)"
    data->totalDistCalLabel = GTK_LABEL(gtk_label_new("Total distance: 0 m  (counts calculated: 0   1: 0   2: 0)"));
    gtk_widget_set_halign(GTK_WIDGET(data->totalDistCalLabel), GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(leftBox), GTK_WIDGET(data->totalDistCalLabel), FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(leftBox), gtk_label_new(""), FALSE, FALSE, 0);
    
    // Row 2: Input field
    GtkWidget* inputRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(leftBox), inputRow, FALSE, FALSE, 0);
    
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
    
    gtk_box_pack_start(GTK_BOX(leftBox), gtk_label_new(""), FALSE, FALSE, 0);
    
    // Row 3: Sensor mode selection
    GtkWidget* sensorRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(leftBox), sensorRow, FALSE, FALSE, 0);
    
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
    
    // Right side: numeric keypad
    data->calibrationKeypad = createNumericKeypad(data);
    gtk_box_pack_end(GTK_BOX(data->calibrationMainBox), data->calibrationKeypad, FALSE, FALSE, 10);
    
    // Bottom: navigation buttons
    GtkWidget* buttonBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_box_pack_end(GTK_BOX(screen), buttonBox, FALSE, FALSE, 5);
    
    GtkWidget* startBtn = gtk_button_new_with_label("start");
    GtkWidget* saveBtn = gtk_button_new_with_label("save");
    GtkWidget* backBtn = gtk_button_new_with_label("back");
    
    g_signal_connect(startBtn, "clicked", G_CALLBACK(on_calibration_start), data);
    g_signal_connect(saveBtn, "clicked", G_CALLBACK(on_save_calibration), data);
    g_signal_connect(backBtn, "clicked", G_CALLBACK(on_show_twinmaster), data);
    
    gtk_box_pack_start(GTK_BOX(buttonBox), startBtn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(buttonBox), saveBtn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(buttonBox), backBtn, TRUE, TRUE, 0);
    
    // Hide totalCountCalLabel - we're using totalDistCalLabel for everything
    data->totalCountCalLabel = GTK_LABEL(gtk_label_new(""));
    
    return screen;
}

// Create Date/Time Setup screen - horizontal layout for 1280x400
GtkWidget* createDateTimeScreen(AppData* data) {
    GtkWidget* screen = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_style_context_add_class(gtk_widget_get_style_context(screen), "datetime-screen");
    gtk_container_set_border_width(GTK_CONTAINER(screen), 10);
    
    // Title row with exit button
    GtkWidget* titleRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(screen), titleRow, FALSE, FALSE, 0);
    
    GtkWidget* titleLabel = gtk_label_new("DATE/TIME SETUP");
    gtk_style_context_add_class(gtk_widget_get_style_context(titleLabel), "title-label");
    gtk_box_pack_start(GTK_BOX(titleRow), titleLabel, TRUE, TRUE, 0);
    
    GtkWidget* exitBtn = gtk_button_new_with_label("exit app");
    g_signal_connect(exitBtn, "clicked", G_CALLBACK(on_exit_app), data);
    gtk_box_pack_end(GTK_BOX(titleRow), exitBtn, FALSE, FALSE, 0);
    
    // Main horizontal container: left side for content, right side for keypad
    GtkWidget* mainBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(screen), mainBox, TRUE, TRUE, 0);
    
    // Left side: clocks, input, buttons
    GtkWidget* leftBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(mainBox), leftBox, TRUE, TRUE, 0);
    
    // System clock row (30px)
    GtkWidget* sysBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget* sysLabel = gtk_label_new("System Clock:");
    gtk_style_context_add_class(gtk_widget_get_style_context(sysLabel), "clock-label");
    data->systemClockLabel = GTK_LABEL(gtk_label_new(""));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->systemClockLabel)), "clock-label");
    gtk_box_pack_start(GTK_BOX(sysBox), sysLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sysBox), GTK_WIDGET(data->systemClockLabel), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(leftBox), sysBox, FALSE, FALSE, 0);
    
    // Rally clock row (30px)
    GtkWidget* rallyBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget* rallyLabel = gtk_label_new("Rally  Clock:");
    gtk_style_context_add_class(gtk_widget_get_style_context(rallyLabel), "clock-label");
    data->rallyClockLabel = GTK_LABEL(gtk_label_new(""));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->rallyClockLabel)), "clock-label");
    gtk_box_pack_start(GTK_BOX(rallyBox), rallyLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(rallyBox), GTK_WIDGET(data->rallyClockLabel), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(leftBox), rallyBox, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(leftBox), gtk_label_new(""), FALSE, FALSE, 0);
    
    // Input row
    GtkWidget* inputRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_box_pack_start(GTK_BOX(leftBox), inputRow, FALSE, FALSE, 0);
    
    GtkWidget* setLabel = gtk_label_new("Set Rally Time:");
    gtk_style_context_add_class(gtk_widget_get_style_context(setLabel), "clock-label");
    GtkWidget* dateLabel = gtk_label_new("Date:");
    gtk_style_context_add_class(gtk_widget_get_style_context(dateLabel), "clock-label");
    data->dateEntry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(data->dateEntry, "yyyy/mm/dd");
    gtk_widget_set_size_request(GTK_WIDGET(data->dateEntry), 200, -1);
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->dateEntry)), "clock-label");
    g_signal_connect(data->dateEntry, "focus-in-event", G_CALLBACK(on_entry_focus), data);
    
    GtkWidget* timeLabel = gtk_label_new("Time:");
    gtk_style_context_add_class(gtk_widget_get_style_context(timeLabel), "clock-label");
    data->timeEntry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(data->timeEntry, "hh:mm:ss");
    gtk_widget_set_size_request(GTK_WIDGET(data->timeEntry), 160, -1);
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->timeEntry)), "clock-label");
    g_signal_connect(data->timeEntry, "focus-in-event", G_CALLBACK(on_entry_focus), data);
    
    gtk_box_pack_start(GTK_BOX(inputRow), setLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inputRow), dateLabel, FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(inputRow), GTK_WIDGET(data->dateEntry), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inputRow), timeLabel, FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(inputRow), GTK_WIDGET(data->timeEntry), FALSE, FALSE, 0);
    
    // Right side: datetime keypad
    data->datetimeKeypad = createDateTimeKeypad(data);
    gtk_box_pack_end(GTK_BOX(mainBox), data->datetimeKeypad, FALSE, FALSE, 10);
    
    // Bottom: navigation buttons
    GtkWidget* buttonBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_box_pack_end(GTK_BOX(screen), buttonBox, FALSE, FALSE, 5);
    
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
