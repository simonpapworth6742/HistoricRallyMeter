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
        int64_t remaining_counts = current_seg.distance_counts - seg_count_diff;
        long remaining_m = countsToCentimeters(remaining_counts, data->state->calibration) / 100;
        
        if (current_speed > 0 && current_speed != -1) {
            double speed_m_per_s = current_speed;
            if (data->state->units) {
                speed_m_per_s = speed_m_per_s * 1.60934;  // MPH to km/h, then to m/s
            } else {
                speed_m_per_s = speed_m_per_s / 3.6;  // km/h to m/s
            }
            int64_t eta_seconds = static_cast<int64_t>(remaining_m / speed_m_per_s);
            if (eta_seconds < 0) {
                ss.str("");
                ss << "Over by " << formatDuration(-eta_seconds);
                gtk_label_set_text(data->nextSegLabel, ss.str().c_str());
            } else {
                ss.str("");
                ss << "speed " << std::fixed << std::setprecision(2) << current_speed 
                   << " in " << remaining_m << " m after " << formatDuration(eta_seconds * 1000);
                gtk_label_set_text(data->nextSegLabel, ss.str().c_str());
            }
        } else {
            ss.str("");
            ss << "speed --.-- in " << remaining_m << " m after --:--:--";
            gtk_label_set_text(data->nextSegLabel, ss.str().c_str());
        }
    } else {
        gtk_label_set_text(data->nextSegLabel, "");
    }
    
    // Updates per second
    data->updateCount++;
    if (current_time_ms - data->lastUpdateCountTime_ms >= 1000) {
        ss.str("");
        ss << "updates per second: " << data->updateCount;
        gtk_label_set_text(data->updatesPerSecLabel, ss.str().c_str());
        data->updateCount = 0;
        data->lastUpdateCountTime_ms = current_time_ms;
    }
}

GtkWidget* createDriverWindow(AppData* data) {
    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Driver Display");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 400);
    
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 20);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    
    // Header with units - matching layout of speed values below
    GtkWidget* headerBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(vbox), headerBox, FALSE, FALSE, 0);
    
    GtkWidget* currentHeader = gtk_label_new("Current");
    GtkWidget* tripHeader = gtk_label_new("Trip");
    GtkWidget* segHeader = gtk_label_new("Seg.");
    GtkWidget* totalHeaderBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget* totalHeader = gtk_label_new("Total");
    data->unitsLabel = GTK_LABEL(gtk_label_new(data->state->units ? "MPH" : "KPH"));
    
    // Center align all header labels to match speed values
    gtk_widget_set_halign(currentHeader, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(tripHeader, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(segHeader, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(totalHeaderBox, GTK_ALIGN_CENTER);
    
    // Create "Total (units)" label
    gtk_box_pack_start(GTK_BOX(totalHeaderBox), totalHeader, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(totalHeaderBox), gtk_label_new(" ("), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(totalHeaderBox), GTK_WIDGET(data->unitsLabel), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(totalHeaderBox), gtk_label_new(")"), FALSE, FALSE, 0);
    
    // Pack headers with same spacing as speed values (TRUE = expand equally)
    gtk_box_pack_start(GTK_BOX(headerBox), currentHeader, TRUE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(headerBox), tripHeader, TRUE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(headerBox), segHeader, TRUE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(headerBox), totalHeaderBox, TRUE, FALSE, 0);
    
    // Speed values - matching header layout
    GtkWidget* speedsBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(vbox), speedsBox, FALSE, FALSE, 0);
    
    data->currentSpeedLabel = GTK_LABEL(gtk_label_new("--.--"));
    data->tripSpeedLabel = GTK_LABEL(gtk_label_new("--.--"));
    data->segSpeedLabel = GTK_LABEL(gtk_label_new("--.--"));
    data->totalSpeedLabel = GTK_LABEL(gtk_label_new("--.--"));
    
    // Center align speed labels to match headers
    gtk_widget_set_halign(GTK_WIDGET(data->currentSpeedLabel), GTK_ALIGN_CENTER);
    gtk_widget_set_halign(GTK_WIDGET(data->tripSpeedLabel), GTK_ALIGN_CENTER);
    gtk_widget_set_halign(GTK_WIDGET(data->segSpeedLabel), GTK_ALIGN_CENTER);
    gtk_widget_set_halign(GTK_WIDGET(data->totalSpeedLabel), GTK_ALIGN_CENTER);
    
    gtk_box_pack_start(GTK_BOX(speedsBox), GTK_WIDGET(data->currentSpeedLabel), TRUE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(speedsBox), GTK_WIDGET(data->tripSpeedLabel), TRUE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(speedsBox), GTK_WIDGET(data->segSpeedLabel), TRUE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(speedsBox), GTK_WIDGET(data->totalSpeedLabel), TRUE, FALSE, 0);
    
    // Target and ahead/behind
    GtkWidget* targetBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(vbox), targetBox, FALSE, FALSE, 0);
    
    GtkWidget* targetLabel = gtk_label_new("target");
    data->targetSpeedLabel = GTK_LABEL(gtk_label_new("--.--"));
    data->aheadBehindLabel = GTK_LABEL(gtk_label_new("+- --:--:-- seconds"));
    
    gtk_box_pack_start(GTK_BOX(targetBox), targetLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(targetBox), GTK_WIDGET(data->targetSpeedLabel), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(targetBox), GTK_WIDGET(data->aheadBehindLabel), FALSE, FALSE, 0);
    
    // Next segment
    data->nextSegLabel = GTK_LABEL(gtk_label_new(""));
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(data->nextSegLabel), FALSE, FALSE, 0);
    
    // Updates per second and unit toggle
    GtkWidget* footerBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(vbox), footerBox, FALSE, FALSE, 0);
    
    data->updatesPerSecLabel = GTK_LABEL(gtk_label_new("updates per second: 0"));
    data->unitToggleBtn = GTK_BUTTON(gtk_button_new_with_label(data->state->units ? "MPH" : "KPH"));
    
    gtk_box_pack_start(GTK_BOX(footerBox), GTK_WIDGET(data->updatesPerSecLabel), TRUE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(footerBox), GTK_WIDGET(data->unitToggleBtn), FALSE, FALSE, 0);
    
    return window;
}
