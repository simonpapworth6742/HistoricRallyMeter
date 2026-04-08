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

    // Set beep cadence and pitch.
    // tone_ms/silence_ms: on/off durations. Both 0 = silent.
    // freq_hz: tone frequency in Hz (e.g. 523.25 for C5).
    void setCadence(int tone_ms, int silence_ms, double freq_hz = 0.0);

    // Play a short one-shot beep for button feedback
    void playBeep();

private:
    void threadFunc();

    std::thread worker_;
    std::atomic<bool> running_{false};
    std::atomic<bool> beep_requested_{false};

    std::mutex mu_;
    int tone_ms_ = 0;
    int silence_ms_ = 0;
    double freq_hz_ = 0.0;
};

#endif // TONE_GENERATOR_H
