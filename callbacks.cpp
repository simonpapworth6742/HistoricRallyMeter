#include "callbacks.h"
#include "rally_types.h"
#include "rally_state.h"
#include "config_file.h"
#include "calculations.h"
#include "ui_driver.h"
#include "ui_copilot.h"
#include "counter_poller.h"
#include "tone_generator.h"
#include <cmath>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cstring>
#include <chrono>
#include <functional>

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

static void applyDialogStyle(GtkWidget* dialog) {
    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        "* { font-size: 30px; }"
        "dialog { border: 3px solid white; }", -1, nullptr);
    
    std::function<void(GtkWidget*)> apply = [&](GtkWidget* w) {
        gtk_style_context_add_provider(
            gtk_widget_get_style_context(w),
            GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 100);
        if (GTK_IS_CONTAINER(w)) {
            gtk_container_forall(GTK_CONTAINER(w),
                [](GtkWidget* child, gpointer data) {
                    auto& fn = *static_cast<std::function<void(GtkWidget*)>*>(data);
                    fn(child);
                }, &apply);
        }
    };
    apply(dialog);
    g_object_unref(provider);
}

void performStageGo(AppData* data) {
    auto current_poll = data->poller->getMostRecent();
    int64_t current_time = getRallyTime_ms(*data->state);

    data->state->total_start_cntr1 = current_poll.cntr1;
    data->state->total_start_cntr2 = current_poll.cntr2;
    data->state->total_start_time_ms = current_time;

    data->state->trip_start_cntr1 = current_poll.cntr1;
    data->state->trip_start_cntr2 = current_poll.cntr2;
    data->state->trip_start_time_ms = current_time;

    data->state->segment_start_cntr1 = current_poll.cntr1;
    data->state->segment_start_cntr2 = current_poll.cntr2;
    data->state->segment_start_time_ms = current_time;

    if (!data->state->segments.empty()) {
        data->state->segment_current_number = 0;
    }

    data->gaugeScale = 0;
    data->gaugeScaleChangeTime = 0;
    data->aheadBehindSeconds = 0.0;
    data->smoothedSpeed = -1.0;
    data->state->ahead_behind_zero_offset_ms = 0;
    data->autoStartTriggered = false;
    
    if (data->toneGen) data->toneGen->setCadence(0, 0, 0.0);
    
    ConfigFile::save(*data->state);
}

static const int RESPONSE_AUTO_START = 99;

void on_stage_go(GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    
    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        "Confirm Stage Go",
        GTK_WINDOW(gtk_widget_get_toplevel(widget)),
        GTK_DIALOG_MODAL,
        "Yes", GTK_RESPONSE_YES,
        "Auto start", RESPONSE_AUTO_START,
        "No", GTK_RESPONSE_NO,
        nullptr);
    
    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 20);
    
    GtkWidget* label = gtk_label_new("Stage Go?\n\nThis will reset Total, Trip,\nand Segment counters.");
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(content), label, TRUE, TRUE, 10);
    gtk_widget_show(label);
    
    applyDialogStyle(dialog);
    
    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    
    if (response == GTK_RESPONSE_YES) {
        performStageGo(data);
    } else if (response == RESPONSE_AUTO_START) {
        on_show_autostart(widget, user_data);
    }
}

static const int RESPONSE_RESET_ZERO = 100;

void on_adj_driver_zero(GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    
    double current_ahead_behind = data->aheadBehindSeconds;
    double current_offset_s = data->state->ahead_behind_zero_offset_ms / 1000.0;
    
    char msg[256];
    snprintf(msg, sizeof(msg),
        "Adjust the ahead/behind value\nby %.2f seconds,\ncurrently %.2f seconds",
        current_ahead_behind, current_offset_s);
    
    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        "Adjust Driver Zero",
        GTK_WINDOW(gtk_widget_get_toplevel(widget)),
        GTK_DIALOG_MODAL,
        "Yes", GTK_RESPONSE_YES,
        "No", GTK_RESPONSE_NO,
        nullptr);
    
    GtkWidget* resetBtn = gtk_dialog_add_button(GTK_DIALOG(dialog), "Reset to 0.0", RESPONSE_RESET_ZERO);
    gtk_widget_set_margin_start(resetBtn, 40);
    
    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 20);
    
    GtkWidget* label = gtk_label_new(msg);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(content), label, TRUE, TRUE, 10);
    gtk_widget_show(label);
    
    applyDialogStyle(dialog);
    
    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    
    if (response == GTK_RESPONSE_YES) {
        long offset_ms = static_cast<long>(current_ahead_behind * -1000.0);
        data->state->ahead_behind_zero_offset_ms += offset_ms;
        ConfigFile::save(*data->state);
    } else if (response == RESPONSE_RESET_ZERO) {
        data->state->ahead_behind_zero_offset_ms = 0;
        ConfigFile::save(*data->state);
    }
}

void on_next_segment(G_GNUC_UNUSED GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    if (data->state->segment_current_number < static_cast<long>(data->state->segments.size()) - 1) {
        auto current_poll = data->poller->getMostRecent();
        data->state->segment_current_number++;
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

void on_next_prev_segment(G_GNUC_UNUSED GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    if (data->state->segment_current_number < 0 ||
        data->state->segment_current_number >= static_cast<long>(data->state->segments.size()))
        return;
    
    auto current_poll = data->poller->getMostRecent();
    int64_t seg_count_diff = calculateDistanceCounts(*data->state,
        current_poll.cntr1, current_poll.cntr2,
        data->state->segment_start_cntr1, data->state->segment_start_cntr2);
    
    Segment& cur_seg = data->state->segments[data->state->segment_current_number];
    int64_t remaining_counts = cur_seg.distance_counts - seg_count_diff;
    long remaining_m = countsToCentimeters(remaining_counts, data->state->calibration) / 100;
    long travelled_m = countsToCentimeters(seg_count_diff, data->state->calibration) / 100;
    
    long next_seg_idx = data->state->segment_current_number + 1;
    bool near_end = (remaining_m >= 0 && remaining_m <= 500) &&
                    (next_seg_idx < static_cast<long>(data->state->segments.size()));
    bool near_start = (travelled_m >= 0 && travelled_m <= 500) &&
                      (data->state->segment_current_number > 0);
    
    if (near_end) {
        // "next": reduce current segment distance to actual distance travelled, then advance
        cur_seg.distance_counts = seg_count_diff;
        cur_seg.distance_m = (cur_seg.distance_counts * data->state->calibration) / 1e6;
        
        data->state->segment_current_number++;
        data->state->segment_start_cntr1 = current_poll.cntr1;
        data->state->segment_start_cntr2 = current_poll.cntr2;
        data->state->segment_start_time_ms = getRallyTime_ms(*data->state);
        data->state->trip_start_cntr1 = current_poll.cntr1;
        data->state->trip_start_cntr2 = current_poll.cntr2;
        data->state->trip_start_time_ms = data->state->segment_start_time_ms;
    } else if (near_start) {
        // "prev": extend previous segment distance to end here, reset current segment start to now
        Segment& prev_seg = data->state->segments[data->state->segment_current_number - 1];
        prev_seg.distance_counts += seg_count_diff;
        prev_seg.distance_m = (prev_seg.distance_counts * data->state->calibration) / 1e6;
        
        data->state->segment_start_cntr1 = current_poll.cntr1;
        data->state->segment_start_cntr2 = current_poll.cntr2;
        data->state->segment_start_time_ms = getRallyTime_ms(*data->state);
        data->state->trip_start_cntr1 = current_poll.cntr1;
        data->state->trip_start_cntr2 = current_poll.cntr2;
        data->state->trip_start_time_ms = data->state->segment_start_time_ms;
    }
    
    ConfigFile::save(*data->state);
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
    
    // Reset calibration state when entering screen
    data->cal_started = false;
    data->activeEntry = data->rallyDistEntry;  // Set active entry for keypad
    
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

    int64_t rally_ms = getRallyTime_ms(*data->state);
    time_t rally_seconds = rally_ms / 1000;
    struct tm* rally_tm = localtime(&rally_seconds);
    char buf[32];
    snprintf(buf, sizeof(buf), "%04d/%02d/%02d",
             rally_tm->tm_year + 1900, rally_tm->tm_mon + 1, rally_tm->tm_mday);
    gtk_entry_set_text(data->dateEntry, buf);
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
             rally_tm->tm_hour, rally_tm->tm_min, rally_tm->tm_sec);
    gtk_entry_set_text(data->timeEntry, buf);
}

// Create numeric keypad widget
GtkWidget* createNumericKeypad(AppData* data) {
    GtkWidget* keypad = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(keypad), 5);
    gtk_grid_set_column_spacing(GTK_GRID(keypad), 5);
    
    const char* digits[] = {"7", "8", "9", "4", "5", "6", "1", "2", "3", ";", "0", "."};
    
    for (int i = 0; i < 12; i++) {
        GtkWidget* btn = gtk_button_new_with_label(digits[i]);
        gtk_widget_set_size_request(btn, 60, 48);
        g_signal_connect(btn, "clicked", G_CALLBACK(on_keypad_digit), data);
        gtk_grid_attach(GTK_GRID(keypad), btn, i % 3, i / 3, 1, 1);
    }
    
    // Row 5: Clear and Backspace
    GtkWidget* clearBtn = gtk_button_new_with_label("C");
    gtk_widget_set_size_request(clearBtn, 60, 48);
    g_signal_connect(clearBtn, "clicked", G_CALLBACK(on_keypad_clear), data);
    gtk_grid_attach(GTK_GRID(keypad), clearBtn, 0, 4, 1, 1);
    
    GtkWidget* bkspBtn = gtk_button_new_with_label("<-");
    gtk_widget_set_size_request(bkspBtn, 130, 48);
    g_signal_connect(bkspBtn, "clicked", G_CALLBACK(on_keypad_backspace), data);
    gtk_grid_attach(GTK_GRID(keypad), bkspBtn, 1, 4, 2, 1);
    
    return keypad;
}

GtkWidget* createDateTimeKeypad(AppData* data) {
    GtkWidget* keypad = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(keypad), 5);
    gtk_grid_set_column_spacing(GTK_GRID(keypad), 5);
    
    const char* digits[] = {"7", "8", "9", "4", "5", "6", "1", "2", "3", "/", "0", ":"};
    
    for (int i = 0; i < 12; i++) {
        GtkWidget* btn = gtk_button_new_with_label(digits[i]);
        gtk_widget_set_size_request(btn, 60, 48);
        g_signal_connect(btn, "clicked", G_CALLBACK(on_keypad_digit), data);
        gtk_grid_attach(GTK_GRID(keypad), btn, i % 3, i / 3, 1, 1);
    }
    
    GtkWidget* clearBtn = gtk_button_new_with_label("C");
    gtk_widget_set_size_request(clearBtn, 60, 48);
    g_signal_connect(clearBtn, "clicked", G_CALLBACK(on_keypad_clear), data);
    gtk_grid_attach(GTK_GRID(keypad), clearBtn, 0, 4, 1, 1);
    
    GtkWidget* bkspBtn = gtk_button_new_with_label("<-");
    gtk_widget_set_size_request(bkspBtn, 130, 48);
    g_signal_connect(bkspBtn, "clicked", G_CALLBACK(on_keypad_backspace), data);
    gtk_grid_attach(GTK_GRID(keypad), bkspBtn, 1, 4, 2, 1);
    
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
            data->state->segments[index].target_speed_kph = kph;
            data->state->segments[index].target_speed_counts_per_hour = kphToCountsPerHour(kph, data->state->calibration);
        } else if (strcmp(entry_type, "distance") == 0) {
            double meters = std::stod(text);
            data->state->segments[index].distance_m = meters;
            data->state->segments[index].distance_counts = (meters * 1e6) / data->state->calibration;
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
        double target_kph = seg.target_speed_kph;
        long distance_m = static_cast<long>(seg.distance_m);
        
        GtkWidget* row = gtk_list_box_row_new();
        GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        gtk_style_context_add_class(gtk_widget_get_style_context(box), "segment-row");
        gtk_container_add(GTK_CONTAINER(row), box);
        
        // Speed entry (editable)
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << target_kph;
        GtkWidget* speedEntry = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(speedEntry), ss.str().c_str());
        gtk_widget_set_size_request(speedEntry, 100, 36);
        g_object_set_data(G_OBJECT(speedEntry), "app_data", data);
        g_object_set_data(G_OBJECT(speedEntry), "entry_type", (gpointer)"speed");
        g_signal_connect(speedEntry, "changed", G_CALLBACK(on_segment_entry_changed), GINT_TO_POINTER(i));
        g_signal_connect(speedEntry, "focus-in-event", G_CALLBACK(on_entry_focus), data);
        
        // Distance entry (editable)
        ss.str("");
        ss << distance_m;
        GtkWidget* distEntry = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(distEntry), ss.str().c_str());
        gtk_widget_set_size_request(distEntry, 100, 36);
        g_object_set_data(G_OBJECT(distEntry), "app_data", data);
        g_object_set_data(G_OBJECT(distEntry), "entry_type", (gpointer)"distance");
        g_signal_connect(distEntry, "changed", G_CALLBACK(on_segment_entry_changed), GINT_TO_POINTER(i));
        g_signal_connect(distEntry, "focus-in-event", G_CALLBACK(on_entry_focus), data);
        
        // Auto checkbox (editable)
        GtkWidget* autoCheck = gtk_check_button_new();
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(autoCheck), seg.autoNext);
        g_object_set_data(G_OBJECT(autoCheck), "app_data", data);
        g_signal_connect(autoCheck, "toggled", G_CALLBACK(on_segment_auto_toggled), GINT_TO_POINTER(i));
        
        // Time label (display only: mm:ss to cover distance at speed)
        int time_secs = (seg.distance_m > 0 && seg.target_speed_kph > 0)
            ? static_cast<int>(seg.distance_m / (seg.target_speed_kph * 1000.0 / 3600.0))
            : 0;
        ss.str("");
        ss << std::setw(2) << std::setfill('0') << (time_secs / 60)
           << ":" << std::setw(2) << std::setfill('0') << (time_secs % 60);
        GtkWidget* timeLabel = gtk_label_new(ss.str().c_str());
        gtk_style_context_add_class(gtk_widget_get_style_context(timeLabel), "segment-label");
        
        // Delete button
        GtkWidget* deleteBtn = gtk_button_new_with_label("del");
        gtk_widget_set_size_request(deleteBtn, -1, 36);
        g_object_set_data(G_OBJECT(deleteBtn), "app_data", data);
        g_signal_connect(deleteBtn, "clicked", G_CALLBACK(on_delete_segment), GINT_TO_POINTER(i));
        
        gtk_box_pack_start(GTK_BOX(box), speedEntry, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(box), distEntry, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(box), autoCheck, FALSE, FALSE, 15);
        gtk_box_pack_start(GTK_BOX(box), timeLabel, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(box), deleteBtn, FALSE, FALSE, 5);
        
        gtk_list_box_insert(data->segmentListBox, row, -1);
        gtk_widget_show_all(row);
    }
}

// Callback for calibration start button
void on_calibration_start(G_GNUC_UNUSED GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    auto current_poll = data->poller->getMostRecent();
    
    // Remember baseline counter values
    data->cal_start_cntr1 = current_poll.cntr1;
    data->cal_start_cntr2 = current_poll.cntr2;
    data->cal_started = true;
    
    // Update display immediately
    updateCalibrationDisplay(data);
}

// Helper function to update calibration display
void updateCalibrationDisplay(AppData* data) {
    auto current_poll = data->poller->getMostRecent();
    
    // Calculate counts from calibration start (or total start if not started)
    uint64_t start_cntr1 = data->cal_started ? data->cal_start_cntr1 : data->state->total_start_cntr1;
    uint64_t start_cntr2 = data->cal_started ? data->cal_start_cntr2 : data->state->total_start_cntr2;
    
    // Individual counter differences
    int64_t cntr1_diff = current_poll.cntr1 - start_cntr1;
    int64_t cntr2_diff = current_poll.cntr2 - start_cntr2;
    
    // CNTR_A (calculated) - average if two counters, or just cntr1 if single
    int64_t cntr_a;
    if (data->state->counters) {
        // Two wheel counters - average
        cntr_a = (cntr1_diff + cntr2_diff) / 2;
    } else {
        // Single gearbox counter
        cntr_a = cntr1_diff;
    }
    
    // Distance in meters
    long total_m = countsToCentimeters(cntr_a, data->state->calibration) / 100;
    
    // Format: "Total distance: xxx,xxx m  (counts calculated: CNTR_A  1: CNTR_1  2: CNTR_2)"
    std::stringstream ss;
    ss << "Total distance: " << total_m << " m  (counts calculated: " << cntr_a 
       << "   1: " << cntr1_diff << "   2: " << cntr2_diff << ")";
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
        double target_counts_per_hour = kphToCountsPerHour(target_kph, data->state->calibration);
        
        // Split distance by comma to allow multiple segments at the same speed
        std::string dist_input(dist_str);
        std::stringstream dist_stream(dist_input);
        std::string token;
        bool added = false;
        
        while (std::getline(dist_stream, token, ';')) {
            if (token.empty()) continue;
            double distance_m = std::stod(token);
            if (distance_m <= 0) continue;
            
            double distance_counts = (distance_m * 1e6) / data->state->calibration;
            
            Segment seg;
            seg.target_speed_kph = target_kph;
            seg.target_speed_counts_per_hour = target_counts_per_hour;
            seg.distance_m = distance_m;
            seg.distance_counts = distance_counts;
            seg.autoNext = autoNext;
            
            data->state->segments.push_back(seg);
            added = true;
        }
        
        if (added) {
            ConfigFile::save(*data->state);
            
            gtk_entry_set_text(data->targetSpeedEntry, "");
            gtk_entry_set_text(data->distanceEntry, "");
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->autoNextCheck), TRUE);
            
            refreshSegmentList(data);
        }
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

void on_memory_set(GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    int slot = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "slot")) - 1;
    if (slot >= 0 && slot < RallyState::MAX_MEMORY_SLOTS) {
        data->state->memory_slots[slot] = data->state->segments;
        ConfigFile::save(*data->state);
    }
}

void on_memory_recall(GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    int slot = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "slot")) - 1;
    if (slot >= 0 && slot < RallyState::MAX_MEMORY_SLOTS && !data->state->memory_slots[slot].empty()) {
        data->state->segments = data->state->memory_slots[slot];
        data->state->segment_current_number = data->state->segments.empty() ? -1 : 0;
        ConfigFile::save(*data->state);
        refreshSegmentList(data);
    }
}

void on_memory_clear(G_GNUC_UNUSED GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    for (int i = 0; i < RallyState::MAX_MEMORY_SLOTS; i++) {
        data->state->memory_slots[i].clear();
    }
    ConfigFile::save(*data->state);
}

void on_save_calibration(G_GNUC_UNUSED GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    
    const char* dist_str = gtk_entry_get_text(data->rallyDistEntry);
    if (dist_str && strlen(dist_str) > 0) {
        long rally_distance_m = std::stol(dist_str);
        
        if (rally_distance_m >= 500 && rally_distance_m <= 100000) {
            auto current_poll = data->poller->getMostRecent();
            
            // Use calibration start values if started, otherwise use total_start
            uint64_t start_cntr1 = data->cal_started ? data->cal_start_cntr1 : data->state->total_start_cntr1;
            uint64_t start_cntr2 = data->cal_started ? data->cal_start_cntr2 : data->state->total_start_cntr2;
            
            int64_t total_count_diff = calculateDistanceCounts(*data->state,
                current_poll.cntr1, current_poll.cntr2,
                start_cntr1, start_cntr2);
            
            if (total_count_diff > 0) {
                // new_cal = (input_meters * 1000 * 1000) / total_count_diff
                data->state->calibration = (rally_distance_m * 1000000) / total_count_diff;
                
                // Recalculate count-based values in all segments from stable human values
                for (auto& seg : data->state->segments) {
                    seg.target_speed_counts_per_hour = kphToCountsPerHour(seg.target_speed_kph, data->state->calibration);
                    seg.distance_counts = (seg.distance_m * 1e6) / data->state->calibration;
                }
                for (int i = 0; i < RallyState::MAX_MEMORY_SLOTS; i++) {
                    for (auto& seg : data->state->memory_slots[i]) {
                        seg.target_speed_counts_per_hour = kphToCountsPerHour(seg.target_speed_kph, data->state->calibration);
                        seg.distance_counts = (seg.distance_m * 1e6) / data->state->calibration;
                    }
                }
                
                ConfigFile::save(*data->state);
                
                // Clear entry and reset calibration state
                gtk_entry_set_text(data->rallyDistEntry, "");
                data->cal_started = false;
                updateCalibrationDisplay(data);
            }
        }
    }
}

void on_reset_calibration_1m(G_GNUC_UNUSED GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);

    // 1m per pulse: 1 count = 1000mm, so 1000 counts = 1,000,000mm
    data->state->calibration = 1000000;

    for (auto& seg : data->state->segments) {
        seg.target_speed_counts_per_hour = kphToCountsPerHour(seg.target_speed_kph, data->state->calibration);
        seg.distance_counts = (seg.distance_m * 1e6) / data->state->calibration;
    }
    for (int i = 0; i < RallyState::MAX_MEMORY_SLOTS; i++) {
        for (auto& seg : data->state->memory_slots[i]) {
            seg.target_speed_counts_per_hour = kphToCountsPerHour(seg.target_speed_kph, data->state->calibration);
            seg.distance_counts = (seg.distance_m * 1e6) / data->state->calibration;
        }
    }

    ConfigFile::save(*data->state);
    data->cal_started = false;
    gtk_entry_set_text(data->rallyDistEntry, "");
    updateCalibrationDisplay(data);
}

static void updateSensorModeLabel(AppData* data) {
    if (data->state->counters) {
        gtk_label_set_text(data->sensorModeLabel, "Currently set to both sensors");
    } else {
        gtk_label_set_text(data->sensorModeLabel, "Currently set to sensor 1");
    }
}

void on_set_sensor_1(G_GNUC_UNUSED GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    data->state->counters = false;

    for (auto& seg : data->state->segments) {
        seg.target_speed_counts_per_hour = kphToCountsPerHour(seg.target_speed_kph, data->state->calibration);
        seg.distance_counts = (seg.distance_m * 1e6) / data->state->calibration;
    }
    for (int i = 0; i < RallyState::MAX_MEMORY_SLOTS; i++) {
        for (auto& seg : data->state->memory_slots[i]) {
            seg.target_speed_counts_per_hour = kphToCountsPerHour(seg.target_speed_kph, data->state->calibration);
            seg.distance_counts = (seg.distance_m * 1e6) / data->state->calibration;
        }
    }

    ConfigFile::save(*data->state);
    updateSensorModeLabel(data);
    updateCalibrationDisplay(data);
}

void on_set_sensor_both(G_GNUC_UNUSED GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    data->state->counters = true;

    for (auto& seg : data->state->segments) {
        seg.target_speed_counts_per_hour = kphToCountsPerHour(seg.target_speed_kph, data->state->calibration);
        seg.distance_counts = (seg.distance_m * 1e6) / data->state->calibration;
    }
    for (int i = 0; i < RallyState::MAX_MEMORY_SLOTS; i++) {
        for (auto& seg : data->state->memory_slots[i]) {
            seg.target_speed_counts_per_hour = kphToCountsPerHour(seg.target_speed_kph, data->state->calibration);
            seg.distance_counts = (seg.distance_m * 1e6) / data->state->calibration;
        }
    }

    ConfigFile::save(*data->state);
    updateSensorModeLabel(data);
    updateCalibrationDisplay(data);
}

void on_alarm_set(GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    int km = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "km"));
    
    auto current_poll = data->poller->getMostRecent();
    int64_t total_counts = calculateDistanceCounts(*data->state,
        current_poll.cntr1, current_poll.cntr2,
        data->state->total_start_cntr1, data->state->total_start_cntr2);
    
    int64_t km_in_counts = static_cast<int64_t>((static_cast<double>(km) * 1000.0 * 1e6) / data->state->calibration);
    data->state->alarm_distance_km = km;
    data->state->alarm_target_counts = total_counts + km_in_counts;
    data->alarmSoundStartTime = 0;
    
    ConfigFile::save(*data->state);
}

void on_alarm_clear(G_GNUC_UNUSED GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    data->state->alarm_distance_km = 0;
    data->state->alarm_target_counts = 0;
    data->alarmSoundStartTime = 0;
    system("pkill -f 'aplay alarm.wav' 2>/dev/null");
    gtk_label_set_text(data->alarmCountdownLabel, "");
    ConfigFile::save(*data->state);
}

void on_exit_app(G_GNUC_UNUSED GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    ConfigFile::save(*data->state);
    gtk_main_quit();
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

// Epoch for auto_start: 2020-01-01 00:00:00 local time
static int64_t getAutoStartEpochMs() {
    struct tm epoch_tm = {};
    epoch_tm.tm_year = 120;  // 2020
    epoch_tm.tm_mon = 0;
    epoch_tm.tm_mday = 1;
    return static_cast<int64_t>(mktime(&epoch_tm)) * 1000;
}

void on_show_autostart(G_GNUC_UNUSED GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    gtk_stack_set_visible_child_name(data->copilotStack, "autostart");
    data->activeEntry = data->autoStartTimeEntry;
    
    if (data->state->auto_start_rally_time_minutes > 0) {
        int64_t target_ms = getAutoStartEpochMs() + 
            static_cast<int64_t>(data->state->auto_start_rally_time_minutes) * 60000;
        time_t target_s = target_ms / 1000;
        struct tm* t = localtime(&target_s);
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
        gtk_entry_set_text(data->autoStartTimeEntry, buf);
    } else {
        gtk_entry_set_text(data->autoStartTimeEntry, "");
    }
    
    updateAutoStartDisplay(data);
}

void updateAutoStartDisplay(AppData* data) {
    int64_t rally_ms = getRallyTime_ms(*data->state);
    time_t rally_s = rally_ms / 1000;
    struct tm* rally_tm = localtime(&rally_s);
    
    char buf[64];
    snprintf(buf, sizeof(buf), "%04d/%02d/%02d  %02d:%02d:%02d",
             rally_tm->tm_year + 1900, rally_tm->tm_mon + 1, rally_tm->tm_mday,
             rally_tm->tm_hour, rally_tm->tm_min, rally_tm->tm_sec);
    gtk_label_set_text(data->autoStartRallyClockLabel, buf);
    
    if (data->state->auto_start_rally_time_minutes > 0) {
        int64_t target_ms = getAutoStartEpochMs() + 
            static_cast<int64_t>(data->state->auto_start_rally_time_minutes) * 60000;
        time_t target_s = target_ms / 1000;
        struct tm* t = localtime(&target_s);
        snprintf(buf, sizeof(buf), "%04d/%02d/%02d  %02d:%02d:%02d",
                 t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                 t->tm_hour, t->tm_min, t->tm_sec);
        gtk_label_set_text(data->autoStartTimeLabel, buf);
    } else {
        gtk_label_set_text(data->autoStartTimeLabel, "");
    }
}

void on_autostart_set(G_GNUC_UNUSED GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    
    const char* time_str = gtk_entry_get_text(data->autoStartTimeEntry);
    if (!time_str || strlen(time_str) == 0) return;
    
    int hour = 0, min = 0, sec = 0;
    if (sscanf(time_str, "%d:%d:%d", &hour, &min, &sec) < 2) return;
    
    if (hour < 0 || hour > 23 || min < 0 || min > 59 || sec < 0 || sec > 59) return;
    
    // Build target time: use today's rally date with the entered time
    int64_t rally_ms = getRallyTime_ms(*data->state);
    time_t rally_s = rally_ms / 1000;
    struct tm target_tm = *localtime(&rally_s);
    target_tm.tm_hour = hour;
    target_tm.tm_min = min;
    target_tm.tm_sec = sec;
    
    int64_t target_ms = static_cast<int64_t>(mktime(&target_tm)) * 1000;
    int64_t diff_ms = target_ms - rally_ms;
    
    if (diff_ms <= 0) {
        gtk_entry_set_text(data->autoStartTimeEntry, "Error: time in past");
        return;
    }
    if (diff_ms > 3 * 3600 * 1000) {
        gtk_entry_set_text(data->autoStartTimeEntry, "Error: >3 hours");
        return;
    }
    
    int64_t epoch_ms = getAutoStartEpochMs();
    data->state->auto_start_rally_time_minutes = 
        static_cast<uint64_t>((target_ms - epoch_ms) / 60000);
    data->autoStartTriggered = false;
    
    ConfigFile::save(*data->state);
    updateAutoStartDisplay(data);
}

void on_autostart_clear(G_GNUC_UNUSED GtkWidget* widget, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    data->state->auto_start_rally_time_minutes = 0;
    data->autoStartTriggered = false;
    ConfigFile::save(*data->state);
    gtk_entry_set_text(data->autoStartTimeEntry, "");
    updateAutoStartDisplay(data);
}
