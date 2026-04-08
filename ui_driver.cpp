#include "ui_driver.h"
#include "calculations.h"
#include "rally_types.h"
#include "rally_state.h"
#include "counter_poller.h"
#include "tone_generator.h"
#include "callbacks.h"
#include <iomanip>
#include <sstream>
#include <cmath>
#include <cstdio>
#include <chrono>
#include <fstream>

static std::string readCpuTemp() {
    std::ifstream f("/sys/class/thermal/thermal_zone0/temp");
    if (!f.is_open()) return "cpu: --C";
    int millideg = 0;
    f >> millideg;
    char buf[16];
    snprintf(buf, sizeof(buf), "cpu: %dC", millideg / 1000);
    return buf;
}

// Auto-scaling gauge with 2-second debounce.
// Scale 0: ±3 seconds   (green arc)
// Scale 1: ±10 seconds  (yellow arc)
// Scale 2: ±5 minutes   (red arc)
static int64_t gauge_now_ms() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

static void updateGaugeScale(AppData* data) {
    double abs_sec = std::abs(data->aheadBehindSeconds);
    int desired;

    if (abs_sec <= 3.0) desired = 0;
    else if (abs_sec <= 10.0) desired = 1;
    else desired = 2;

    if (desired == data->gaugeScale) return;

    // Cooldown: don't change again within 2 seconds of the last change
    int64_t now = gauge_now_ms();
    if (now - data->gaugeScaleChangeTime < 2000) return;

    data->gaugeScale = desired;
    data->gaugeScaleChangeTime = now;
}

struct GaugeScaleInfo {
    double max_seconds;
    int major_count;       // number of major divisions on each side
    int minor_per_major;   // minor ticks between each major
    double arc_r, arc_g, arc_b;  // arc colour
};

static GaugeScaleInfo getGaugeScaleInfo(int scale) {
    switch (scale) {
    case 0:  return { 3.0,    3,  5, 0.0, 0.7, 0.0 };   // green
    case 2:  return { 300.0,  5,  6, 0.8, 0.1, 0.1 };   // red
    default: return { 10.0,   5,  5, 0.85, 0.65, 0.0 };  // yellow
    }
}

// Format the digital readout based on scale
// Red (±5min): ±hhh:mm:ss   Yellow/Green (±10s/±3s): ±ss.s
static void formatGaugeDigital(char* buf, size_t bufsize, double seconds, int scale) {
    double abs_sec = std::abs(seconds);
    const char* sign = seconds < 0 ? "-" : "+";

    if (scale == 2) {
        int total_sec = static_cast<int>(abs_sec + 0.5);
        int h = total_sec / 3600;
        int m = (total_sec % 3600) / 60;
        int s = total_sec % 60;
        if (h > 0)
            snprintf(buf, bufsize, "%s%d:%02d:%02d", sign, h, m, s);
        else
            snprintf(buf, bufsize, "%s%02d:%02d", sign, m, s);
    } else {
        snprintf(buf, bufsize, "%s%.1f", sign, abs_sec);
    }
}

// Draw the rally gauge (GaugePilot RallyMaster style) with auto-scaling
static gboolean on_gauge_draw(GtkWidget* widget, cairo_t* cr, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);

    GtkAllocation alloc;
    gtk_widget_get_allocation(widget, &alloc);
    double width = alloc.width;
    double height = alloc.height;
    double radius = (std::min(width / 2, height) - 25) * 0.8;
    double centerX = width - radius - 20;
    double centerY = (height + radius) / 2;

    updateGaugeScale(data);
    GaugeScaleInfo si = getGaugeScaleInfo(data->gaugeScale);
    double max_val = si.max_seconds;

    // Background
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
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

    // Coloured graduated arc (green/yellow/red depending on scale)
    int arc_segments = 40;
    for (int i = 0; i <= arc_segments; i++) {
        double frac = -1.0 + (2.0 * i) / arc_segments;
        double angle = M_PI + M_PI/2 + frac * (M_PI / 2);
        double next_frac = -1.0 + (2.0 * (i + 1)) / arc_segments;
        double next_angle = M_PI + M_PI/2 + next_frac * (M_PI / 2);

        double intensity = 0.3 + 0.7 * std::abs(frac);
        cairo_set_source_rgb(cr, si.arc_r * intensity, si.arc_g * intensity, si.arc_b * intensity);
        cairo_set_line_width(cr, 12);
        cairo_arc(cr, centerX, centerY, radius, angle, next_angle);
        cairo_stroke(cr);
    }

    // Determine label values based on scale
    // Scale 0 (±3s):   majors at 1,2,3 -- labels "1","2","3" (sec)
    // Scale 1 (±10s):  majors at 2,4,6,8,10 -- labels "2","4","6","8","10" (sec)
    // Scale 2 (±5min): majors at 1,2,3,4,5 -- labels "1","2","3","4","5" (min)
    bool show_minutes = (data->gaugeScale == 2);
    double label_divisor = show_minutes ? 60.0 : 1.0;

    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 13);

    // Major ticks
    double major_step_sec = max_val / si.major_count;
    for (int i = -si.major_count; i <= si.major_count; i++) {
        double val_sec = i * major_step_sec;
        double frac = val_sec / max_val;
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

        double label_val = std::abs(val_sec) / label_divisor;
        char label[16];
        if (label_val == static_cast<int>(label_val))
            snprintf(label, sizeof(label), "%d", static_cast<int>(label_val));
        else
            snprintf(label, sizeof(label), "%.1f", label_val);

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
    int total_minor = si.major_count * si.minor_per_major;
    for (int i = -total_minor; i <= total_minor; i++) {
        if (i % si.minor_per_major == 0) continue;
        double frac = (double)i / total_minor;
        double angle = M_PI + M_PI/2 + frac * (M_PI / 2);

        double x1 = centerX + (radius - 10) * cos(angle);
        double y1 = centerY + (radius - 10) * sin(angle);
        double x2 = centerX + (radius + 4) * cos(angle);
        double y2 = centerY + (radius + 4) * sin(angle);

        cairo_move_to(cr, x1, y1);
        cairo_line_to(cr, x2, y2);
        cairo_stroke(cr);
    }

    // Unit labels
    const char* unit = show_minutes ? "min" : "sec";
    cairo_set_font_size(cr, 11);
    cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);

    char left_label[16], right_label[16];
    snprintf(left_label, sizeof(left_label), "- %s", unit);
    snprintf(right_label, sizeof(right_label), "%s +", unit);

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

    // Digital display box - white outlined
    double box_width = 130;
    double box_height = 36;
    double box_x = centerX - box_width / 2;
    double box_y = centerY + 18;

    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_rectangle(cr, box_x, box_y, box_width, box_height);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_set_line_width(cr, 2.0);
    cairo_rectangle(cr, box_x, box_y, box_width, box_height);
    cairo_stroke(cr);

    // Digital readout - large white text
    double seconds = data->aheadBehindSeconds;
    char digital[24];
    formatGaugeDigital(digital, sizeof(digital), seconds, data->gaugeScale);

    cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 22);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);

    cairo_text_extents_t dext;
    cairo_text_extents(cr, digital, &dext);
    cairo_move_to(cr, centerX - dext.width / 2, box_y + box_height / 2 + dext.height / 2 - 2);
    cairo_show_text(cr, digital);

    // Needle (narrow white triangle)
    double needle_seconds = seconds;
    if (needle_seconds > max_val) needle_seconds = max_val;
    if (needle_seconds < -max_val) needle_seconds = -max_val;

    double needle_angle = M_PI + M_PI/2 + (needle_seconds / max_val) * (M_PI / 2);
    double needle_length = radius - 25;
    double half_width = 12.0;

    double tip_x = centerX + needle_length * cos(needle_angle);
    double tip_y = centerY + needle_length * sin(needle_angle);
    double perp_x = -sin(needle_angle);
    double perp_y = cos(needle_angle);

    // White filled triangle
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_move_to(cr, tip_x, tip_y);
    cairo_line_to(cr, centerX + half_width * perp_x, centerY + half_width * perp_y);
    cairo_line_to(cr, centerX - half_width * perp_x, centerY - half_width * perp_y);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Needle hub
    cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
    cairo_arc(cr, centerX, centerY, 10, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
    cairo_set_line_width(cr, 1);
    cairo_arc(cr, centerX, centerY, 10, 0, 2 * M_PI);
    cairo_stroke(cr);

    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_arc(cr, centerX, centerY, 4, 0, 2 * M_PI);
    cairo_fill(cr);

    return FALSE;
}

void updateDriverDisplay(AppData* data) {
    auto current_poll = data->poller->getMostRecent();
    auto tenth_poll = data->poller->get10th();
    auto current_time_ms = getRallyTime_ms(*data->state);
    
    // Current speed (from rolling average, then EMA-smoothed for display)
    double current_speed = calculateCurrentSpeed(*data->state, current_poll, tenth_poll);
    if (current_speed < 0) {
        data->smoothedSpeed = -1.0;
    } else if (data->smoothedSpeed < 0) {
        data->smoothedSpeed = current_speed;
    } else {
        constexpr double alpha = 0.02;
        data->smoothedSpeed = alpha * current_speed + (1.0 - alpha) * data->smoothedSpeed;
    }
    std::stringstream ss;
    if (data->smoothedSpeed < 0) {
        ss << "--.-";
    } else {
        ss << std::fixed << std::setprecision(1) << data->smoothedSpeed;
    }
    gtk_label_set_text(data->currentSpeedLabel, ss.str().c_str());
    
    // Trip average speed
    int64_t trip_count_diff = calculateDistanceCounts(*data->state, 
        current_poll.cntr1, current_poll.cntr2,
        data->state->trip_start_cntr1, data->state->trip_start_cntr2);
    double trip_speed = calculateAverageSpeed(*data->state,
        data->state->trip_start_time_ms, current_time_ms, trip_count_diff);
    ss.str("");
    ss << std::fixed << std::setprecision(1) << trip_speed;
    gtk_label_set_text(data->tripSpeedLabel, ss.str().c_str());
    
    // Total average speed
    int64_t total_count_diff = calculateDistanceCounts(*data->state,
        current_poll.cntr1, current_poll.cntr2,
        data->state->total_start_cntr1, data->state->total_start_cntr2);
    double total_speed = calculateAverageSpeed(*data->state,
        data->state->total_start_time_ms, current_time_ms, total_count_diff);
    ss.str("");
    ss << std::fixed << std::setprecision(1) << total_speed;
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
        ss << std::fixed << std::setprecision(1) << target_kph;
        gtk_label_set_text(data->targetSpeedLabel, ss.str().c_str());
        gtk_label_set_text(data->gaugeTargetLabel, ss.str().c_str());
        
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
            
            double speed_diff;
            if (adjusted_time_s > 0.1) {
                double needed_kph = (500.0 / adjusted_time_s) * 3.6;
                speed_diff = needed_kph - target_kph_raw;
            } else {
                // Deficit too large to recover in 500m - max arrows in needed direction
                speed_diff = (seconds < 0) ? 999.0 : -999.0;
            }
            
            if (data->state->units) {
                speed_diff = speed_diff * 0.621371;
            }
            
            double abs_diff = std::abs(speed_diff);
            int num_arrows = 0;
            if (abs_diff >= 10.0) {
                num_arrows = 3;
            } else if (abs_diff >= 3.0) {
                num_arrows = 2;
            } else if (abs_diff > 0) {
                num_arrows = 1;
            }
            
            if (num_arrows > 0) {
                ss.str("");
                const char* color = (speed_diff > 0) ? "#00CC00" : "#EE0000";
                const char* arrow = (speed_diff > 0) ? "↑" : "↓";
                ss << "<span foreground=\"" << color << "\">";
                for (int i = 0; i < num_arrows; i++) ss << arrow;
                ss << "</span>";
                gtk_label_set_markup(data->speedAdjustArrowsLabel, ss.str().c_str());
            } else {
                gtk_label_set_text(data->speedAdjustArrowsLabel, "");
            }
            
            // Tone cadence: silent if beyond ±30s, otherwise match arrow brackets.
            // Behind (speed_diff > 0, speed up): C6=1046.50, F6=1396.91, A6=1760.00
            // Ahead  (speed_diff < 0, slow down): F6=1396.91, D6=1174.66, C6=1046.50
            if (data->toneGen) {
                if (abs_seconds > 30.0 || num_arrows == 0) {
                    data->toneGen->setCadence(0, 0);
                } else {
                    bool behind = (speed_diff > 0);
                    double freq;
                    int tone, silence;
                    if (num_arrows >= 3) {
                        freq = behind ? 1760.00 : 1046.50;
                        tone = 700; silence = 300;
                    } else if (num_arrows == 2) {
                        freq = behind ? 1396.91 : 1174.66;
                        tone = 500; silence = 200;
                    } else {
                        freq = behind ? 1046.50 : 1396.91;
                        tone = 100; silence = 100;
                    }
                    data->toneGen->setCadence(tone, silence, freq);
                }
            }
        } else {
            gtk_label_set_text(data->speedAdjustArrowsLabel, "");
            if (data->toneGen) data->toneGen->setCadence(0, 0);
        }
        
        // Redraw gauge
        gtk_widget_queue_draw(data->rallyGaugeDrawingArea);
    } else {
        gtk_label_set_text(data->targetSpeedLabel, "--.-");
        gtk_label_set_text(data->gaugeTargetLabel, "--.-");
        gtk_label_set_text(data->aheadBehindLabel, "--:--.--");
        gtk_label_set_text(data->speedAdjustArrowsLabel, "");
        if (data->toneGen) data->toneGen->setCadence(0, 0);
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
    
    // FPS counter and CPU temp (refreshed once per second)
    data->updateCount++;
    if (current_time_ms - data->lastUpdateCountTime_ms >= 1000) {
        ss.str("");
        ss << "fps: " << data->updateCount;
        gtk_label_set_text(data->updatesPerSecLabel, ss.str().c_str());
        gtk_label_set_text(data->cpuTempLabel, readCpuTemp().c_str());
        data->updateCount = 0;
        data->lastUpdateCountTime_ms = current_time_ms;
    }
}

// Apply CSS styling for large fonts
static void applyDriverCSS(GtkWidget* G_GNUC_UNUSED widget) {
    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        "window, .background { background-color: #000000; }"
        "label { color: #FFFFFF; font-weight: bold; }"
        "button { background-color: #333333; color: #FFFFFF; font-weight: bold; }"
        ".speed-header { font-size: 28px; }"
        ".speed-value { font-size: 64px; font-family: monospace; }"
        ".speed-value-xl { font-size: 80px; font-family: monospace; }"
        ".speed-value-target { font-size: 45px; font-family: monospace; }"
        ".target-info { font-size: 22px; }"
        ".ahead-behind { font-size: 28px; font-family: monospace; }"
        ".next-info { font-size: 18px; }"
        ".footer-info { font-size: 14px; }"
        ".speed-arrows { font-size: 28px; }",
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
    gtk_container_set_border_width(GTK_CONTAINER(mainBox), 2);
    gtk_container_add(GTK_CONTAINER(window), mainBox);
    
    // Main content: left side speeds + footer, right side gauge (full height)
    GtkWidget* contentBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(mainBox), contentBox, TRUE, TRUE, 0);
    
    // Left side: vertical box holding speed columns on top, footer on bottom (~40% width)
    GtkWidget* speedsBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(speedsBox, 580, -1);
    gtk_box_pack_start(GTK_BOX(contentBox), speedsBox, FALSE, TRUE, 0);
    
    // Two-column speed display row
    GtkWidget* speedColsBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(speedsBox), speedColsBox, TRUE, TRUE, 0);
    
    // Left column: Current speed + arrows, Target speed
    GtkWidget* leftCol = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(speedColsBox), leftCol, TRUE, TRUE, 0);
    
    // Current header with arrows beside it
    GtkWidget* currentHeaderRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_halign(currentHeaderRow, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(leftCol), currentHeaderRow, FALSE, FALSE, 0);
    
    GtkWidget* currentHeader = gtk_label_new("Current");
    gtk_style_context_add_class(gtk_widget_get_style_context(currentHeader), "speed-header");
    gtk_box_pack_start(GTK_BOX(currentHeaderRow), currentHeader, FALSE, FALSE, 0);
    
    data->speedAdjustArrowsLabel = GTK_LABEL(gtk_label_new(""));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->speedAdjustArrowsLabel)), "speed-arrows");
    gtk_widget_set_valign(GTK_WIDGET(data->speedAdjustArrowsLabel), GTK_ALIGN_END);
    gtk_label_set_width_chars(data->speedAdjustArrowsLabel, 3);
    gtk_box_pack_start(GTK_BOX(currentHeaderRow), GTK_WIDGET(data->speedAdjustArrowsLabel), FALSE, FALSE, 0);
    
    // Current speed value (fixed width, right-aligned so decimal stays put)
    data->currentSpeedLabel = GTK_LABEL(gtk_label_new("--.-"));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->currentSpeedLabel)), "speed-value-xl");
    gtk_label_set_width_chars(data->currentSpeedLabel, 6);
    gtk_label_set_xalign(data->currentSpeedLabel, 1.0);
    gtk_widget_set_halign(GTK_WIDGET(data->currentSpeedLabel), GTK_ALIGN_CENTER);
    gtk_widget_set_valign(GTK_WIDGET(data->currentSpeedLabel), GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(leftCol), GTK_WIDGET(data->currentSpeedLabel), TRUE, TRUE, 0);
    
    // Target speed
    GtkWidget* targetHeader = gtk_label_new("Target");
    gtk_style_context_add_class(gtk_widget_get_style_context(targetHeader), "speed-header");
    gtk_widget_set_halign(targetHeader, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(leftCol), targetHeader, FALSE, FALSE, 0);
    
    data->targetSpeedLabel = GTK_LABEL(gtk_label_new("--.-"));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->targetSpeedLabel)), "speed-value-target");
    gtk_label_set_width_chars(data->targetSpeedLabel, 6);
    gtk_label_set_xalign(data->targetSpeedLabel, 1.0);
    gtk_widget_set_halign(GTK_WIDGET(data->targetSpeedLabel), GTK_ALIGN_CENTER);
    gtk_widget_set_valign(GTK_WIDGET(data->targetSpeedLabel), GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(leftCol), GTK_WIDGET(data->targetSpeedLabel), TRUE, TRUE, 0);
    
    // Right column: Total + Trip (vertically aligned)
    GtkWidget* rightCol = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(speedColsBox), rightCol, TRUE, TRUE, 0);
    
    GtkWidget* totalHeader = gtk_label_new("Total");
    gtk_style_context_add_class(gtk_widget_get_style_context(totalHeader), "speed-header");
    gtk_widget_set_halign(totalHeader, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(rightCol), totalHeader, FALSE, FALSE, 0);
    
    data->totalSpeedLabel = GTK_LABEL(gtk_label_new("--.-"));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->totalSpeedLabel)), "speed-value");
    gtk_label_set_width_chars(data->totalSpeedLabel, 6);
    gtk_label_set_xalign(data->totalSpeedLabel, 1.0);
    gtk_widget_set_halign(GTK_WIDGET(data->totalSpeedLabel), GTK_ALIGN_CENTER);
    gtk_widget_set_valign(GTK_WIDGET(data->totalSpeedLabel), GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(rightCol), GTK_WIDGET(data->totalSpeedLabel), TRUE, TRUE, 0);
    
    GtkWidget* tripHeader = gtk_label_new("Trip");
    gtk_style_context_add_class(gtk_widget_get_style_context(tripHeader), "speed-header");
    gtk_widget_set_halign(tripHeader, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(rightCol), tripHeader, FALSE, FALSE, 0);
    
    data->tripSpeedLabel = GTK_LABEL(gtk_label_new("--.-"));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->tripSpeedLabel)), "speed-value");
    gtk_label_set_width_chars(data->tripSpeedLabel, 6);
    gtk_label_set_xalign(data->tripSpeedLabel, 1.0);
    gtk_widget_set_halign(GTK_WIDGET(data->tripSpeedLabel), GTK_ALIGN_CENTER);
    gtk_widget_set_valign(GTK_WIDGET(data->tripSpeedLabel), GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(rightCol), GTK_WIDGET(data->tripSpeedLabel), TRUE, TRUE, 0);
    
    // Footer row at bottom of LEFT side only (under speeds)
    GtkWidget* footerBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_end(GTK_BOX(speedsBox), footerBox, FALSE, FALSE, 5);
    
    data->updatesPerSecLabel = GTK_LABEL(gtk_label_new("fps: 0"));
    data->cpuTempLabel = GTK_LABEL(gtk_label_new(readCpuTemp().c_str()));
    data->nextSegLabel = GTK_LABEL(gtk_label_new(""));
    
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->updatesPerSecLabel)), "footer-info");
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->cpuTempLabel)), "footer-info");
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->nextSegLabel)), "next-info");
    
    gtk_box_pack_start(GTK_BOX(footerBox), GTK_WIDGET(data->updatesPerSecLabel), FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(footerBox), GTK_WIDGET(data->cpuTempLabel), FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(footerBox), GTK_WIDGET(data->nextSegLabel), TRUE, TRUE, 0);
    
    // Right side: Rally gauge fills full height with unit toggle overlaid at top right
    GtkWidget* gaugeOverlay = gtk_overlay_new();
    gtk_widget_set_vexpand(gaugeOverlay, TRUE);
    gtk_widget_set_hexpand(gaugeOverlay, TRUE);
    gtk_box_pack_end(GTK_BOX(contentBox), gaugeOverlay, TRUE, TRUE, 0);
    
    data->rallyGaugeDrawingArea = gtk_drawing_area_new();
    gtk_widget_set_hexpand(data->rallyGaugeDrawingArea, TRUE);
    gtk_widget_set_vexpand(data->rallyGaugeDrawingArea, TRUE);
    g_signal_connect(data->rallyGaugeDrawingArea, "draw", G_CALLBACK(on_gauge_draw), data);
    gtk_container_add(GTK_CONTAINER(gaugeOverlay), data->rallyGaugeDrawingArea);
    
    data->unitToggleBtn = GTK_BUTTON(gtk_button_new_with_label(data->state->units ? "MPH" : "KPH"));
    gtk_widget_set_halign(GTK_WIDGET(data->unitToggleBtn), GTK_ALIGN_END);
    gtk_widget_set_valign(GTK_WIDGET(data->unitToggleBtn), GTK_ALIGN_START);
    gtk_overlay_add_overlay(GTK_OVERLAY(gaugeOverlay), GTK_WIDGET(data->unitToggleBtn));
    
    // Units label (hidden, kept for update logic compatibility)
    data->unitsLabel = GTK_LABEL(gtk_label_new(data->state->units ? "(MPH)" : "(KPH)"));
    
    // Hidden labels still needed by update logic
    data->aheadBehindLabel = GTK_LABEL(gtk_label_new(""));
    data->gaugeTargetLabel = GTK_LABEL(gtk_label_new(""));
    
    return window;
}
