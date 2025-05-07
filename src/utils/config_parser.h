#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <string>
#include <unordered_map>
#include <stdio.h>
#include <stdint.h>
#include <optional>
#include "log.h"
enum class RadioMode
{
    NotSet,
    Primary,
    Secondary,
    Automatic,
    Mesh
};

class ConfigParser
{
public:
    // Constructor that takes the path to the config file
    explicit ConfigParser(const std::string &filePath);

    // Load and parse the configuration file
    void load();

    // Print out all of the configuration values for debugging
    void print();

    // Getters for configuration values
    std::optional<std::string> interface_name;
    std::optional<RadioMode> mode;
    std::optional<uint8_t> channel;
    std::optional<std::string> tunnel_ip_address;
    std::optional<std::string> tunnel_netmask;
    std::optional<uint64_t> poll_interval;
    std::optional<bool> enable_tunnel_logs;
    std::optional<uint16_t> ce_pin;

private:
    // Get a value from the configuration file
    std::string get(const std::string &key) const;

    std::string filePath;
    std::unordered_map<std::string, std::string> config;
};

#endif // CONFIG_PARSER_H
