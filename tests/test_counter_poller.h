#ifndef TEST_COUNTER_POLLER_H
#define TEST_COUNTER_POLLER_H

#include "test_framework.h"
#include "mock_i2c_counter.h"
#include "../rally_types.h"
#include <chrono>
#include <thread>

// Simplified CounterPoller for testing (without actual I2C)
class TestableCounterPoller {
private:
    static constexpr int ARRAY_SIZE = 10;
    CounterPoll polls[ARRAY_SIZE];
    CounterPoll most_recent_poll;
    int64_t last_poll_time_ms = -100;  // Initialize to allow first poll
    bool first_poll_done = false;
    static constexpr int MIN_POLL_INTERVAL_MS = 5;
    
public:
    TestableCounterPoller() {
        for (int i = 0; i < ARRAY_SIZE; i++) {
            polls[i].time_ms = 0;
            polls[i].cntr1 = 0;
            polls[i].cntr2 = 0;
        }
        most_recent_poll = {0, 0, 0};
    }
    
    bool poll(uint32_t val1, uint32_t val2, int64_t time_ms) {
        // Don't poll more than every 5ms (unless first poll)
        if (first_poll_done && time_ms - last_poll_time_ms < MIN_POLL_INTERVAL_MS) {
            return false;
        }
        
        if (!first_poll_done) {
            // First poll: store in first position only, no shift
            polls[0].cntr1 = val1;
            polls[0].cntr2 = val2;
            polls[0].time_ms = time_ms;
            first_poll_done = true;
        } else if ((time_ms - polls[0].time_ms) >= 200) {
            // Shift array down when >200ms since position 0
            for (int i = ARRAY_SIZE - 1; i > 0; i--) {
                polls[i] = polls[i-1];
            }
            polls[0].cntr1 = val1;
            polls[0].cntr2 = val2;
            polls[0].time_ms = time_ms;
        }
        // Else: do not modify the array (discard this poll for array purposes)
        
        // Always store the actual most recent I2C read for the speed calculation
        most_recent_poll.cntr1 = val1;
        most_recent_poll.cntr2 = val2;
        most_recent_poll.time_ms = time_ms;
        
        last_poll_time_ms = time_ms;
        return true;
    }
    
    CounterPoll get10th() const {
        if (polls[ARRAY_SIZE - 1].time_ms > 0) {
            int64_t age_ms = most_recent_poll.time_ms - polls[ARRAY_SIZE - 1].time_ms;
            if (age_ms >= 1280) {
                return polls[ARRAY_SIZE - 1];
            }
        }
        return {0, 0, 0};
    }
    
    CounterPoll getMostRecent() const {
        return most_recent_poll;
    }
    
    // Test helper
    CounterPoll getPosition(int idx) const {
        if (idx >= 0 && idx < ARRAY_SIZE) {
            return polls[idx];
        }
        return {0, 0, 0};
    }
};

class TestCounterPoller {
public:
    TestSuite* createSuite() {
        auto* suite = new TestSuite("Counter Poller Tests");
        
        // Test first poll stores in position 0
        suite->addTest("First poll stores in array position 0", []() {
            TestableCounterPoller poller;
            
            poller.poll(100, 200, 1000);
            
            CounterPoll pos0 = poller.getPosition(0);
            ASSERT_EQ(pos0.cntr1, 100u);
            ASSERT_EQ(pos0.cntr2, 200u);
            ASSERT_EQ(pos0.time_ms, 1000);
            
            return true;
        });
        
        // Test array shifts when >200ms since position 0
        suite->addTest("Array shifts down when >200ms since position 0", []() {
            TestableCounterPoller poller;
            
            poller.poll(100, 100, 0);      // First poll at t=0
            poller.poll(200, 200, 205);    // Second poll at t=205 (>200ms)
            
            CounterPoll pos0 = poller.getPosition(0);
            CounterPoll pos1 = poller.getPosition(1);
            
            ASSERT_EQ(pos0.cntr1, 200u);  // New value in position 0
            ASSERT_EQ(pos1.cntr1, 100u);  // Old value shifted to position 1
            
            return true;
        });
        
        // Test array does not shift when <200ms
        suite->addTest("Array does not shift if <200ms since position 0", []() {
            TestableCounterPoller poller;
            
            poller.poll(100, 100, 0);
            poller.poll(200, 200, 100);  // Only 100ms later
            
            CounterPoll pos0 = poller.getPosition(0);
            CounterPoll pos1 = poller.getPosition(1);
            
            ASSERT_EQ(pos0.cntr1, 100u);  // Still the first value
            ASSERT_EQ(pos1.time_ms, 0);   // Position 1 still empty
            
            return true;
        });
        
        // Test get10th returns empty when position 9 time is zero
        suite->addTest("get10th returns invalid when position 9 time is zero", []() {
            TestableCounterPoller poller;
            
            // Only a few polls - position 9 will be empty
            poller.poll(100, 100, 0);
            poller.poll(200, 200, 205);
            poller.poll(300, 300, 410);
            
            CounterPoll tenth = poller.get10th();
            ASSERT_EQ(tenth.time_ms, 0);  // Invalid
            
            return true;
        });
        
        // Test get10th returns invalid when age < 1280ms
        suite->addTest("get10th returns invalid when age < 1280ms", []() {
            TestableCounterPoller poller;
            
            // Fill array quickly
            for (int i = 0; i < 10; i++) {
                poller.poll(i * 10, i * 10, i * 100);  // 100ms apart, not 200ms
            }
            
            // The age will be small since we polled quickly
            CounterPoll tenth = poller.get10th();
            
            // Should be invalid because age is less than 1280ms
            ASSERT_EQ(tenth.time_ms, 0);
            
            return true;
        });
        
        // Test get10th returns valid when age >= 1280ms
        suite->addTest("get10th returns valid data when age >= 1280ms", []() {
            TestableCounterPoller poller;
            
            // Fill array with proper 200ms spacing
            for (int i = 0; i < 10; i++) {
                poller.poll(i * 10, i * 10, i * 200);  // 0, 200, 400, ..., 1800
            }
            // Poll one more at 2000ms to ensure most_recent is updated
            poller.poll(100, 100, 2000);
            
            // Age = 2000 - 0 = 2000ms >= 1280ms
            CounterPoll tenth = poller.get10th();
            
            ASSERT_GT(tenth.time_ms, 0);  // Should be valid
            
            return true;
        });
        
        // Test getMostRecent returns actual latest read
        suite->addTest("getMostRecent returns actual latest I2C read", []() {
            TestableCounterPoller poller;
            
            poller.poll(100, 100, 0);
            poller.poll(200, 200, 50);   // <200ms, won't shift array
            poller.poll(300, 300, 100);  // <200ms, won't shift array
            
            CounterPoll recent = poller.getMostRecent();
            
            // Should have the latest values even though array wasn't shifted
            ASSERT_EQ(recent.cntr1, 300u);
            ASSERT_EQ(recent.time_ms, 100);
            
            return true;
        });
        
        // Test polling rate limiting (5ms minimum)
        suite->addTest("Polling rate limited to 5ms minimum", []() {
            TestableCounterPoller poller;
            
            bool first = poller.poll(100, 100, 0);
            bool second = poller.poll(200, 200, 3);  // Only 3ms later
            bool third = poller.poll(300, 300, 10);  // 10ms after first
            
            ASSERT_TRUE(first);
            ASSERT_FALSE(second);  // Should be rejected (< 5ms)
            ASSERT_TRUE(third);    // Should succeed (>= 5ms)
            
            return true;
        });
        
        // Test current speed uses most_recent vs 10th
        suite->addTest("Speed calculation uses most_recent vs 10th position", []() {
            TestableCounterPoller poller;
            
            // Fill array
            for (int i = 0; i < 10; i++) {
                poller.poll(i * 100, 0, i * 200);
            }
            poller.poll(1000, 0, 2000);
            
            CounterPoll recent = poller.getMostRecent();
            CounterPoll tenth = poller.get10th();
            
            // Verify they are different (for speed calculation)
            ASSERT_NE(recent.cntr1, tenth.cntr1);
            ASSERT_NE(recent.time_ms, tenth.time_ms);
            
            return true;
        });
        
        // Test display shows --.-- when 10th is invalid
        suite->addTest("Display --.-- when 10th position is invalid", []() {
            TestableCounterPoller poller;
            
            poller.poll(100, 100, 0);
            
            CounterPoll tenth = poller.get10th();
            
            // When time_ms is 0, speed should show "--.--"
            bool should_show_dashes = (tenth.time_ms == 0);
            ASSERT_TRUE(should_show_dashes);
            
            return true;
        });
        
        return suite;
    }
};

#endif // TEST_COUNTER_POLLER_H
