#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <string>
#include <unordered_map>
#include <stdio.h>
#include <stdint.h>

enum operation_mode
{
    PRIMARY,
    SECONDARY,
    COMMON, 
    MS_BIDIRECTIONAL,
};

struct ConfigValues {
    std::string interface_name;
    operation_mode mode;
    uint8_t channel;
    std::string ip_address;
    std::string ip_mask;
    std::string tunnel_ip_address;
    std::string tunnel_netmask;
    uint64_t poll_interval;
    bool enable_tunnel_logs;
    uint16_t ce_pin;
    uint8_t device_id;
};

class ConfigParser {
public:
    // Constructor that takes the path to the config file
    explicit ConfigParser(const std::string& filePath);

    // Load and parse the configuration file
    void load();

    // Get a value from the configuration file
    std::string get(const std::string& key) const;

    ConfigValues getConfig();

private:
    std::string filePath;
    std::unordered_map<std::string, std::string> config;
    ConfigValues config_values;
};

#endif // CONFIG_PARSER_H
