CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++20 -O2 $(shell pkg-config --cflags gtk+-3.0)
LDFLAGS = $(shell pkg-config --libs gtk+-3.0)
TARGET = HistoricRallyMeter

SOURCES = main.cpp i2c_counter.cpp rally_state.cpp config_file.cpp counter_poller.cpp \
          calculations.cpp ui_driver.cpp ui_copilot.cpp callbacks.cpp
OBJECTS = $(SOURCES:.cpp=.o)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: clean
