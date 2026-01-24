#include <iostream>
#include <exception>
#include <gtk/gtk.h>
#include "i2c_counter.h"
#include "rally_state.h"
#include "config_file.h"
#include "counter_poller.h"
#include "rally_types.h"
#include "ui_driver.h"
#include "ui_copilot.h"
#include "callbacks.h"
#include "calculations.h"

int main(int argc, char* argv[]) {
    try {
        const int I2C_BUS = 1;
        const int CNTR_1_ADDRESS = 0x70;
        const int CNTR_2_ADDRESS = 0x71;
        const uint8_t REGISTER = 0x07;
        
        gtk_init(&argc, &argv);
        
        // Load state
        RallyState state;
        ConfigFile::load(state);
        
        // Initialize counters from current values if not set
        I2CCounter counter1(I2C_BUS, CNTR_1_ADDRESS);
        I2CCounter counter2(I2C_BUS, CNTR_2_ADDRESS);
        
        if (state.total_start_cntr1 == 0 && state.total_start_cntr2 == 0) {
            state.total_start_cntr1 = counter1.readRegister(REGISTER);
            state.total_start_cntr2 = counter2.readRegister(REGISTER);
        }
        if (state.trip_start_cntr1 == 0 && state.trip_start_cntr2 == 0) {
            state.trip_start_cntr1 = counter1.readRegister(REGISTER);
            state.trip_start_cntr2 = counter2.readRegister(REGISTER);
        }
        if (state.segment_start_cntr1 == 0 && state.segment_start_cntr2 == 0) {
            state.segment_start_cntr1 = counter1.readRegister(REGISTER);
            state.segment_start_cntr2 = counter2.readRegister(REGISTER);
        }
        
        // Create application data
        AppData app_data;
        app_data.counter1 = &counter1;
        app_data.counter2 = &counter2;
        app_data.register_addr = REGISTER;
        app_data.state = &state;
        app_data.poller = new CounterPoller();
        app_data.lastUpdateCountTime_ms = getRallyTime_ms(state);
        
        // Create windows
        app_data.driverWindow = createDriverWindow(&app_data);
        app_data.copilotWindow = createCopilotWindow(&app_data);
        
        // Connect window delete handlers
        g_signal_connect(app_data.driverWindow, "delete-event", G_CALLBACK(on_window_delete), NULL);
        g_signal_connect(app_data.copilotWindow, "delete-event", G_CALLBACK(on_window_delete), NULL);
        
        // Connect button handlers
        g_signal_connect(app_data.unitToggleBtn, "clicked", G_CALLBACK(on_unit_toggle), &app_data);
        
        // Find and connect copilot buttons (they're created in createCopilotWindow)
        // We'll need to store them in AppData or find them - for now, simplified
        
        // Show windows
        gtk_widget_show_all(app_data.driverWindow);
        gtk_widget_show_all(app_data.copilotWindow);
        
        // Set up timer (10ms = 100Hz)
        g_timeout_add(10, update_display, &app_data);
        
        gtk_main();
        
        delete app_data.poller;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
