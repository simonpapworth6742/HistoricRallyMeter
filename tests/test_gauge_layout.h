#ifndef TEST_GAUGE_LAYOUT_H
#define TEST_GAUGE_LAYOUT_H

#include "test_framework.h"
#include "../calculations.h"
#include <cmath>
#include <string>

// Geometry tests for the compact (800x480) driver gauge layout. The same
// arithmetic is mirrored in tools/layout-preview/DriverWindowTextLayout.html;
// if that mockup is edited, these expectations move with it.
class TestGaugeLayout {
public:
    TestSuite* createSuite() {
        auto* suite = new TestSuite("Compact Gauge Layout");

        suite->addTest("radius and centre match the 800x480 box", []() {
            CompactGaugeLayout L = computeCompactGaugeLayout(800, 480);
            // radius = min(width/2 - 25, height - 95) = min(375, 385) = 375
            return std::abs(L.radius - 375.0) < 0.001
                && std::abs(L.centerX - 400.0) < 0.001
                && std::abs(L.centerY - 405.0) < 0.001;
        });

        suite->addTest("fscale never exceeds 1.0", []() {
            // A gauge larger than the reference must not scale fonts UP, or
            // the values overflow the panel on a wide display.
            CompactGaugeLayout big = computeCompactGaugeLayout(1600, 900);
            CompactGaugeLayout small = computeCompactGaugeLayout(400, 240);
            return std::abs(big.fscale - 1.0) < 0.001 && small.fscale < 1.0;
        });

        suite->addTest("distance anchor sits left of centre, scaled by radius", []() {
            // Not mirrored off centerX like rightAnchor -- distanceAnchor is
            // where the "Distance (metres)" caption's own text ends up
            // right-aligning to, an absolute offset from the arc's own
            // radius, not a centre-relative gap.
            CompactGaugeLayout L = computeCompactGaugeLayout(800, 480);
            // radius = 375, so distanceAnchor = 375 * 0.72 = 270.
            return std::abs(L.distanceAnchor - 270.0) < 0.001
                && L.distanceAnchor < L.centerX;
        });

        suite->addTest("Target sits above Total sits above Trip", []() {
            // Smaller baseline = higher on screen. Rows must be one rowGap
            // apart so a distance on the left lines up with its speed.
            CompactGaugeLayout L = computeCompactGaugeLayout(800, 480);
            return L.targetBaseline < L.totalBaseline
                && L.totalBaseline < L.tripBaseline
                && std::abs((L.totalBaseline - L.targetBaseline) - L.rowGap) < 0.001
                && std::abs((L.tripBaseline - L.totalBaseline) - L.rowGap) < 0.001;
        });

        suite->addTest("row gap is 48 at full scale", []() {
            CompactGaugeLayout L = computeCompactGaugeLayout(800, 480);
            return std::abs(L.rowGap - 48.0) < 0.001;
        });

        suite->addTest("Current sits above every other row", []() {
            CompactGaugeLayout L = computeCompactGaugeLayout(800, 480);
            return L.curBaseline < L.targetBaseline;
        });

        suite->addTest("Trip row clears the needle hub", []() {
            // The bottom row must not collide with the hub at centerY.
            CompactGaugeLayout L = computeCompactGaugeLayout(800, 480);
            return L.tripBaseline < L.centerY;
        });

        suite->addTest("caption clears the Trip row above it", []() {
            CompactGaugeLayout L = computeCompactGaugeLayout(800, 480);
            return L.captionBaseline > L.tripBaseline;
        });

        suite->addTest("distance caption follows the metre/kilometre switch", []() {
            // The caption carries the unit, so it must track the automatic
            // switch formatDistanceAutoUnit() makes above 999,999 m -- a
            // hard-coded "metres" would misread a km value by 1000x.
            return distanceColumnCaption("m") == "Distance (metres)"
                && distanceColumnCaption("km") == "Distance (kilometres)";
        });

        suite->addTest("distance caption defaults to metres on a null unit", []() {
            return distanceColumnCaption(nullptr) == "Distance (metres)";
        });

        suite->addTest("speed caption follows the KPH/MPH setting", []() {
            return speedColumnCaption(false) == "Average Speed (Kmh)"
                && speedColumnCaption(true) == "Average Speed (Mph)";
        });

        suite->addTest("error box covers the needle hub", []() {
            // The box is lifted so its top edge is above the hub; drawn after
            // the needle it then paints the hub out, which is the point.
            CompactGaugeLayout L = computeCompactGaugeLayout(800, 480);
            return L.boxY < L.centerY
                && (L.boxY + L.boxHeight) > L.centerY;
        });

        suite->addTest("error box is 50px tall", []() {
            CompactGaugeLayout L = computeCompactGaugeLayout(800, 480);
            return std::abs(L.boxHeight - 50.0) < 0.001;
        });

        suite->addTest("footer sits on the bottom edge of the error box", []() {
            CompactGaugeLayout L = computeCompactGaugeLayout(800, 480);
            return std::abs(L.footBaseline - (L.boxY + L.boxHeight)) < 0.001;
        });

        suite->addTest("footer text never shrinks below 11px", []() {
            // Below ~11px the fps/cpu readout is unreadable on the box's
            // 800x480 panel at arm's length.
            CompactGaugeLayout tiny = computeCompactGaugeLayout(320, 200);
            return tiny.footSize >= 11.0;
        });

        // Re-enabled here at the RB-DRV-02/RB-DRV-03 merge point: this test
        // was deferred on RB-DRV-03 (a sibling of RB-DRV-02, both cut from
        // RB-DRV-01) since CompactGaugeLayout::captionBaseline only existed
        // on RB-DRV-02 until both branches landed on box.
        suite->addTest("captions clear the error box", []() {
            // The captions were previously top-aligned with the box. Now
            // the box has moved up over the hub, they must not follow it
            // there.
            CompactGaugeLayout L = computeCompactGaugeLayout(800, 480);
            return L.captionBaseline > L.boxY;
        });

        // Needle geometry tests -- from RB-DRV-04/05, a separate lineage cut
        // from main directly (no CompactGaugeLayout on that side), merged
        // into this same suite/file at the RB-DRV land point.
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
