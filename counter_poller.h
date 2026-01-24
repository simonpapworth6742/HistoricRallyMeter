#ifndef COUNTER_POLLER_H
#define COUNTER_POLLER_H

#include <cstdint>
#include "rally_types.h"

class I2CCounter;

class CounterPoller {
private:
    static constexpr int ARRAY_SIZE = 10;
    CounterPoll polls[ARRAY_SIZE];   // position 0 is only for array shifting, never for speed
    CounterPoll most_recent_poll;    // actual most recent I2C read, used for speed calc
    int current_index = 0;
    int64_t last_poll_time_ms = 0;
    static constexpr int MIN_POLL_INTERVAL_MS = 5;
    
public:
    CounterPoller();
    
    bool poll(I2CCounter* cntr1, I2CCounter* cntr2, uint8_t reg);
    
    CounterPoll get10th() const;
    CounterPoll getMostRecent() const;  // most recent I2C read; use this for speed, never polls[0]
};

#endif // COUNTER_POLLER_H
