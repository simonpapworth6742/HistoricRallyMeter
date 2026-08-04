#ifndef TEST_SIMPLE_TONE_H
#define TEST_SIMPLE_TONE_H

#include "test_framework.h"
#include "../simple_tone.h"

// Tests for the alternative, time-error-only ahead/behind tone: silent
// within +/-0.2s, otherwise a constant tone that only re-latches when the
// error has moved more than 0.2s from the last latched value.
class TestSimpleTone {
public:
    TestSuite* createSuite() {
        auto* suite = new TestSuite("Simple Tone Mode");

        suite->addTest("silent within the +/-0.2s quiet band", []() {
            SimpleToneState st;
            SimpleToneResult r = updateSimpleTone(st, 0.15, 500.0, false);
            return r.active == false;
        });

        suite->addTest("silent before 250m into the stage even with a large error", []() {
            SimpleToneState st;
            SimpleToneResult r = updateSimpleTone(st, 5.0, 100.0, false);
            return r.active == false;
        });

        suite->addTest("silent once past the stage end", []() {
            SimpleToneState st;
            SimpleToneResult r = updateSimpleTone(st, 5.0, 1000.0, true);
            return r.active == false;
        });

        suite->addTest("silent beyond +/-30s", []() {
            SimpleToneState st;
            SimpleToneResult r = updateSimpleTone(st, 31.0, 1000.0, false);
            return r.active == false;
        });

        suite->addTest("behind schedule (negative seconds) sounds C6", []() {
            SimpleToneState st;
            SimpleToneResult r = updateSimpleTone(st, -1.0, 1000.0, false);
            return r.active == true && std::abs(r.freq_hz - 1046.50) < 0.01;
        });

        suite->addTest("ahead of schedule (positive seconds) sounds F6", []() {
            SimpleToneState st;
            SimpleToneResult r = updateSimpleTone(st, 1.0, 1000.0, false);
            return r.active == true && std::abs(r.freq_hz - 1396.91) < 0.01;
        });

        suite->addTest("a move of 0.2s or less does not re-latch, holding the old tone", []() {
            SimpleToneState st;
            // Latch onto -0.30 (just past the band, behind => C6).
            SimpleToneResult r1 = updateSimpleTone(st, -0.30, 1000.0, false);
            if (!(r1.active == true && std::abs(r1.freq_hz - 1046.50) < 0.01)) return false;
            // Raw is now -0.15 -- which is INSIDE the quiet band on its own
            // -- but only 0.15s away from the -0.30 latch, so the latch
            // must hold and the tone must still be active/behind.
            SimpleToneResult r2 = updateSimpleTone(st, -0.15, 1000.0, false);
            return r2.active == true && std::abs(r2.freq_hz - 1046.50) < 0.01;
        });

        suite->addTest("a move of more than 0.2s re-latches, even crossing to silence", []() {
            SimpleToneState st;
            SimpleToneResult r1 = updateSimpleTone(st, -0.30, 1000.0, false);
            if (!(r1.active == true)) return false;
            // 0.35s away from the -0.30 latch -- must re-latch onto +0.05,
            // which is inside the quiet band, so the tone goes silent.
            SimpleToneResult r2 = updateSimpleTone(st, 0.05, 1000.0, false);
            return r2.active == false;
        });

        suite->addTest("leaving the zone resets the latch for the next entry", []() {
            SimpleToneState st;
            SimpleToneResult r1 = updateSimpleTone(st, -5.0, 1000.0, false);
            if (!(r1.active == true)) return false;
            // Leave the zone (before 250m of a new stage, say).
            SimpleToneResult r2 = updateSimpleTone(st, -5.0, 100.0, false);
            if (!(r2.active == false)) return false;
            // Re-enter with a small error that must NOT trigger just
            // because a large value was latched before leaving.
            SimpleToneResult r3 = updateSimpleTone(st, 0.15, 1000.0, false);
            return r3.active == false;
        });

        return suite;
    }
};

#endif // TEST_SIMPLE_TONE_H
