#ifndef TEST_GAUGE_LAYOUT_H
#define TEST_GAUGE_LAYOUT_H

#include <cmath>
#include "test_framework.h"
#include "../calculations.h"

class TestGaugeLayout {
public:
    TestSuite* createSuite() {
        auto* suite = new TestSuite("Gauge Layout Tests");

        suite->addTest("needle points straight up at zero error", []() {
            NeedleGeometry n = computeNeedleGeometry(0.0, 10.0, 375.0);
            // Cairo's y grows downward, so straight up is 3*PI/2.
            return std::abs(n.angle - (3.0 * M_PI / 2.0)) < 1e-9;
        });

        suite->addTest("needle reaches the scale ends at full deflection", []() {
            NeedleGeometry pos = computeNeedleGeometry(10.0, 10.0, 375.0);
            NeedleGeometry neg = computeNeedleGeometry(-10.0, 10.0, 375.0);
            return std::abs(pos.angle - 2.0 * M_PI) < 1e-9
                && std::abs(neg.angle - M_PI) < 1e-9;
        });

        suite->addTest("needle pegs rather than sweeping past the scale", []() {
            // Beyond full deflection the needle stops; the digital readout
            // carries the real magnitude.
            NeedleGeometry way_over = computeNeedleGeometry(600.0, 10.0, 375.0);
            NeedleGeometry at_max = computeNeedleGeometry(10.0, 10.0, 375.0);
            return std::abs(way_over.angle - at_max.angle) < 1e-9;
        });

        suite->addTest("needle tip stops short of the tick ring", []() {
            NeedleGeometry n = computeNeedleGeometry(0.0, 10.0, 375.0);
            return n.length < 375.0 - 10.0 && n.length > 0.0;
        });

        suite->addTest("needle is a constant-width bar matching the ticks", []() {
            NeedleGeometry a = computeNeedleGeometry(0.0, 10.0, 375.0);
            NeedleGeometry b = computeNeedleGeometry(7.5, 10.0, 375.0);
            return std::abs(a.halfWidth - 3.0) < 1e-9
                && std::abs(a.halfWidth - b.halfWidth) < 1e-9;
        });

        return suite;
    }
};

#endif // TEST_GAUGE_LAYOUT_H
