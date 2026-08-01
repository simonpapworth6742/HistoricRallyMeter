#ifndef TEST_GAUGE_LAYOUT_H
#define TEST_GAUGE_LAYOUT_H

#include "test_framework.h"
#include "../calculations.h"
#include <cmath>

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

        // TODO(RB-DRV-03/RB-DRV-02 merge): re-enable once this branch is
        // merged with rb-drv-02-gauge-column-captions, which adds
        // CompactGaugeLayout::captionBaseline. RB-DRV-03 was cut from
        // RB-DRV-01, a sibling of RB-DRV-02, so captionBaseline does not
        // exist here yet -- it will exist once both branches land on box.
        //
        // suite->addTest("captions clear the error box", []() {
        //     // The captions were previously top-aligned with the box. Now
        //     // the box has moved up over the hub, they must not follow it
        //     // there.
        //     CompactGaugeLayout L = computeCompactGaugeLayout(800, 480);
        //     return L.captionBaseline > L.boxY;
        // });

        return suite;
    }
};

#endif // TEST_GAUGE_LAYOUT_H
