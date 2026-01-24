#ifndef CALLBACKS_H
#define CALLBACKS_H

#include <gtk/gtk.h>
#include "rally_types.h"

gboolean on_window_delete(GtkWidget* widget, GdkEvent* event, gpointer user_data);
void on_unit_toggle(GtkWidget* widget, gpointer user_data);
void on_total_reset(GtkWidget* widget, gpointer user_data);
void on_trip_reset(GtkWidget* widget, gpointer user_data);
void on_stage_go(GtkWidget* widget, gpointer user_data);
void on_next_segment(GtkWidget* widget, gpointer user_data);
void on_show_segments(GtkWidget* widget, gpointer user_data);
void on_show_calibration(GtkWidget* widget, gpointer user_data);
void on_show_twinmaster(GtkWidget* widget, gpointer user_data);
void on_show_datetime(GtkWidget* widget, gpointer user_data);
void on_add_segment(GtkWidget* widget, gpointer user_data);
void on_delete_segment(GtkWidget* widget, gpointer user_data);
void on_save_calibration(GtkWidget* widget, gpointer user_data);
void on_save_datetime(GtkWidget* widget, gpointer user_data);
void refreshSegmentList(AppData* data);
void updateCalibrationDisplay(AppData* data);
void updateDateTimeDisplay(AppData* data);
gboolean update_display(gpointer user_data);
void on_keypad_digit(GtkWidget* widget, gpointer user_data);
void on_keypad_clear(GtkWidget* widget, gpointer user_data);
void on_keypad_backspace(GtkWidget* widget, gpointer user_data);
gboolean on_entry_focus(GtkWidget* widget, GdkEvent* event, gpointer user_data);
void on_segment_entry_changed(GtkWidget* widget, gpointer user_data);
void on_segment_auto_toggled(GtkWidget* widget, gpointer user_data);
GtkWidget* createNumericKeypad(AppData* data);

#endif // CALLBACKS_H
