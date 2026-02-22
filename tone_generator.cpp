#include "tone_generator.h"
#include <alsa/asoundlib.h>
#include <cmath>
#include <vector>
#include <iostream>

static constexpr unsigned SAMPLE_RATE = 44100;
static constexpr double   FREQ_HZ    = 800.0;
static constexpr double   AMPLITUDE  = 0.25;
static constexpr unsigned CHUNK_MS   = 20;
static constexpr unsigned CHUNK_FRAMES = SAMPLE_RATE * CHUNK_MS / 1000;

ToneGenerator::ToneGenerator() = default;

ToneGenerator::~ToneGenerator() {
    stop();
}

void ToneGenerator::start() {
    if (running_) return;
    running_ = true;
    worker_ = std::thread(&ToneGenerator::threadFunc, this);
}

void ToneGenerator::stop() {
    running_ = false;
    if (worker_.joinable()) worker_.join();
}

void ToneGenerator::setCadence(int tone_ms, int silence_ms) {
    std::lock_guard<std::mutex> lk(mu_);
    tone_ms_ = tone_ms;
    silence_ms_ = silence_ms;
}

void ToneGenerator::threadFunc() {
    snd_pcm_t* pcm = nullptr;
    int err = snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        std::cerr << "ToneGenerator: cannot open audio: " << snd_strerror(err) << std::endl;
        return;
    }

    snd_pcm_set_params(pcm,
        SND_PCM_FORMAT_S16_LE,
        SND_PCM_ACCESS_RW_INTERLEAVED,
        1,
        SAMPLE_RATE,
        1,
        50000);

    // Pre-generate one chunk of sine wave and one chunk of silence
    std::vector<int16_t> tone_buf(CHUNK_FRAMES);
    std::vector<int16_t> silence_buf(CHUNK_FRAMES, 0);

    double phase = 0.0;
    const double phase_inc = 2.0 * M_PI * FREQ_HZ / SAMPLE_RATE;
    for (unsigned i = 0; i < CHUNK_FRAMES; i++) {
        tone_buf[i] = static_cast<int16_t>(AMPLITUDE * 32767.0 * std::sin(phase));
        phase += phase_inc;
        if (phase >= 2.0 * M_PI) phase -= 2.0 * M_PI;
    }

    int cycle_ms = 0;
    bool in_tone = true;

    while (running_) {
        int cur_tone, cur_silence;
        {
            std::lock_guard<std::mutex> lk(mu_);
            cur_tone = tone_ms_;
            cur_silence = silence_ms_;
        }

        if (cur_tone <= 0) {
            // Silent - write silence to keep ALSA happy, sleep to avoid busy loop
            snd_pcm_writei(pcm, silence_buf.data(), CHUNK_FRAMES);
            cycle_ms = 0;
            in_tone = true;
            continue;
        }

        int threshold = in_tone ? cur_tone : cur_silence;
        if (cycle_ms >= threshold) {
            in_tone = !in_tone;
            cycle_ms = 0;
        }

        const int16_t* buf = in_tone ? tone_buf.data() : silence_buf.data();
        snd_pcm_sframes_t frames = snd_pcm_writei(pcm, buf, CHUNK_FRAMES);
        if (frames < 0) {
            snd_pcm_recover(pcm, static_cast<int>(frames), 1);
        }

        cycle_ms += CHUNK_MS;
    }

    snd_pcm_drain(pcm);
    snd_pcm_close(pcm);
}
