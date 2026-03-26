#ifndef CONFIG_FILE_H
#define CONFIG_FILE_H

#include <string>

class RallyState;

class ConfigFile {
public:
    static constexpr const char* DEFAULT_PATH = "rally_config.json";
    static void load(RallyState& state, const std::string& path = DEFAULT_PATH);
    static void save(const RallyState& state, const std::string& path = DEFAULT_PATH);
};

#endif // CONFIG_FILE_H
