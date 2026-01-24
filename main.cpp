#include <iostream>
#include <exception>
#include <cstring>
#include <string>
#include <cctype>
#include <fstream>
#include <filesystem>
#include <vector>
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

// Check if string contains substring (case-insensitive)
static bool containsIgnoreCase(const std::string& haystack, const std::string& needle) {
    if (haystack.empty() || needle.empty()) return false;
    std::string h = haystack;
    std::string n = needle;
    for (auto& c : h) c = std::tolower(c);
    for (auto& c : n) c = std::tolower(c);
    return h.find(n) != std::string::npos;
}

// Structure to hold connector info from DRM
struct DrmConnector {
    std::string name;       // e.g., "DSI-2", "HDMI-A-1"
    std::string status;     // "connected" or "disconnected"
    int width = 0;
    int height = 0;
};

// Read connector information from /sys/class/drm/
static std::vector<DrmConnector> getDrmConnectors() {
    std::vector<DrmConnector> connectors;
    const std::string drm_path = "/sys/class/drm/";
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(drm_path)) {
            std::string name = entry.path().filename().string();
            
            // Skip card entries, look for connector entries like "card1-DSI-2"
            if (name.find("card") == 0 && name.find('-') != std::string::npos) {
                DrmConnector conn;
                
                // Extract connector name (e.g., "DSI-2" from "card1-DSI-2")
                size_t dash_pos = name.find('-');
                if (dash_pos != std::string::npos) {
                    conn.name = name.substr(dash_pos + 1);
                }
                
                // Read status
                std::ifstream status_file(entry.path() / "status");
                if (status_file) {
                    std::getline(status_file, conn.status);
                }
                
                // Read modes to get resolution
                std::ifstream modes_file(entry.path() / "modes");
                if (modes_file) {
                    std::string mode;
                    if (std::getline(modes_file, mode)) {
                        // Parse mode like "1280x400" or "3840x2160"
                        size_t x_pos = mode.find('x');
                        if (x_pos != std::string::npos) {
                            try {
                                conn.width = std::stoi(mode.substr(0, x_pos));
                                conn.height = std::stoi(mode.substr(x_pos + 1));
                            } catch (...) {}
                        }
                    }
                }
                
                if (!conn.name.empty()) {
                    connectors.push_back(conn);
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Warning: Could not read DRM connectors: " << e.what() << std::endl;
    }
    
    return connectors;
}

// Find DSI-2 connector info
static DrmConnector* findDsi2Connector(std::vector<DrmConnector>& connectors) {
    for (auto& conn : connectors) {
        if (containsIgnoreCase(conn.name, "dsi-2") || containsIgnoreCase(conn.name, "dsi2")) {
            return &conn;
        }
    }
    // Fallback: look for any DSI connector
    for (auto& conn : connectors) {
        if (containsIgnoreCase(conn.name, "dsi")) {
            return &conn;
        }
    }
    return nullptr;
}

// Find DSI-2 display for co-pilot window
static GdkMonitor* findDsi2Monitor(GdkDisplay* display) {
    int n_monitors = gdk_display_get_n_monitors(display);
    
    // Get DRM connector info from system
    auto connectors = getDrmConnectors();
    
    std::cout << "System connectors:" << std::endl;
    for (const auto& conn : connectors) {
        std::cout << "  " << conn.name << ": " << conn.status 
                  << " (" << conn.width << "x" << conn.height << ")" << std::endl;
    }
    
    // Find DSI-2 connector
    DrmConnector* dsi2 = findDsi2Connector(connectors);
    if (dsi2 && dsi2->status == "connected") {
        std::cout << "DSI-2 connector found: " << dsi2->width << "x" << dsi2->height << std::endl;
    } else {
        std::cout << "DSI-2 connector not found or not connected" << std::endl;
    }
    
    std::cout << "GDK monitors (" << n_monitors << "):" << std::endl;
    
    GdkMonitor* dsi2_monitor = nullptr;
    
    for (int i = 0; i < n_monitors; i++) {
        GdkMonitor* monitor = gdk_display_get_monitor(display, i);
        GdkRectangle geometry;
        gdk_monitor_get_geometry(monitor, &geometry);
        
        std::cout << "  Monitor " << i << ": "
                  << geometry.width << "x" << geometry.height
                  << " at (" << geometry.x << "," << geometry.y << ")"
                  << std::endl;
        
        // Match monitor to DSI-2 connector by resolution
        if (dsi2 && dsi2->status == "connected" && !dsi2_monitor) {
            // Check if resolution matches (considering rotation)
            bool matches_dsi2 = (geometry.width == dsi2->width && geometry.height == dsi2->height) ||
                                (geometry.width == dsi2->height && geometry.height == dsi2->width);
            if (matches_dsi2) {
                std::cout << "  -> Matched to DSI-2" << std::endl;
                dsi2_monitor = monitor;
            }
        }
        
        // Fallback: check by 1280x400 resolution if no DSI-2 connector info
        if (!dsi2 && !dsi2_monitor) {
            bool is_1280x400 = (geometry.width == 1280 && geometry.height == 400) ||
                               (geometry.width == 400 && geometry.height == 1280);
            if (is_1280x400) {
                std::cout << "  -> Matched by 1280x400 resolution" << std::endl;
                dsi2_monitor = monitor;
            }
        }
    }
    
    if (dsi2_monitor) {
        std::cout << "Co-pilot display: DSI-2 screen selected" << std::endl;
        return dsi2_monitor;
    }
    
    std::cout << "Co-pilot display: DSI-2 not found, will use window mode" << std::endl;
    return nullptr;
}

// Save driver window position callback
static gboolean on_driver_configure(GtkWidget* widget, GdkEventConfigure* event, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    
    // Get size from event or window
    if (event) {
        data->state->driver_window_width = event->width;
        data->state->driver_window_height = event->height;
    } else {
        gint width, height;
        gtk_window_get_size(GTK_WINDOW(widget), &width, &height);
        data->state->driver_window_width = width;
        data->state->driver_window_height = height;
    }
    
    // Get monitor and calculate position
    GdkDisplay* display = gdk_display_get_default();
    GdkWindow* gdk_window = gtk_widget_get_window(widget);
    if (gdk_window) {
        GdkMonitor* monitor = gdk_display_get_monitor_at_window(display, gdk_window);
        int n_monitors = gdk_display_get_n_monitors(display);
        for (int i = 0; i < n_monitors; i++) {
            if (gdk_display_get_monitor(display, i) == monitor) {
                data->state->driver_window_monitor = i;
                
                // Try multiple methods to get window position
                gint root_x = 0, root_y = 0;
                
                // Method 1: gdk_window_get_root_origin (X11)
                gdk_window_get_root_origin(gdk_window, &root_x, &root_y);
                
                // Method 2: If that returns 0,0 try gdk_window_get_origin
                if (root_x == 0 && root_y == 0) {
                    gdk_window_get_origin(gdk_window, &root_x, &root_y);
                }
                
                // Method 3: Use event x,y if available (may work on some Wayland compositors)
                if (root_x == 0 && root_y == 0 && event) {
                    root_x = event->x;
                    root_y = event->y;
                }
                
                // Store position relative to monitor origin
                GdkRectangle mon_geom;
                gdk_monitor_get_geometry(monitor, &mon_geom);
                data->state->driver_window_x = root_x - mon_geom.x;
                data->state->driver_window_y = root_y - mon_geom.y;
                
                break;
            }
        }
    }
    
    return FALSE;  // Continue processing
}

// Save state on window close
static gboolean on_window_close_save(GtkWidget* G_GNUC_UNUSED widget, GdkEvent* G_GNUC_UNUSED event, gpointer user_data) {
    AppData* data = static_cast<AppData*>(user_data);
    
    // Position/size should already be tracked via configure-event
    // Just save the current state to config file
    std::cout << "Saving window - relative position: (" << data->state->driver_window_x 
              << "," << data->state->driver_window_y << ") "
              << "size: " << data->state->driver_window_width << "x" << data->state->driver_window_height
              << " monitor: " << data->state->driver_window_monitor << std::endl;
    
    ConfigFile::save(*data->state);
    
    gtk_main_quit();
    return TRUE;
}

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
        
        // Get display info and find DSI-2 monitor
        GdkDisplay* display = gdk_display_get_default();
        GdkMonitor* dsi2_monitor = findDsi2Monitor(display);
        
        // Position co-pilot window on DSI-2
        if (dsi2_monitor) {
            // Find the monitor index for DSI-2
            int dsi2_index = 0;
            int n_monitors = gdk_display_get_n_monitors(display);
            for (int i = 0; i < n_monitors; i++) {
                if (gdk_display_get_monitor(display, i) == dsi2_monitor) {
                    dsi2_index = i;
                    break;
                }
            }
            
            // Position and fullscreen on DSI-2 monitor
            GdkRectangle geometry;
            gdk_monitor_get_geometry(dsi2_monitor, &geometry);
            std::cout << "Positioning co-pilot window on monitor " << dsi2_index 
                      << " at (" << geometry.x << "," << geometry.y << ")" << std::endl;
            gtk_window_move(GTK_WINDOW(app_data.copilotWindow), geometry.x, geometry.y);
            gtk_window_fullscreen_on_monitor(GTK_WINDOW(app_data.copilotWindow), 
                gdk_display_get_default_screen(display), dsi2_index);
        } else {
            // No DSI-2 display found - use 1280x400 window
            std::cout << "No DSI-2 found, opening co-pilot as 1280x400 window" << std::endl;
            gtk_window_set_default_size(GTK_WINDOW(app_data.copilotWindow), 1280, 400);
            gtk_window_resize(GTK_WINDOW(app_data.copilotWindow), 1280, 400);
        }
        
        // Position driver window (restore saved position and monitor)
        std::cout << "Restoring driver window: size " 
                  << state.driver_window_width << "x" << state.driver_window_height
                  << " on monitor " << state.driver_window_monitor << std::endl;
        
        // Set size
        gtk_window_set_default_size(GTK_WINDOW(app_data.driverWindow), 
                                    state.driver_window_width, state.driver_window_height);
        
        // On Wayland, gtk_window_move doesn't work. We need to position based on monitor.
        // Find the non-DSI2 monitor (driver's monitor) and position there
        int n_monitors = gdk_display_get_n_monitors(display);
        GdkMonitor* driver_monitor = nullptr;
        int driver_monitor_index = -1;
        
        // First, try to use the saved monitor
        if (state.driver_window_monitor >= 0 && state.driver_window_monitor < n_monitors) {
            GdkMonitor* saved_monitor = gdk_display_get_monitor(display, state.driver_window_monitor);
            if (saved_monitor && saved_monitor != dsi2_monitor) {
                driver_monitor = saved_monitor;
                driver_monitor_index = state.driver_window_monitor;
            }
        }
        
        // Fallback: find any monitor that isn't DSI-2
        if (!driver_monitor) {
            for (int i = 0; i < n_monitors; i++) {
                GdkMonitor* mon = gdk_display_get_monitor(display, i);
                if (mon != dsi2_monitor) {
                    driver_monitor = mon;
                    driver_monitor_index = i;
                    break;
                }
            }
        }
        
        if (driver_monitor) {
            GdkRectangle mon_geometry;
            gdk_monitor_get_geometry(driver_monitor, &mon_geometry);
            
            std::cout << "Driver monitor " << driver_monitor_index 
                      << " geometry: " << mon_geometry.width << "x" << mon_geometry.height
                      << " at (" << mon_geometry.x << "," << mon_geometry.y << ")" << std::endl;
            std::cout << "Saved relative position: (" << state.driver_window_x 
                      << "," << state.driver_window_y << ")" << std::endl;
            
            // Saved position is relative to monitor, convert to absolute
            int abs_x, abs_y;
            
            // On Wayland, (0,0) is usually invalid - it means we couldn't get real position
            // Also validate position is within reasonable bounds
            bool position_valid = !(state.driver_window_x == 0 && state.driver_window_y == 0) &&
                                  state.driver_window_x >= 0 && state.driver_window_y >= 0 &&
                                  state.driver_window_x < mon_geometry.width && 
                                  state.driver_window_y < mon_geometry.height;
            
            if (position_valid) {
                abs_x = mon_geometry.x + state.driver_window_x;
                abs_y = mon_geometry.y + state.driver_window_y;
                std::cout << "Using saved position" << std::endl;
            } else {
                // Center on monitor
                abs_x = mon_geometry.x + (mon_geometry.width - state.driver_window_width) / 2;
                abs_y = mon_geometry.y + (mon_geometry.height - state.driver_window_height) / 2;
                std::cout << "Centering window (saved position invalid)" << std::endl;
            }
            
            std::cout << "Positioning driver window at (" << abs_x << "," << abs_y << ")" << std::endl;
            
            // Position the window
            gtk_window_move(GTK_WINDOW(app_data.driverWindow), abs_x, abs_y);
        }
        
        // Connect window delete handlers (save state on close)
        g_signal_connect(app_data.driverWindow, "delete-event", 
                         G_CALLBACK(on_window_close_save), &app_data);
        g_signal_connect(app_data.copilotWindow, "delete-event", 
                         G_CALLBACK(on_window_close_save), &app_data);
        
        // Track driver window position changes
        g_signal_connect(app_data.driverWindow, "configure-event",
                         G_CALLBACK(on_driver_configure), &app_data);
        
        // Connect button handlers
        g_signal_connect(app_data.unitToggleBtn, "clicked", G_CALLBACK(on_unit_toggle), &app_data);
        
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
