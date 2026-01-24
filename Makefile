CXX = g++
CXXFLAGS_BASE = -Wall -Wextra -std=c++20 $(shell pkg-config --cflags gtk+-3.0)
CXXFLAGS = $(CXXFLAGS_BASE) -O2
CXXFLAGS_DEBUG = $(CXXFLAGS_BASE) -g -O0 -DDEBUG
CXXFLAGS_TEST = -Wall -Wextra -std=c++20 -O2 -I. -DRALLY_NO_GTK
LDFLAGS = $(shell pkg-config --libs gtk+-3.0)
TARGET = HistoricRallyMeter
TARGET_DEBUG = HistoricRallyMeter_debug
TEST_TARGET = run_tests

# Main application sources
SOURCES = main.cpp i2c_counter.cpp rally_state.cpp config_file.cpp counter_poller.cpp \
          calculations.cpp ui_driver.cpp ui_copilot.cpp callbacks.cpp
OBJECTS = $(SOURCES:.cpp=.o)
OBJECTS_DEBUG = $(SOURCES:.cpp=_debug.o)

# Test sources (only need calculations and rally_state for unit tests)
TEST_SOURCES = tests/test_main.cpp calculations.cpp rally_state.cpp
TEST_OBJECTS = tests/test_main.o calculations_test.o rally_state_test.o

# Default target
all: $(TARGET)

# Debug target
debug: $(TARGET_DEBUG)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

$(TARGET_DEBUG): $(OBJECTS_DEBUG)
	$(CXX) $(CXXFLAGS_DEBUG) -o $(TARGET_DEBUG) $(OBJECTS_DEBUG) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%_debug.o: %.cpp
	$(CXX) $(CXXFLAGS_DEBUG) -c $< -o $@

# Test targets
test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJECTS)
	$(CXX) $(CXXFLAGS_TEST) -o $(TEST_TARGET) $(TEST_OBJECTS)

tests/test_main.o: tests/test_main.cpp tests/*.h
	$(CXX) $(CXXFLAGS_TEST) -c tests/test_main.cpp -o tests/test_main.o

calculations_test.o: calculations.cpp calculations.h rally_state.h rally_types.h
	$(CXX) $(CXXFLAGS_TEST) -c calculations.cpp -o calculations_test.o

rally_state_test.o: rally_state.cpp rally_state.h rally_types.h
	$(CXX) $(CXXFLAGS_TEST) -c rally_state.cpp -o rally_state_test.o

clean:
	rm -f $(TARGET) $(TARGET_DEBUG) $(OBJECTS) $(OBJECTS_DEBUG) $(TEST_TARGET) $(TEST_OBJECTS)

.PHONY: all clean test debug
