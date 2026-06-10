#ifndef UI_DRIVER_H
#define UI_DRIVER_H

#include <gtk/gtk.h>
#include "rally_types.h"

void updateDriverDisplay(AppData* data);
GtkWidget* createDriverWindow(AppData* data);

// Rally gauge draw callback (also used for the gauge embedded in the
// TwinMaster right panel in single-display mode)
gboolean on_gauge_draw(GtkWidget* widget, cairo_t* cr, gpointer user_data);

#endif // UI_DRIVER_H
