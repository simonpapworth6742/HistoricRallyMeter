#include "ui_driver.h"
#include "calculations.h"
#include "rally_types.h"
#include "rally_state.h"
#include "counter_poller.h"
#include <iomanip>
#include <sstream>
#include <cmath>
#include <cstdio>

// Auto-scaling gauge: three scales with hysteresis to avoid flickering.
// Scale 0: ±3 seconds   (fine - when close to target)
// Scale 1: ±10 seconds  (medium)
// Scale 2: ±5 minutes   (coarse - when very far off)
static void updateGaugeScale(AppData* data) {
    double abs_sec = std::abs(data->aheadBehindSeconds);
    int scale = data->gaugeScale;

    // Hysteresis: scale up when value exceeds range, scale down when comfortably inside
    switch (scale) {
    case 0: // ±3s
        if (abs_sec > 3.0) scale = 1;
        break;
    case 1: // ±10s
        if (abs_sec > 10.0) scale = 2;
        else if (abs_sec < 2.0) scale = 0;
        break;
    case 2: // ±5min
        if (abs_sec < 8.0) scale = 1;
        break;
    }
    data->gaugeScale = scale;
}

struct GaugeScaleInfo {
    double max_seconds;       // full-scale value in seconds
    int major_step;           // major tick interval (in display units)
    int minor_divisions;      // minor ticks between majors
    int num_arc_segments;     // segments for graduated arc
    const char* unit_label;   // "sec" or "min"
    bool labels_in_minutes;   // show labels as minutes
};

static GaugeScaleInfo getGaugeScaleInfo(int scale) {
    switch (scale) {
    case 0:  return { 3.0,   1,  5,  30, "sec", false };
    case 2:  return { 300.0, 1, 5,  30, "min", true  };
    default: return { 10.0,  5,  5,  40, "sec", false };
    }
}

// Draw the rally gauge (GaugePilot RallyMaster style) with auto-scaling
static gboolean on_gauge_draw(GtkWidget* widget, cairo_t* cr, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);

    GtkAllocation alloc;
    gtk_widget_get_allocation(widget, &alloc);

    double width = alloc.width;
    double height = alloc.height;

    double centerX = width / 2;
    double centerY = height - 15;
    double radius = std::min(width / 2, height) - 25;

    updateGaugeScale(data);
    GaugeScaleInfo si = getGaugeScaleInfo(data->gaugeScale);
    double max_val = si.max_seconds;
    double display_max = si.labels_in_minutes ? max_val / 60.0 : max_val;

    // Background
    cairo_set_source_rgb(cr, 0.05, 0.05, 0.05);
    cairo_paint(cr);

    // Outer bezel ring
    cairo_set_source_rgb(cr, 0.25, 0.25, 0.25);
    cairo_set_line_width(cr, 4);
    cairo_arc(cr, centerX, centerY, radius + 18, M_PI, 2 * M_PI);
    cairo_stroke(cr);

    // Arc background
    cairo_set_source_rgb(cr, 0.12, 0.12, 0.12);
    cairo_set_line_width(cr, 28);
    cairo_arc(cr, centerX, centerY, radius, M_PI, 2 * M_PI);
    cairo_stroke(cr);

    // Graduated yellow/gold arc
    int segments = si.num_arc_segments;
    for (int i = 0; i <= segments; i++) {
        double frac = -1.0 + (2.0 * i) / segments;
        double angle = M_PI + M_PI/2 + frac * (M_PI / 2);
        double next_frac = -1.0 + (2.0 * (i + 1)) / segments;
        double next_angle = M_PI + M_PI/2 + next_frac * (M_PI / 2);

        double intensity = std::abs(frac);
        double r = 0.85 * (0.3 + 0.7 * intensity);
        double g = 0.65 * (0.3 + 0.7 * intensity);

        cairo_set_source_rgb(cr, r, g, 0.0);
        cairo_set_line_width(cr, 12);
        cairo_arc(cr, centerX, centerY, radius, angle, next_angle);
        cairo_stroke(cr);
    }

    // Major ticks and labels
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_set_line_width(cr, 2.5);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 13);

    int num_major = static_cast<int>(display_max / si.major_step);
    for (int i = -num_major; i <= num_major; i++) {
        double display_val = i * si.major_step;
        double frac = display_val / display_max;
        double angle = M_PI + M_PI/2 + frac * (M_PI / 2);

        double x1 = centerX + (radius - 20) * cos(angle);
        double y1 = centerY + (radius - 20) * sin(angle);
        double x2 = centerX + (radius + 8) * cos(angle);
        double y2 = centerY + (radius + 8) * sin(angle);

        cairo_set_line_width(cr, 2.5);
        cairo_move_to(cr, x1, y1);
        cairo_line_to(cr, x2, y2);
        cairo_stroke(cr);

        if (i == 0) continue;

        char label[16];
        snprintf(label, sizeof(label), "%d", std::abs(static_cast<int>(display_val)));
        cairo_text_extents_t extents;
        cairo_text_extents(cr, label, &extents);
        double label_r = radius - 32;
        double lx = centerX + label_r * cos(angle) - extents.width / 2;
        double ly = centerY + label_r * sin(angle) + extents.height / 2;
        cairo_move_to(cr, lx, ly);
        cairo_show_text(cr, label);
    }

    // Minor ticks
    cairo_set_line_width(cr, 1);
    int total_minor = num_major * si.minor_divisions;
    for (int i = -total_minor; i <= total_minor; i++) {
        double frac = (double)i / total_minor;
        // Skip positions that coincide with major ticks
        if (i % si.minor_divisions == 0) continue;

        double angle = M_PI + M_PI/2 + frac * (M_PI / 2);
        double x1 = centerX + (radius - 10) * cos(angle);
        double y1 = centerY + (radius - 10) * sin(angle);
        double x2 = centerX + (radius + 4) * cos(angle);
        double y2 = centerY + (radius + 4) * sin(angle);

        cairo_move_to(cr, x1, y1);
        cairo_line_to(cr, x2, y2);
        cairo_stroke(cr);
    }

    // Unit labels: "- sec/min" on left, "sec/min +" on right
    cairo_set_font_size(cr, 11);
    cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);

    char left_label[16], right_label[16];
    snprintf(left_label, sizeof(left_label), "- %s", si.unit_label);
    snprintf(right_label, sizeof(right_label), "%s +", si.unit_label);

    cairo_text_extents_t ext;
    cairo_text_extents(cr, left_label, &ext);
    cairo_move_to(cr, centerX - radius + 5, centerY - 5);
    cairo_show_text(cr, left_label);

    cairo_text_extents(cr, right_label, &ext);
    cairo_move_to(cr, centerX + radius - ext.width - 5, centerY - 5);
    cairo_show_text(cr, right_label);

    // Center triangle marker at 0
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    double tri_y = centerY - radius - 12;
    cairo_move_to(cr, centerX, tri_y + 10);
    cairo_line_to(cr, centerX - 6, tri_y);
    cairo_line_to(cr, centerX + 6, tri_y);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Digital display box
    double box_width = 70;
    double box_height = 28;
    double box_x = centerX - box_width / 2;
    double box_y = centerY - 45;

    cairo_set_source_rgb(cr, 0.08, 0.08, 0.08);
    cairo_rectangle(cr, box_x, box_y, box_width, box_height);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
    cairo_set_line_width(cr, 1.5);
    cairo_rectangle(cr, box_x, box_y, box_width, box_height);
    cairo_stroke(cr);

    // Digital readout
    double seconds = data->aheadBehindSeconds;
    char digital[20];
    if (si.labels_in_minutes) {
        double abs_min = std::abs(seconds) / 60.0;
        snprintf(digital, sizeof(digital), "%04.1f", abs_min);
    } else {
        snprintf(digital, sizeof(digital), "%05.1f", std::abs(seconds));
    }

    cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 18);
    cairo_set_source_rgb(cr, 0.9, 0.75, 0.0);

    cairo_text_extents_t dext;
    cairo_text_extents(cr, digital, &dext);
    cairo_move_to(cr, centerX - dext.width / 2, box_y + box_height / 2 + dext.height / 2 - 2);
    cairo_show_text(cr, digital);

    // Needle
    double needle_seconds = seconds;
    if (needle_seconds > max_val) needle_seconds = max_val;
    if (needle_seconds < -max_val) needle_seconds = -max_val;

    double needle_angle = M_PI + M_PI/2 + (needle_seconds / max_val) * (M_PI / 2);
    double needle_length = radius - 45;

    // Needle shadow
    cairo_set_source_rgba(cr, 0, 0, 0, 0.4);
    cairo_set_line_width(cr, 5);
    cairo_move_to(cr, centerX + 2, centerY + 2);
    cairo_line_to(cr, centerX + 2 + needle_length * cos(needle_angle),
                  centerY + 2 + needle_length * sin(needle_angle));
    cairo_stroke(cr);

    // Needle
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_set_line_width(cr, 3);
    cairo_move_to(cr, centerX, centerY);
    cairo_line_to(cr, centerX + needle_length * cos(needle_angle),
                  centerY + needle_length * sin(needle_angle));
    cairo_stroke(cr);

    // Needle hub
    cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
    cairo_arc(cr, centerX, centerY, 10, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
    cairo_set_line_width(cr, 1);
    cairo_arc(cr, centerX, centerY, 10, 0, 2 * M_PI);
    cairo_stroke(cr);

    // Center dot
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_arc(cr, centerX, centerY, 4, 0, 2 * M_PI);
    cairo_fill(cr);

    return FALSE;
}

void updateDriverDisplay(AppData* data) {
    auto current_poll = data->poller->getMostRecent();
    auto tenth_poll = data->poller->get10th();
    auto current_time_ms = getRallyTime_ms(*data->state);
    
    // Current speed (from 10-second rolling average)
    double current_speed = calculateCurrentSpeed(*data->state, current_poll, tenth_poll);
    std::stringstream ss;
    if (current_speed < 0) {
        ss << "--.--";
    } else {
        ss << std::fixed << std::setprecision(2) << current_speed;
    }
    gtk_label_set_text(data->currentSpeedLabel, ss.str().c_str());
    
    // Trip average speed
    int64_t trip_count_diff = calculateDistanceCounts(*data->state, 
        current_poll.cntr1, current_poll.cntr2,
        data->state->trip_start_cntr1, data->state->trip_start_cntr2);
    double trip_speed = calculateAverageSpeed(*data->state,
        data->state->trip_start_time_ms, current_time_ms, trip_count_diff);
    ss.str("");
    ss << std::fixed << std::setprecision(2) << trip_speed;
    gtk_label_set_text(data->tripSpeedLabel, ss.str().c_str());
    
    // Total average speed
    int64_t total_count_diff = calculateDistanceCounts(*data->state,
        current_poll.cntr1, current_poll.cntr2,
        data->state->total_start_cntr1, data->state->total_start_cntr2);
    double total_speed = calculateAverageSpeed(*data->state,
        data->state->total_start_time_ms, current_time_ms, total_count_diff);
    ss.str("");
    ss << std::fixed << std::setprecision(2) << total_speed;
    gtk_label_set_text(data->totalSpeedLabel, ss.str().c_str());
    
    // Target speed and ahead/behind
    if (data->state->segment_current_number >= 0 && 
        data->state->segment_current_number < static_cast<long>(data->state->segments.size())) {
        const Segment& seg = data->state->segments[data->state->segment_current_number];
        double target_kph = countsPerHourToKPH(seg.target_speed_counts_per_hour, data->state->calibration);
        if (data->state->units) {
            target_kph = target_kph * 0.621371;  // Convert to MPH
        }
        ss.str("");
        ss << std::fixed << std::setprecision(2) << target_kph;
        gtk_label_set_text(data->targetSpeedLabel, ss.str().c_str());
        
        // Ahead/behind - calculated from stage start accounting for all segment speeds
        int64_t total_count_diff_ab = calculateDistanceCounts(*data->state,
            current_poll.cntr1, current_poll.cntr2,
            data->state->total_start_cntr1, data->state->total_start_cntr2);
        double seconds = calculateAheadBehindFromStageStart(*data->state, current_time_ms, total_count_diff_ab);
        
        // Store for gauge
        data->aheadBehindSeconds = seconds;
        
        // Format as mm:ss.s (not hh:mm:ss.ss)
        ss.str("");
        if (seconds >= 0) {
            ss << "+";
        } else {
            ss << "-";
        }
        double abs_seconds = std::abs(seconds);
        int total_sec = static_cast<int>(abs_seconds);
        int tenths = static_cast<int>((abs_seconds - total_sec) * 10);
        int mins = total_sec / 60;
        int secs = total_sec % 60;
        ss << std::setfill('0') << std::setw(2) << mins << ":"
           << std::setw(2) << secs << "." << tenths;
        gtk_label_set_text(data->aheadBehindLabel, ss.str().c_str());
        
        // Speed adjustment arrows - only if more than 0.1 seconds off
        GtkStyleContext* arrowCtx = gtk_widget_get_style_context(GTK_WIDGET(data->speedAdjustArrowsLabel));
        gtk_style_context_remove_class(arrowCtx, "arrows-up");
        gtk_style_context_remove_class(arrowCtx, "arrows-down");
        
        if (abs_seconds > 0.1 && target_kph > 0) {
            // Calculate speed needed to match target in next 500 meters
            double target_kph_raw = countsPerHourToKPH(seg.target_speed_counts_per_hour, data->state->calibration);
            double target_time_s = 500.0 / (target_kph_raw / 3.6);
            
            double adjusted_time_s;
            if (seconds < 0) {
                adjusted_time_s = target_time_s - abs_seconds;
            } else {
                adjusted_time_s = target_time_s + abs_seconds;
            }
            
            if (adjusted_time_s > 0.1) {
                double needed_kph = (500.0 / adjusted_time_s) * 3.6;
                double speed_diff = needed_kph - target_kph_raw;
                
                if (data->state->units) {
                    speed_diff = speed_diff * 0.621371;
                }
                
                double abs_diff = std::abs(speed_diff);
                int num_arrows = 0;
                if (abs_diff >= 5.0) {
                    num_arrows = 3;
                } else if (abs_diff >= 3.0) {
                    num_arrows = 2;
                } else if (abs_diff > 0) {
                    num_arrows = 1;
                }
                
                if (num_arrows > 0) {
                    ss.str("");
                    if (speed_diff > 0) {
                        for (int i = 0; i < num_arrows; i++) ss << "↑";
                        gtk_style_context_add_class(arrowCtx, "arrows-up");
                    } else {
                        for (int i = 0; i < num_arrows; i++) ss << "↓";
                        gtk_style_context_add_class(arrowCtx, "arrows-down");
                    }
                    gtk_label_set_text(data->speedAdjustArrowsLabel, ss.str().c_str());
                } else {
                    gtk_label_set_text(data->speedAdjustArrowsLabel, "");
                }
            } else {
                gtk_label_set_text(data->speedAdjustArrowsLabel, "");
            }
        } else {
            gtk_label_set_text(data->speedAdjustArrowsLabel, "");
        }
        
        // Redraw gauge
        gtk_widget_queue_draw(data->rallyGaugeDrawingArea);
    } else {
        gtk_label_set_text(data->targetSpeedLabel, "--.--");
        gtk_label_set_text(data->aheadBehindLabel, "--:--.--");
        gtk_label_set_text(data->speedAdjustArrowsLabel, "");
        data->aheadBehindSeconds = 0.0;
        if (data->rallyGaugeDrawingArea) {
            gtk_widget_queue_draw(data->rallyGaugeDrawingArea);
        }
    }
    
    // Next segment info
    if (data->state->segment_current_number >= 0 && 
        data->state->segment_current_number < static_cast<long>(data->state->segments.size()) - 1) {
        const Segment& current_seg = data->state->segments[data->state->segment_current_number];
        int64_t seg_count_diff = calculateDistanceCounts(*data->state,
            current_poll.cntr1, current_poll.cntr2,
            data->state->segment_start_cntr1, data->state->segment_start_cntr2);
        
        double remaining_counts = current_seg.distance_counts - static_cast<double>(seg_count_diff);
        double remaining_m = countsToMeters(static_cast<int64_t>(remaining_counts), data->state->calibration);
        
        const Segment& next_seg = data->state->segments[data->state->segment_current_number + 1];
        double next_target = countsPerHourToKPH(next_seg.target_speed_counts_per_hour, data->state->calibration);
        if (data->state->units) {
            next_target = next_target * 0.621371;
        }
        
        if (current_speed > 0 && current_speed != -1) {
            double speed_m_per_s = current_speed;
            if (data->state->units) {
                speed_m_per_s = speed_m_per_s * 1.60934;
            }
            speed_m_per_s = speed_m_per_s / 3.6;
            double eta_seconds = remaining_m / speed_m_per_s;
            
            if (eta_seconds < 0) {
                ss.str("");
                ss << "Over by " << formatDuration(static_cast<int64_t>(-eta_seconds * 1000));
                gtk_label_set_text(data->nextSegLabel, ss.str().c_str());
            } else {
                ss.str("");
                ss << "next: " << std::fixed << std::setprecision(2) << next_target 
                   << " in " << static_cast<long>(remaining_m) << " m  ETA " << formatDuration(static_cast<int64_t>(eta_seconds * 1000));
                gtk_label_set_text(data->nextSegLabel, ss.str().c_str());
            }
        } else {
            ss.str("");
            ss << "next: " << std::fixed << std::setprecision(2) << next_target 
               << " in " << static_cast<long>(remaining_m) << " m  ETA --:--:--";
            gtk_label_set_text(data->nextSegLabel, ss.str().c_str());
        }
    } else {
        gtk_label_set_text(data->nextSegLabel, "");
    }
    
    // Updates per second
    data->updateCount++;
    if (current_time_ms - data->lastUpdateCountTime_ms >= 1000) {
        ss.str("");
        ss << "updates/sec: " << data->updateCount;
        gtk_label_set_text(data->updatesPerSecLabel, ss.str().c_str());
        data->updateCount = 0;
        data->lastUpdateCountTime_ms = current_time_ms;
    }
}

// Apply CSS styling for large fonts
static void applyDriverCSS(GtkWidget* G_GNUC_UNUSED widget) {
    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        ".speed-header { font-size: 28px; font-weight: bold; }"
        ".speed-value { font-size: 64px; font-weight: bold; font-family: monospace; }"
        ".target-info { font-size: 22px; font-weight: bold; }"
        ".ahead-behind { font-size: 28px; font-weight: bold; font-family: monospace; }"
        ".next-info { font-size: 18px; }"
        ".footer-info { font-size: 14px; }"
        ".speed-arrows { font-size: 36px; font-weight: bold; }"
        ".arrows-up { color: #00CC00; }"
        ".arrows-down { color: #EE0000; }",
        -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

GtkWidget* createDriverWindow(AppData* data) {
    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Driver Display");
    gtk_window_set_default_size(GTK_WINDOW(window), 1280, 400);
    
    applyDriverCSS(window);
    
    GtkWidget* mainBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(mainBox), 5);
    gtk_container_add(GTK_CONTAINER(window), mainBox);
    
    // Main content: left side speeds, right side gauge
    GtkWidget* contentBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(mainBox), contentBox, TRUE, TRUE, 0);
    
    // Left side: Speed displays
    GtkWidget* speedsBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(contentBox), speedsBox, TRUE, TRUE, 0);
    
    // Speed headers
    GtkWidget* headerBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(speedsBox), headerBox, FALSE, FALSE, 0);
    
    GtkWidget* currentHeader = gtk_label_new("Current");
    GtkWidget* tripHeader = gtk_label_new("Trip");
    GtkWidget* totalHeader = gtk_label_new("Total");
    
    gtk_style_context_add_class(gtk_widget_get_style_context(currentHeader), "speed-header");
    gtk_style_context_add_class(gtk_widget_get_style_context(tripHeader), "speed-header");
    gtk_style_context_add_class(gtk_widget_get_style_context(totalHeader), "speed-header");
    
    gtk_widget_set_halign(currentHeader, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(tripHeader, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(totalHeader, GTK_ALIGN_CENTER);
    
    gtk_box_pack_start(GTK_BOX(headerBox), currentHeader, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(headerBox), tripHeader, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(headerBox), totalHeader, TRUE, TRUE, 0);
    
    // Speed values
    GtkWidget* valuesBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(speedsBox), valuesBox, TRUE, TRUE, 0);
    
    data->currentSpeedLabel = GTK_LABEL(gtk_label_new("--.--"));
    data->tripSpeedLabel = GTK_LABEL(gtk_label_new("--.--"));
    data->totalSpeedLabel = GTK_LABEL(gtk_label_new("--.--"));
    
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->currentSpeedLabel)), "speed-value");
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->tripSpeedLabel)), "speed-value");
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->totalSpeedLabel)), "speed-value");
    
    gtk_widget_set_halign(GTK_WIDGET(data->currentSpeedLabel), GTK_ALIGN_CENTER);
    gtk_widget_set_halign(GTK_WIDGET(data->tripSpeedLabel), GTK_ALIGN_CENTER);
    gtk_widget_set_halign(GTK_WIDGET(data->totalSpeedLabel), GTK_ALIGN_CENTER);
    gtk_widget_set_valign(GTK_WIDGET(data->currentSpeedLabel), GTK_ALIGN_CENTER);
    gtk_widget_set_valign(GTK_WIDGET(data->tripSpeedLabel), GTK_ALIGN_CENTER);
    gtk_widget_set_valign(GTK_WIDGET(data->totalSpeedLabel), GTK_ALIGN_CENTER);
    
    gtk_box_pack_start(GTK_BOX(valuesBox), GTK_WIDGET(data->currentSpeedLabel), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(valuesBox), GTK_WIDGET(data->tripSpeedLabel), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(valuesBox), GTK_WIDGET(data->totalSpeedLabel), TRUE, TRUE, 0);
    
    // Right side: Rally gauge and info
    GtkWidget* gaugeBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_size_request(gaugeBox, 450, -1);
    gtk_box_pack_end(GTK_BOX(contentBox), gaugeBox, FALSE, FALSE, 10);
    
    // Units label at top right
    data->unitsLabel = GTK_LABEL(gtk_label_new(data->state->units ? "(MPH)" : "(KPH)"));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->unitsLabel)), "speed-header");
    gtk_widget_set_halign(GTK_WIDGET(data->unitsLabel), GTK_ALIGN_END);
    gtk_box_pack_start(GTK_BOX(gaugeBox), GTK_WIDGET(data->unitsLabel), FALSE, FALSE, 0);
    
    // Rally gauge drawing area
    data->rallyGaugeDrawingArea = gtk_drawing_area_new();
    gtk_widget_set_size_request(data->rallyGaugeDrawingArea, 400, 180);
    g_signal_connect(data->rallyGaugeDrawingArea, "draw", G_CALLBACK(on_gauge_draw), data);
    gtk_box_pack_start(GTK_BOX(gaugeBox), data->rallyGaugeDrawingArea, TRUE, TRUE, 0);
    
    // Target speed and ahead/behind below gauge
    GtkWidget* targetInfoBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(targetInfoBox, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(gaugeBox), targetInfoBox, FALSE, FALSE, 0);
    
    GtkWidget* targetLabel = gtk_label_new("target:");
    data->targetSpeedLabel = GTK_LABEL(gtk_label_new("--.--"));
    data->aheadBehindLabel = GTK_LABEL(gtk_label_new("--:--.--"));
    data->speedAdjustArrowsLabel = GTK_LABEL(gtk_label_new(""));
    
    gtk_style_context_add_class(gtk_widget_get_style_context(targetLabel), "target-info");
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->targetSpeedLabel)), "target-info");
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->aheadBehindLabel)), "ahead-behind");
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->speedAdjustArrowsLabel)), "speed-arrows");
    
    gtk_box_pack_start(GTK_BOX(targetInfoBox), targetLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(targetInfoBox), GTK_WIDGET(data->targetSpeedLabel), FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(targetInfoBox), GTK_WIDGET(data->aheadBehindLabel), FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(targetInfoBox), GTK_WIDGET(data->speedAdjustArrowsLabel), FALSE, FALSE, 5);
    
    // Bottom row: Updates counter, next segment, unit toggle
    GtkWidget* footerBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_end(GTK_BOX(mainBox), footerBox, FALSE, FALSE, 5);
    
    data->updatesPerSecLabel = GTK_LABEL(gtk_label_new("updates/sec: 0"));
    data->nextSegLabel = GTK_LABEL(gtk_label_new(""));
    data->unitToggleBtn = GTK_BUTTON(gtk_button_new_with_label(data->state->units ? "MPH" : "KPH"));
    
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->updatesPerSecLabel)), "footer-info");
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->nextSegLabel)), "next-info");
    
    gtk_widget_set_halign(GTK_WIDGET(data->nextSegLabel), GTK_ALIGN_CENTER);
    
    gtk_box_pack_start(GTK_BOX(footerBox), GTK_WIDGET(data->updatesPerSecLabel), FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(footerBox), GTK_WIDGET(data->nextSegLabel), TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(footerBox), GTK_WIDGET(data->unitToggleBtn), FALSE, FALSE, 10);
    
    return window;
}
