#ifndef TONE_GENERATOR_H
#define TONE_GENERATOR_H

#include <atomic>
#include <thread>
#include <mutex>

class ToneGenerator {
public:
    ToneGenerator();
    ~ToneGenerator();

    void start();
    void stop();

    // Set beep cadence: tone_ms on, silence_ms off. Both 0 = silent.
    void setCadence(int tone_ms, int silence_ms);

private:
    void threadFunc();

    std::thread worker_;
    std::atomic<bool> running_{false};

    std::mutex mu_;
    int tone_ms_ = 0;
    int silence_ms_ = 0;
};

#endif // TONE_GENERATOR_H
