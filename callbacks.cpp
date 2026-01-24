#include "callbacks.h"
#include "rally_types.h"
#include "rally_state.h"
#include "config_file.h"
#include "calculations.h"
#include "ui_driver.h"
#include "ui_copilot.h"
#include "counter_poller.h"
#include <cmath>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cstring>
#include <chrono>

gboolean on_window_delete(G_GNUC_UNUSED GtkWidget* widget, G_GNUC_UNUSED GdkEvent* event, G_GNUC_UNUSED gpointer user_data) {
    gtk_main_quit();
    return FALSE;
}

void on_unit_toggle(G_GNUC_UNUSED GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    data->state->units = !data->state->units;
    ConfigFile::save(*data->state);
    if (data->state->units) {
        gtk_button_set_label(data->unitToggleBtn, "MPH");
        gtk_label_set_text(data->unitsLabel, "MPH");
    } else {
        gtk_button_set_label(data->unitToggleBtn, "KPH");
        gtk_label_set_text(data->unitsLabel, "KPH");
    }
}

void on_total_reset(G_GNUC_UNUSED GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    auto current_poll = data->poller->getMostRecent();
    data->state->total_start_cntr1 = current_poll.cntr1;
    data->state->total_start_cntr2 = current_poll.cntr2;
    data->state->total_start_time_ms = getRallyTime_ms(*data->state);
    ConfigFile::save(*data->state);
}

void on_trip_reset(G_GNUC_UNUSED GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    auto current_poll = data->poller->getMostRecent();
    data->state->trip_start_cntr1 = current_poll.cntr1;
    data->state->trip_start_cntr2 = current_poll.cntr2;
    data->state->trip_start_time_ms = getRallyTime_ms(*data->state);
    ConfigFile::save(*data->state);
}

void on_stage_go(G_GNUC_UNUSED GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    auto current_poll = data->poller->getMostRecent();
    int64_t current_time = getRallyTime_ms(*data->state);
    
    // Reset Total
    data->state->total_start_cntr1 = current_poll.cntr1;
    data->state->total_start_cntr2 = current_poll.cntr2;
    data->state->total_start_time_ms = current_time;
    
    // Reset Trip
    data->state->trip_start_cntr1 = current_poll.cntr1;
    data->state->trip_start_cntr2 = current_poll.cntr2;
    data->state->trip_start_time_ms = current_time;
    
    // Reset Segment
    data->state->segment_start_cntr1 = current_poll.cntr1;
    data->state->segment_start_cntr2 = current_poll.cntr2;
    data->state->segment_start_time_ms = current_time;
    
    // Reset to first segment if segments exist
    if (!data->state->segments.empty()) {
        data->state->segment_current_number = 0;
    }
    
    ConfigFile::save(*data->state);
}

void on_next_segment(G_GNUC_UNUSED GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    if (data->state->segment_current_number < static_cast<long>(data->state->segments.size()) - 1) {
        data->state->segment_current_number++;
        auto current_poll = data->poller->getMostRecent();
        data->state->segment_start_cntr1 = current_poll.cntr1;
        data->state->segment_start_cntr2 = current_poll.cntr2;
        data->state->segment_start_time_ms = getRallyTime_ms(*data->state);
        // Reset trip
        data->state->trip_start_cntr1 = current_poll.cntr1;
        data->state->trip_start_cntr2 = current_poll.cntr2;
        data->state->trip_start_time_ms = data->state->segment_start_time_ms;
        ConfigFile::save(*data->state);
    }
}

gboolean update_display(gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    
    // Poll counters (respects 5ms minimum interval)
    data->poller->poll(data->counter1, data->counter2, data->register_addr);
    
    // Check for auto-advance segments
    if (data->state->segment_current_number >= 0 && 
        data->state->segment_current_number < static_cast<long>(data->state->segments.size())) {
        Segment& seg = data->state->segments[data->state->segment_current_number];
        if (seg.autoNext) {
            auto current_poll = data->poller->getMostRecent();
            int64_t seg_count_diff = calculateDistanceCounts(*data->state,
                current_poll.cntr1, current_poll.cntr2,
                data->state->segment_start_cntr1, data->state->segment_start_cntr2);
            
            if (seg_count_diff >= seg.distance_counts) {
                // Advance to next segment
                if (data->state->segment_current_number < static_cast<long>(data->state->segments.size()) - 1) {
                    data->state->segment_current_number++;
                    auto current_poll = data->poller->getMostRecent();
                    data->state->segment_start_cntr1 = current_poll.cntr1;
                    data->state->segment_start_cntr2 = current_poll.cntr2;
                    data->state->segment_start_time_ms = getRallyTime_ms(*data->state);
                    // Reset trip
                    data->state->trip_start_cntr1 = current_poll.cntr1;
                    data->state->trip_start_cntr2 = current_poll.cntr2;
                    data->state->trip_start_time_ms = data->state->segment_start_time_ms;
                    ConfigFile::save(*data->state);
                }
            }
        }
    }
    
    updateDriverDisplay(data);
    updateCopilotDisplay(data);
    
    return G_SOURCE_CONTINUE;
}

// Screen navigation callbacks
void on_show_segments(G_GNUC_UNUSED GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    gtk_stack_set_visible_child_name(data->copilotStack, "stagesetup");
    // Refresh segment list
    refreshSegmentList(data);
}

void on_show_calibration(G_GNUC_UNUSED GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    gtk_stack_set_visible_child_name(data->copilotStack, "calibration");
    // Update calibration display
    updateCalibrationDisplay(data);
}

void on_show_twinmaster(G_GNUC_UNUSED GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    gtk_stack_set_visible_child_name(data->copilotStack, "twinmaster");
}

void on_show_datetime(G_GNUC_UNUSED GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    gtk_stack_set_visible_child_name(data->copilotStack, "datetime");
    updateDateTimeDisplay(data);
}

// Create numeric keypad widget
GtkWidget* createNumericKeypad(AppData* data) {
    GtkWidget* keypad = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(keypad), 5);
    gtk_grid_set_column_spacing(GTK_GRID(keypad), 5);
    
    const char* digits[] = {"7", "8", "9", "4", "5", "6", "1", "2", "3", ".", "0", "C"};
    
    for (int i = 0; i < 12; i++) {
        GtkWidget* btn = gtk_button_new_with_label(digits[i]);
        gtk_widget_set_size_request(btn, 50, 40);
        
        if (strcmp(digits[i], "C") == 0) {
            g_signal_connect(btn, "clicked", G_CALLBACK(on_keypad_clear), data);
        } else {
            g_signal_connect(btn, "clicked", G_CALLBACK(on_keypad_digit), data);
        }
        
        gtk_grid_attach(GTK_GRID(keypad), btn, i % 3, i / 3, 1, 1);
    }
    
    // Backspace button
    GtkWidget* bkspBtn = gtk_button_new_with_label("<-");
    gtk_widget_set_size_request(bkspBtn, 160, 40);
    g_signal_connect(bkspBtn, "clicked", G_CALLBACK(on_keypad_backspace), data);
    gtk_grid_attach(GTK_GRID(keypad), bkspBtn, 0, 4, 3, 1);
    
    return keypad;
}

// Keypad callbacks
void on_keypad_digit(GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    if (!data->activeEntry) return;
    
    const char* digit = gtk_button_get_label(GTK_BUTTON(widget));
    const char* current = gtk_entry_get_text(data->activeEntry);
    
    std::string new_text = std::string(current) + digit;
    gtk_entry_set_text(data->activeEntry, new_text.c_str());
}

void on_keypad_clear(G_GNUC_UNUSED GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    if (!data->activeEntry) return;
    gtk_entry_set_text(data->activeEntry, "");
}

void on_keypad_backspace(G_GNUC_UNUSED GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    if (!data->activeEntry) return;
    
    const char* current = gtk_entry_get_text(data->activeEntry);
    std::string text(current);
    if (!text.empty()) {
        text.pop_back();
        gtk_entry_set_text(data->activeEntry, text.c_str());
    }
}

gboolean on_entry_focus(GtkWidget* widget, G_GNUC_UNUSED GdkEvent* event, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    data->activeEntry = GTK_ENTRY(widget);
    gtk_widget_show(data->numericKeypad);
    return FALSE;
}

// Callback for when segment entry value changes
void on_segment_entry_changed(GtkWidget* widget, gpointer user_data) {
    int index = GPOINTER_TO_INT(user_data);
    AppData* data = static_cast<AppData*>(g_object_get_data(G_OBJECT(widget), "app_data"));
    if (!data || index < 0 || index >= static_cast<int>(data->state->segments.size())) return;
    
    const char* entry_type = static_cast<const char*>(g_object_get_data(G_OBJECT(widget), "entry_type"));
    const char* text = gtk_entry_get_text(GTK_ENTRY(widget));
    
    if (text && strlen(text) > 0) {
        if (strcmp(entry_type, "speed") == 0) {
            double kph = std::stod(text);
            data->state->segments[index].target_speed_counts_per_hour = kphToCountsPerHour(kph, data->state->calibration);
        } else if (strcmp(entry_type, "distance") == 0) {
            long meters = std::stol(text);
            // Convert meters to counts: counts = meters * 1000000 / calibration
            data->state->segments[index].distance_counts = (meters * 1000000L) / data->state->calibration;
        }
        ConfigFile::save(*data->state);
    }
}

// Callback for when segment auto checkbox toggled
void on_segment_auto_toggled(GtkWidget* widget, gpointer user_data) {
    int index = GPOINTER_TO_INT(user_data);
    AppData* data = static_cast<AppData*>(g_object_get_data(G_OBJECT(widget), "app_data"));
    if (!data || index < 0 || index >= static_cast<int>(data->state->segments.size())) return;
    
    data->state->segments[index].autoNext = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    ConfigFile::save(*data->state);
}

// Helper function to refresh segment list with editable entries
void refreshSegmentList(AppData* data) {
    // Clear existing rows
    GList* children = gtk_container_get_children(GTK_CONTAINER(data->segmentListBox));
    for (GList* l = children; l != NULL; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);
    
    // Add segments with editable entries
    for (size_t i = 0; i < data->state->segments.size(); i++) {
        const Segment& seg = data->state->segments[i];
        double target_kph = countsPerHourToKPH(seg.target_speed_counts_per_hour, data->state->calibration);
        long distance_m = countsToCentimeters(seg.distance_counts, data->state->calibration) / 100;
        
        GtkWidget* row = gtk_list_box_row_new();
        GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        gtk_container_add(GTK_CONTAINER(row), box);
        
        // Speed entry (editable)
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << target_kph;
        GtkWidget* speedEntry = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(speedEntry), ss.str().c_str());
        gtk_widget_set_size_request(speedEntry, 100, -1);
        g_object_set_data(G_OBJECT(speedEntry), "app_data", data);
        g_object_set_data(G_OBJECT(speedEntry), "entry_type", (gpointer)"speed");
        g_signal_connect(speedEntry, "changed", G_CALLBACK(on_segment_entry_changed), GINT_TO_POINTER(i));
        g_signal_connect(speedEntry, "focus-in-event", G_CALLBACK(on_entry_focus), data);
        
        // Distance entry (editable)
        ss.str("");
        ss << distance_m;
        GtkWidget* distEntry = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(distEntry), ss.str().c_str());
        gtk_widget_set_size_request(distEntry, 100, -1);
        g_object_set_data(G_OBJECT(distEntry), "app_data", data);
        g_object_set_data(G_OBJECT(distEntry), "entry_type", (gpointer)"distance");
        g_signal_connect(distEntry, "changed", G_CALLBACK(on_segment_entry_changed), GINT_TO_POINTER(i));
        g_signal_connect(distEntry, "focus-in-event", G_CALLBACK(on_entry_focus), data);
        
        // Auto checkbox (editable)
        GtkWidget* autoCheck = gtk_check_button_new();
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(autoCheck), seg.autoNext);
        g_object_set_data(G_OBJECT(autoCheck), "app_data", data);
        g_signal_connect(autoCheck, "toggled", G_CALLBACK(on_segment_auto_toggled), GINT_TO_POINTER(i));
        
        // Delete button
        GtkWidget* deleteBtn = gtk_button_new_with_label("del");
        g_object_set_data(G_OBJECT(deleteBtn), "app_data", data);
        g_signal_connect(deleteBtn, "clicked", G_CALLBACK(on_delete_segment), GINT_TO_POINTER(i));
        
        gtk_box_pack_start(GTK_BOX(box), speedEntry, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(box), distEntry, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(box), autoCheck, FALSE, FALSE, 15);
        gtk_box_pack_start(GTK_BOX(box), deleteBtn, FALSE, FALSE, 5);
        
        gtk_list_box_insert(data->segmentListBox, row, -1);
        gtk_widget_show_all(row);
    }
}

// Helper function to update calibration display
void updateCalibrationDisplay(AppData* data) {
    auto current_poll = data->poller->getMostRecent();
    int64_t total_count_diff = calculateDistanceCounts(*data->state,
        current_poll.cntr1, current_poll.cntr2,
        data->state->total_start_cntr1, data->state->total_start_cntr2);
    long total_m = countsToCentimeters(total_count_diff, data->state->calibration) / 100;
    
    std::stringstream ss;
    ss << "Total distance " << total_m << " m (" << total_count_diff << " count)";
    gtk_label_set_text(data->totalDistCalLabel, ss.str().c_str());
}

// Helper function to update date/time display
void updateDateTimeDisplay(AppData* data) {
    // System clock
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    time_t seconds = ms / 1000;
    struct tm* tm = localtime(&seconds);
    
    char buf[100];
    snprintf(buf, sizeof(buf), "%04d/%02d/%02d  %02d:%02d:%02d",
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec);
    gtk_label_set_text(data->systemClockLabel, buf);
    
    // Rally clock
    int64_t rally_ms = getRallyTime_ms(*data->state);
    time_t rally_seconds = rally_ms / 1000;
    struct tm* rally_tm = localtime(&rally_seconds);
    
    snprintf(buf, sizeof(buf), "%04d/%02d/%02d  %02d:%02d:%02d",
             rally_tm->tm_year + 1900, rally_tm->tm_mon + 1, rally_tm->tm_mday,
             rally_tm->tm_hour, rally_tm->tm_min, rally_tm->tm_sec);
    gtk_label_set_text(data->rallyClockLabel, buf);
}

void on_add_segment(G_GNUC_UNUSED GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    
    const char* speed_str = gtk_entry_get_text(data->targetSpeedEntry);
    const char* dist_str = gtk_entry_get_text(data->distanceEntry);
    bool autoNext = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->autoNextCheck));
    
    if (speed_str && dist_str && strlen(speed_str) > 0 && strlen(dist_str) > 0) {
        double target_kph = std::stod(speed_str);
        long distance_m = std::stol(dist_str);
        
        // Convert to counts
        long target_counts_per_hour = kphToCountsPerHour(target_kph, data->state->calibration);
        long distance_counts = (distance_m * 1000000) / data->state->calibration;  // meters to counts
        
        Segment seg;
        seg.target_speed_counts_per_hour = target_counts_per_hour;
        seg.distance_counts = distance_counts;
        seg.autoNext = autoNext;
        
        data->state->segments.push_back(seg);
        ConfigFile::save(*data->state);
        
        // Clear entries
        gtk_entry_set_text(data->targetSpeedEntry, "");
        gtk_entry_set_text(data->distanceEntry, "");
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->autoNextCheck), FALSE);
        
        // Refresh list
        refreshSegmentList(data);
    }
}

void on_delete_segment(GtkWidget* widget, gpointer user_data) {
    // user_data contains the segment index
    int index = GPOINTER_TO_INT(user_data);
    
    // We need to get AppData from the widget's parent hierarchy
    // For now, use a simpler approach - store AppData pointer in widget data
    AppData* data = static_cast<AppData*>(g_object_get_data(G_OBJECT(widget), "app_data"));
    if (data && index >= 0 && index < static_cast<int>(data->state->segments.size())) {
        data->state->segments.erase(data->state->segments.begin() + index);
        ConfigFile::save(*data->state);
        refreshSegmentList(data);
    }
}

void on_save_calibration(G_GNUC_UNUSED GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    
    const char* dist_str = gtk_entry_get_text(data->rallyDistEntry);
    if (dist_str && strlen(dist_str) > 0) {
        long rally_distance_m = std::stol(dist_str);
        
        if (rally_distance_m >= 500 && rally_distance_m <= 100000) {
            auto current_poll = data->poller->getMostRecent();
            int64_t total_count_diff = calculateDistanceCounts(*data->state,
                current_poll.cntr1, current_poll.cntr2,
                data->state->total_start_cntr1, data->state->total_start_cntr2);
            
            if (total_count_diff > 0) {
                // new_cal = (input_meters * 1000 * 1000) / total_count_diff
                data->state->calibration = (rally_distance_m * 1000000) / total_count_diff;
                ConfigFile::save(*data->state);
                
                // Clear entry
                gtk_entry_set_text(data->rallyDistEntry, "");
                updateCalibrationDisplay(data);
            }
        }
    }
}

void on_save_datetime(G_GNUC_UNUSED GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    
    const char* date_str = gtk_entry_get_text(data->dateEntry);
    const char* time_str = gtk_entry_get_text(data->timeEntry);
    
    if (date_str && time_str && strlen(date_str) > 0 && strlen(time_str) > 0) {
        // Parse date (yyyy/mm/dd)
        int year, month, day, hour, min, sec;
        if (sscanf(date_str, "%d/%d/%d", &year, &month, &day) == 3 &&
            sscanf(time_str, "%d:%d:%d", &hour, &min, &sec) == 3) {
            
            struct tm tm = {};
            tm.tm_year = year - 1900;
            tm.tm_mon = month - 1;
            tm.tm_mday = day;
            tm.tm_hour = hour;
            tm.tm_min = min;
            tm.tm_sec = sec;
            
            time_t rally_time = mktime(&tm);
            auto now = std::chrono::system_clock::now();
            auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count();
            
            // Calculate offset
            int64_t rally_ms = rally_time * 1000;
            data->state->rallyTimeOffset_ms = rally_ms - now_ms;
            ConfigFile::save(*data->state);
            
            // Clear entries
            gtk_entry_set_text(data->dateEntry, "");
            gtk_entry_set_text(data->timeEntry, "");
            updateDateTimeDisplay(data);
        }
    }
}
