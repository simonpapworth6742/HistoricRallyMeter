#ifndef CONFIG_FILE_H
#define CONFIG_FILE_H

class RallyState;

class ConfigFile {
public:
    static void load(RallyState& state);
    static void save(const RallyState& state);
};

#endif // CONFIG_FILE_H
