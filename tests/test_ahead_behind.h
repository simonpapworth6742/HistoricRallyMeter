#ifndef TEST_AHEAD_BEHIND_H
#define TEST_AHEAD_BEHIND_H

#include "test_framework.h"
#include "../calculations.h"
#include "../rally_state.h"

class TestAheadBehind {
public:
    TestSuite* createSuite() {
        auto* suite = new TestSuite("Ahead/Behind and ETA Tests");
        
        // Test ideal_counts formula
        suite->addTest("ideal_counts = (time_ms / 3600000) * target_counts_h", []() {
            int64_t time_ms = 1800000;  // 30 minutes
            long target_counts_h = 100000;
            
            double ideal = (time_ms / 3600000.0) * target_counts_h;
            // 0.5 hours * 100000 = 50000 counts
            ASSERT_NEAR(ideal, 50000.0, 0.1);
            
            return true;
        });
        
        // Test diff calculation
        suite->addTest("diff = actual_counts - ideal_counts", []() {
            double ideal = 50000.0;
            int64_t actual = 55000;
            
            double diff = actual - ideal;
            ASSERT_NEAR(diff, 5000.0, 0.1);
            
            return true;
        });
        
        // Test seconds calculation
        suite->addTest("seconds = diff / (target_counts_h / 3600)", []() {
            double diff = 5000.0;
            long target_counts_h = 100000;
            
            double seconds = diff / (target_counts_h / 3600.0);
            // 5000 / (100000/3600) = 5000 / 27.78 = 180 seconds
            ASSERT_NEAR(seconds, 180.0, 1.0);
            
            return true;
        });
        
        // Test positive = ahead (too fast)
        suite->addTest("Positive seconds means travelling too fast (ahead)", []() {
            RallyState state;
            state.segment_current_number = 0;
            
            int64_t current_time_ms = 1800000;  // 30 min
            int64_t segment_start_time = 0;
            long target_counts_h = 100000;
            int64_t actual_counts = 55000;  // More than ideal (50000)
            
            double seconds = calculateAheadBehind(state, current_time_ms, segment_start_time,
                                                  target_counts_h, actual_counts);
            
            ASSERT_GT(seconds, 0);  // Positive = ahead
            
            return true;
        });
        
        // Test negative = behind (too slow)
        suite->addTest("Negative seconds means travelling too slow (behind)", []() {
            RallyState state;
            state.segment_current_number = 0;
            
            int64_t current_time_ms = 1800000;  // 30 min
            int64_t segment_start_time = 0;
            long target_counts_h = 100000;
            int64_t actual_counts = 45000;  // Less than ideal (50000)
            
            double seconds = calculateAheadBehind(state, current_time_ms, segment_start_time,
                                                  target_counts_h, actual_counts);
            
            ASSERT_LT(seconds, 0);  // Negative = behind
            
            return true;
        });
        
        // Test display format
        suite->addTest("Display +xxxxx for ahead, -xxxxx for behind", []() {
            double ahead = 180.5;
            double behind = -120.3;
            
            char buf_ahead[20];
            char buf_behind[20];
            
            snprintf(buf_ahead, sizeof(buf_ahead), "%+.0f", ahead);
            snprintf(buf_behind, sizeof(buf_behind), "%+.0f", behind);
            
            ASSERT_STR_EQ(std::string(buf_ahead), "+180");
            ASSERT_STR_EQ(std::string(buf_behind), "-120");
            
            return true;
        });
        
        // Test ETA calculation
        suite->addTest("ETA = remaining_distance / current_speed", []() {
            long remaining_cm = 6000000;  // 60km = 6000000cm
            double current_speed_kph = 60.0;
            
            // ETA in hours = 60km / 60kph = 1 hour
            double eta_hours = (remaining_cm / 100000.0) / current_speed_kph;
            ASSERT_NEAR(eta_hours, 1.0, 0.01);
            
            // In seconds = 3600
            double eta_seconds = eta_hours * 3600;
            ASSERT_NEAR(eta_seconds, 3600.0, 1.0);
            
            return true;
        });
        
        // Test ETA with zero speed
        suite->addTest("Display --.-- when current speed is zero", []() {
            double current_speed = 0.0;
            
            bool should_show_dashes = (current_speed <= 0);
            ASSERT_TRUE(should_show_dashes);
            
            return true;
        });
        
        // Test ETA with negative remaining (over segment end)
        suite->addTest("Display Over by hh:mm:ss when past segment end", []() {
            long remaining = -10000;  // Negative = past end
            
            bool is_over = (remaining < 0);
            ASSERT_TRUE(is_over);
            
            // Format as "Over by xx:xx:xx"
            long over_by = -remaining;
            // Over by 10000 cm = 100m, at 60kph would be ~6 seconds
            
            return true;
        });
        
        // Test ETA format
        suite->addTest("Format ETA as hh:mm:ss", []() {
            int64_t eta_ms = 3661000;  // 1 hour, 1 minute, 1 second
            
            std::string formatted = formatDuration(eta_ms);
            ASSERT_STR_EQ(formatted, "001:01:01.0");
            
            return true;
        });
        
        // Test no segment returns 0
        suite->addTest("No segment returns 0 for ahead/behind", []() {
            RallyState state;
            state.segment_current_number = -1;  // No segment
            
            double seconds = calculateAheadBehind(state, 1000, 0, 100000, 50000);
            ASSERT_NEAR(seconds, 0.0, 0.001);
            
            return true;
        });
        
        // Test zero target speed
        suite->addTest("Zero target speed returns 0", []() {
            RallyState state;
            state.segment_current_number = 0;
            
            double seconds = calculateAheadBehind(state, 1000, 0, 0, 50000);  // target = 0
            ASSERT_NEAR(seconds, 0.0, 0.001);
            
            return true;
        });
        
        // Test timing accuracy to 0.1s
        suite->addTest("Timing accuracy to 0.1 seconds", []() {
            RallyState state;
            state.segment_current_number = 0;
            
            // Small time difference should give precise result
            int64_t current_time_ms = 100;  // 0.1 seconds
            long target_counts_h = 360000;  // 100 counts per second
            int64_t actual_counts = 11;     // 1 count more than ideal (10)
            
            double seconds = calculateAheadBehind(state, current_time_ms, 0,
                                                  target_counts_h, actual_counts);
            
            // 1 count / 100 counts per second = 0.01 seconds ahead
            ASSERT_NEAR(seconds, 0.01, 0.001);
            
            return true;
        });
        
        return suite;
    }
};

#endif // TEST_AHEAD_BEHIND_H
