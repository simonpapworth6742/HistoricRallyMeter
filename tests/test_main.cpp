// Test Runner for Historic Rally Meter
// All tests are in separate header files from the functional implementation

#include "test_framework.h"
#include "test_config_file.h"
#include "test_distance.h"
#include "test_calibration.h"
#include "test_speed.h"
#include "test_counter_poller.h"
#include "test_segments.h"
#include "test_ahead_behind.h"
#include "test_rally_clock.h"
#include "test_edge_cases.h"

int main() {
    TestRunner runner;
    
    // Create test suites
    TestConfigFile configTests;
    TestDistance distanceTests;
    TestCalibration calibrationTests;
    TestSpeed speedTests;
    TestCounterPoller pollerTests;
    TestSegments segmentTests;
    TestAheadBehind aheadBehindTests;
    TestRallyClock clockTests;
    TestEdgeCases edgeCaseTests;
    
    // Add all test suites
    runner.addSuite(configTests.createSuite());
    runner.addSuite(distanceTests.createSuite());
    runner.addSuite(calibrationTests.createSuite());
    runner.addSuite(speedTests.createSuite());
    runner.addSuite(pollerTests.createSuite());
    runner.addSuite(segmentTests.createSuite());
    runner.addSuite(aheadBehindTests.createSuite());
    runner.addSuite(clockTests.createSuite());
    runner.addSuite(edgeCaseTests.createSuite());
    
    // Run all tests
    int failures = runner.runAll();
    
    return (failures > 0) ? 1 : 0;
}
