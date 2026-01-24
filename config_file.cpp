#include "config_file.h"
#include "rally_state.h"
#include <fstream>
#include <string>
#include <sstream>

static int64_t extractLong(const std::string& line) {
    size_t pos = line.find(':');
    if (pos != std::string::npos) {
        std::string val = line.substr(pos + 1);
        // Remove trailing comma if present
        size_t comma = val.find(',');
        if (comma != std::string::npos) val = val.substr(0, comma);
        return std::stoll(val);
    }
    return 0;
}

static bool extractBool(const std::string& line) {
    return line.find("true") != std::string::npos;
}

void ConfigFile::load(RallyState& state) {
    std::ifstream file("rally_config.json");
    if (!file.is_open()) {
        return;  // Use defaults
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();
    
    std::istringstream stream(content);
    std::string line;
    bool in_segments = false;
    bool in_segment_obj = false;
    Segment current_segment = {0, 0, false};
    
    while (std::getline(stream, line)) {
        if (line.find("\"segments\"") != std::string::npos) {
            in_segments = true;
            state.segments.clear();
            continue;
        }
        
        if (in_segments) {
            if (line.find('{') != std::string::npos) {
                in_segment_obj = true;
                current_segment = {0, 0, false};
            } else if (line.find('}') != std::string::npos && in_segment_obj) {
                state.segments.push_back(current_segment);
                in_segment_obj = false;
            } else if (line.find(']') != std::string::npos) {
                in_segments = false;
            } else if (in_segment_obj) {
                if (line.find("\"target_speed_counts_per_hour\"") != std::string::npos) {
                    current_segment.target_speed_counts_per_hour = extractLong(line);
                } else if (line.find("\"distance_counts\"") != std::string::npos) {
                    current_segment.distance_counts = extractLong(line);
                } else if (line.find("\"autoNext\"") != std::string::npos) {
                    current_segment.autoNext = extractBool(line);
                }
            }
        } else {
            // Parse non-segment fields
            if (line.find("\"units\"") != std::string::npos) {
                state.units = extractBool(line);
            } else if (line.find("\"calibration\"") != std::string::npos) {
                state.calibration = extractLong(line);
            } else if (line.find("\"counters\"") != std::string::npos) {
                state.counters = extractBool(line);
            } else if (line.find("\"total_start_cntr1\"") != std::string::npos) {
                state.total_start_cntr1 = static_cast<uint32_t>(extractLong(line));
            } else if (line.find("\"total_start_cntr2\"") != std::string::npos) {
                state.total_start_cntr2 = static_cast<uint32_t>(extractLong(line));
            } else if (line.find("\"total_start_time_ms\"") != std::string::npos) {
                state.total_start_time_ms = extractLong(line);
            } else if (line.find("\"trip_start_cntr1\"") != std::string::npos) {
                state.trip_start_cntr1 = static_cast<uint32_t>(extractLong(line));
            } else if (line.find("\"trip_start_cntr2\"") != std::string::npos) {
                state.trip_start_cntr2 = static_cast<uint32_t>(extractLong(line));
            } else if (line.find("\"trip_start_time_ms\"") != std::string::npos) {
                state.trip_start_time_ms = extractLong(line);
            } else if (line.find("\"segment_start_cntr1\"") != std::string::npos) {
                state.segment_start_cntr1 = static_cast<uint32_t>(extractLong(line));
            } else if (line.find("\"segment_start_cntr2\"") != std::string::npos) {
                state.segment_start_cntr2 = static_cast<uint32_t>(extractLong(line));
            } else if (line.find("\"segment_start_time_ms\"") != std::string::npos) {
                state.segment_start_time_ms = extractLong(line);
            } else if (line.find("\"segment_current_number\"") != std::string::npos) {
                state.segment_current_number = static_cast<int32_t>(extractLong(line));
            } else if (line.find("\"rallyTimeOffset_ms\"") != std::string::npos) {
                state.rallyTimeOffset_ms = extractLong(line);
            }
        }
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
