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

        suite->addTest("effective max stays at 3 within the inner zone", []() {
            // Matches pristine's green ±3s scale exactly while deflecting.
            return std::abs(gaugeEffectiveMaxSeconds(1.5) - 3.0) < 1e-9
                && std::abs(gaugeEffectiveMaxSeconds(-2.9) - 3.0) < 1e-9;
        });

        suite->addTest("effective max tracks the reading beyond 3, capped at 30", []() {
            // This is what makes the needle sit pinned at the edge past 3s
            // with no separate clamp: seconds/effectiveMax is always exactly
            // +-1 here, by construction.
            return std::abs(gaugeEffectiveMaxSeconds(15.0) - 15.0) < 1e-9
                && std::abs(gaugeEffectiveMaxSeconds(-15.0) - 15.0) < 1e-9
                && std::abs(gaugeEffectiveMaxSeconds(45.0) - 30.0) < 1e-9
                && std::abs(gaugeEffectiveMaxSeconds(-100.0) - 30.0) < 1e-9;
        });

        suite->addTest("zone follows the 10s and 30s boundaries, sign-independent", []() {
            return gaugeZone(5.0) == 0 && gaugeZone(9.9) == 0
                && gaugeZone(10.0) == 1 && gaugeZone(29.9) == 1
                && gaugeZone(30.0) == 2 && gaugeZone(-30.0) == 2;
        });

        suite->addTest("arc colour matches pristine's per-scale values", []() {
            GaugeArcColor green = gaugeArcColor(0);
            GaugeArcColor amber = gaugeArcColor(1);
            GaugeArcColor red   = gaugeArcColor(2);
            return std::abs(green.g - 0.7) < 1e-9 && green.r < 1e-9
                && std::abs(amber.r - 0.85) < 1e-9 && std::abs(amber.g - 0.65) < 1e-9
                && std::abs(red.r - 0.8) < 1e-9 && std::abs(red.g - 0.1) < 1e-9;
        });

        suite->addTest("tick label is signed, blank at zero", []() {
            // The centre is marked by the triangle above the arc; a "0"
            // there would sit under the needle at rest.
            return gaugeTickLabel(0).empty()
                && gaugeTickLabel(-7) == "-7"
                && gaugeTickLabel(12) == "12";
        });

        suite->addTest("tick labels are visible only in the green zone", []() {
            return gaugeTickLabelsVisible(5.0)
                && !gaugeTickLabelsVisible(10.0)
                && !gaugeTickLabelsVisible(30.0)
                && !gaugeTickLabelsVisible(-15.0);
        });

        return suite;
    }
};

#endif // TEST_GAUGE_LAYOUT_H
