#ifndef UI_CONTROL_H
#define UI_CONTROL_H

#include <gtk/gtk.h>
#include "rally_types.h"

// Creates the "Driving Controls (SIM)" window: START/STOP and 25-50 km/h
// (5 km/h step) speed buttons that drive AppData::simCounter1/2 in real
// time. Only meaningful when RALLY_SIM_I2C=1 (simCounter1/2 non-null).
GtkWidget* createControlWindow(AppData* data);

// Refreshes the status label and highlights the active speed button.
// Safe to call even if createControlWindow() was never called
// (controlStatusLabel is nullptr, so it's a no-op).
void updateControlDisplay(AppData* data);

#endif // UI_CONTROL_H
