#ifndef UI_DRIVER_H
#define UI_DRIVER_H

#include <gtk/gtk.h>
#include "rally_types.h"

void updateDriverDisplay(AppData* data);
GtkWidget* createDriverWindow(AppData* data);

#endif // UI_DRIVER_H
