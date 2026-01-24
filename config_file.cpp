#include "config_file.h"
#include "rally_state.h"
#include <fstream>
#include <string>

void ConfigFile::load(RallyState& state) {
    std::ifstream file("rally_config.json");
    if (!file.is_open()) {
        return;  // Use defaults
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Very simple parser - just look for key:value pairs
        if (line.find("\"units\"") != std::string::npos) {
            if (line.find("true") != std::string::npos) state.units = true;
        } else if (line.find("\"calibration\"") != std::string::npos) {
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                state.calibration = std::stol(line.substr(pos + 1));
            }
        }
        // Add more parsing as needed...
    }
}

void ConfigFile::save(const RallyState& state) {
    std::ofstream file("rally_config.json");
    if (!file.is_open()) {
        return;
    }
    
    file << "{\n";
    file << "  \"units\": " << (state.units ? "true" : "false") << ",\n";
    file << "  \"calibration\": " << state.calibration << ",\n";
    file << "  \"counters\": " << (state.counters ? "true" : "false") << ",\n";
    file << "  \"total_start_cntr1\": " << state.total_start_cntr1 << ",\n";
    file << "  \"total_start_cntr2\": " << state.total_start_cntr2 << ",\n";
    file << "  \"total_start_time_ms\": " << state.total_start_time_ms << ",\n";
    file << "  \"trip_start_cntr1\": " << state.trip_start_cntr1 << ",\n";
    file << "  \"trip_start_cntr2\": " << state.trip_start_cntr2 << ",\n";
    file << "  \"trip_start_time_ms\": " << state.trip_start_time_ms << ",\n";
    file << "  \"segment_start_cntr1\": " << state.segment_start_cntr1 << ",\n";
    file << "  \"segment_start_cntr2\": " << state.segment_start_cntr2 << ",\n";
    file << "  \"segment_start_time_ms\": " << state.segment_start_time_ms << ",\n";
    file << "  \"segment_current_number\": " << state.segment_current_number << ",\n";
    file << "  \"rallyTimeOffset_ms\": " << state.rallyTimeOffset_ms << ",\n";
    file << "  \"segments\": [\n";
    for (size_t i = 0; i < state.segments.size(); i++) {
        file << "    {\n";
        file << "      \"target_speed_counts_per_hour\": " << state.segments[i].target_speed_counts_per_hour << ",\n";
        file << "      \"distance_counts\": " << state.segments[i].distance_counts << ",\n";
        file << "      \"autoNext\": " << (state.segments[i].autoNext ? "true" : "false") << "\n";
        file << "    }";
        if (i < state.segments.size() - 1) file << ",";
        file << "\n";
    }
    file << "  ]\n";
    file << "}\n";
}
