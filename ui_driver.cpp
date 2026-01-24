#include "ui_driver.h"
#include "calculations.h"
#include "rally_types.h"
#include "rally_state.h"
#include "counter_poller.h"
#include <iomanip>
#include <sstream>
#include <cmath>

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
    
    // Segment average speed
    if (data->state->segment_current_number >= 0) {
        int64_t seg_count_diff = calculateDistanceCounts(*data->state,
            current_poll.cntr1, current_poll.cntr2,
            data->state->segment_start_cntr1, data->state->segment_start_cntr2);
        double seg_speed = calculateAverageSpeed(*data->state,
            data->state->segment_start_time_ms, current_time_ms, seg_count_diff);
        ss.str("");
        ss << std::fixed << std::setprecision(2) << seg_speed;
        gtk_label_set_text(data->segSpeedLabel, ss.str().c_str());
    } else {
        gtk_label_set_text(data->segSpeedLabel, "--.--");
    }
    
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
        
        // Ahead/behind
        int64_t seg_count_diff = calculateDistanceCounts(*data->state,
            current_poll.cntr1, current_poll.cntr2,
            data->state->segment_start_cntr1, data->state->segment_start_cntr2);
        double seconds = calculateAheadBehind(*data->state, current_time_ms,
            data->state->segment_start_time_ms, seg.target_speed_counts_per_hour, seg_count_diff);
        ss.str("");
        if (seconds >= 0) {
            ss << "+";
        }
        int total_sec = static_cast<int>(std::abs(seconds));
        int hours = total_sec / 3600;
        int mins = (total_sec % 3600) / 60;
        int secs = total_sec % 60;
        ss << std::setfill('0') << std::setw(2) << hours << ":"
           << std::setw(2) << mins << ":" << std::setw(2) << secs;
        gtk_label_set_text(data->aheadBehindLabel, ss.str().c_str());
    } else {
        gtk_label_set_text(data->targetSpeedLabel, "--.--");
        gtk_label_set_text(data->aheadBehindLabel, "--:--:--");
    }
    
    // Next segment info
    if (data->state->segment_current_number >= 0 && 
        data->state->segment_current_number < static_cast<long>(data->state->segments.size()) - 1) {
        const Segment& current_seg = data->state->segments[data->state->segment_current_number];
        int64_t seg_count_diff = calculateDistanceCounts(*data->state,
            current_poll.cntr1, current_poll.cntr2,
            data->state->segment_start_cntr1, data->state->segment_start_cntr2);
        
        // High precision: remaining distance in meters
        double remaining_counts = current_seg.distance_counts - static_cast<double>(seg_count_diff);
        double remaining_m = countsToMeters(static_cast<int64_t>(remaining_counts), data->state->calibration);
        
        // Get next segment target speed
        const Segment& next_seg = data->state->segments[data->state->segment_current_number + 1];
        double next_target = countsPerHourToKPH(next_seg.target_speed_counts_per_hour, data->state->calibration);
        if (data->state->units) {
            next_target = next_target * 0.621371;
        }
        
        if (current_speed > 0 && current_speed != -1) {
            // High precision ETA calculation
            double speed_m_per_s = current_speed;
            if (data->state->units) {
                speed_m_per_s = speed_m_per_s * 1.60934;  // MPH to km/h
            }
            speed_m_per_s = speed_m_per_s / 3.6;  // km/h to m/s
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
        ".speed-header { font-size: 24px; font-weight: bold; }"
        ".speed-value { font-size: 48px; font-weight: bold; }"
        ".target-info { font-size: 20px; }"
        ".next-info { font-size: 18px; }"
        ".footer-info { font-size: 14px; }",
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
    
    GtkWidget* mainBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(mainBox), 10);
    gtk_container_add(GTK_CONTAINER(window), mainBox);
    
    // Row 1: Speed headers with units on far right
    GtkWidget* headerBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(mainBox), headerBox, FALSE, FALSE, 5);
    
    GtkWidget* currentHeader = gtk_label_new("Current");
    GtkWidget* tripHeader = gtk_label_new("Trip");
    GtkWidget* segHeader = gtk_label_new("Seg.");
    GtkWidget* totalHeader = gtk_label_new("Total");
    data->unitsLabel = GTK_LABEL(gtk_label_new(data->state->units ? "(MPH)" : "(KPH)"));
    
    gtk_style_context_add_class(gtk_widget_get_style_context(currentHeader), "speed-header");
    gtk_style_context_add_class(gtk_widget_get_style_context(tripHeader), "speed-header");
    gtk_style_context_add_class(gtk_widget_get_style_context(segHeader), "speed-header");
    gtk_style_context_add_class(gtk_widget_get_style_context(totalHeader), "speed-header");
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->unitsLabel)), "speed-header");
    
    gtk_widget_set_halign(currentHeader, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(tripHeader, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(segHeader, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(totalHeader, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(GTK_WIDGET(data->unitsLabel), GTK_ALIGN_END);
    
    gtk_box_pack_start(GTK_BOX(headerBox), currentHeader, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(headerBox), tripHeader, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(headerBox), segHeader, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(headerBox), totalHeader, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(headerBox), GTK_WIDGET(data->unitsLabel), FALSE, FALSE, 20);
    
    // Row 2: Large speed values spread across width
    GtkWidget* speedsBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(mainBox), speedsBox, TRUE, TRUE, 5);
    
    data->currentSpeedLabel = GTK_LABEL(gtk_label_new("--.--"));
    data->tripSpeedLabel = GTK_LABEL(gtk_label_new("--.--"));
    data->segSpeedLabel = GTK_LABEL(gtk_label_new("--.--"));
    data->totalSpeedLabel = GTK_LABEL(gtk_label_new("--.--"));
    
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->currentSpeedLabel)), "speed-value");
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->tripSpeedLabel)), "speed-value");
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->segSpeedLabel)), "speed-value");
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->totalSpeedLabel)), "speed-value");
    
    gtk_widget_set_halign(GTK_WIDGET(data->currentSpeedLabel), GTK_ALIGN_CENTER);
    gtk_widget_set_halign(GTK_WIDGET(data->tripSpeedLabel), GTK_ALIGN_CENTER);
    gtk_widget_set_halign(GTK_WIDGET(data->segSpeedLabel), GTK_ALIGN_CENTER);
    gtk_widget_set_halign(GTK_WIDGET(data->totalSpeedLabel), GTK_ALIGN_CENTER);
    
    gtk_box_pack_start(GTK_BOX(speedsBox), GTK_WIDGET(data->currentSpeedLabel), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(speedsBox), GTK_WIDGET(data->tripSpeedLabel), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(speedsBox), GTK_WIDGET(data->segSpeedLabel), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(speedsBox), GTK_WIDGET(data->totalSpeedLabel), TRUE, TRUE, 0);
    
    // Row 3: Target + ahead/behind on left, next segment info on right
    GtkWidget* middleBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_box_pack_start(GTK_BOX(mainBox), middleBox, FALSE, FALSE, 5);
    
    // Left side: target and ahead/behind
    GtkWidget* targetBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget* targetLabel = gtk_label_new("target");
    data->targetSpeedLabel = GTK_LABEL(gtk_label_new("--.--"));
    data->aheadBehindLabel = GTK_LABEL(gtk_label_new("--:--:--"));
    
    gtk_style_context_add_class(gtk_widget_get_style_context(targetLabel), "target-info");
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->targetSpeedLabel)), "target-info");
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->aheadBehindLabel)), "target-info");
    
    gtk_box_pack_start(GTK_BOX(targetBox), targetLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(targetBox), GTK_WIDGET(data->targetSpeedLabel), FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(targetBox), GTK_WIDGET(data->aheadBehindLabel), FALSE, FALSE, 0);
    
    // Right side: next segment info
    data->nextSegLabel = GTK_LABEL(gtk_label_new(""));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->nextSegLabel)), "next-info");
    gtk_widget_set_halign(GTK_WIDGET(data->nextSegLabel), GTK_ALIGN_END);
    
    gtk_box_pack_start(GTK_BOX(middleBox), targetBox, FALSE, FALSE, 20);
    gtk_box_pack_end(GTK_BOX(middleBox), GTK_WIDGET(data->nextSegLabel), TRUE, TRUE, 20);
    
    // Row 4: Updates counter on left, unit toggle button on far right
    GtkWidget* footerBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(mainBox), footerBox, FALSE, FALSE, 5);
    
    data->updatesPerSecLabel = GTK_LABEL(gtk_label_new("updates/sec: 0"));
    data->unitToggleBtn = GTK_BUTTON(gtk_button_new_with_label(data->state->units ? "MPH" : "KPH"));
    
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(data->updatesPerSecLabel)), "footer-info");
    
    gtk_box_pack_start(GTK_BOX(footerBox), GTK_WIDGET(data->updatesPerSecLabel), FALSE, FALSE, 20);
    gtk_box_pack_end(GTK_BOX(footerBox), GTK_WIDGET(data->unitToggleBtn), FALSE, FALSE, 20);
    
    return window;
}
