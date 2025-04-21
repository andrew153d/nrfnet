#include "config_parser.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>
ConfigParser::ConfigParser(const std::string& filePath) : filePath(filePath) {}

void ConfigParser::load() {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + filePath);
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') {
            continue;
        }
        //std::cout << "Processing line: " << line << std::endl;
        std::istringstream lineStream(line);
        std::string key, value;
        if (std::getline(lineStream, key, '=') && std::getline(lineStream, value)) {
            
            config[key] = value;
        }
    }

    // Parse the configuration values into config_values
    config_values.interface_name = get("interface_name");
    
    std::string mode_str = get("mode");
    if (mode_str == "primary") {
        config_values.mode = PRIMARY;
    } else if (mode_str == "secondary") {
        config_values.mode = SECONDARY;
    } else if (mode_str == "common") {
        config_values.mode = COMMON;
    } else if (mode_str == "bidirectional") {
        config_values.mode = MS_BIDIRECTIONAL;
    } else {
        throw std::runtime_error("Invalid mode in config: " + mode_str);
    }

    config_values.channel = std::stoi(get("channel"));
    config_values.ip_address = get("ip_address");
    config_values.ip_mask = get("netmask");
    config_values.tunnel_ip_address = get("tunnel_ip_address");
    config_values.tunnel_netmask = get("tunnel_netmask");
    config_values.poll_interval = std::stoull(get("poll_interval"));
    config_values.enable_tunnel_logs = (get("enable_tunnel_logs") == "true");
    config_values.ce_pin = std::stoi(get("ce_pin"));
    config_values.device_id = std::stoi(get("device_id"));

    if (config_values.ce_pin < 0 || config_values.ce_pin > 255) {
        throw std::runtime_error("Invalid CE pin in config: " + std::to_string(config_values.ce_pin));
    }
    if (config_values.channel < 0 || config_values.channel > 127) {
        throw std::runtime_error("Invalid channel in config: " + std::to_string(config_values.channel));
    }
    if (config_values.poll_interval < 0) {
        throw std::runtime_error("Invalid poll interval in config: " + std::to_string(config_values.poll_interval));
    }
    if (config_values.interface_name.empty()) {
        throw std::runtime_error("Interface name is empty in config");
    }
    if (config_values.ip_address.empty()) {
        throw std::runtime_error("IP address is empty in config");
    }
    if (config_values.ip_mask.empty()) {
        throw std::runtime_error("IP mask is empty in config");
    }
    if (config_values.tunnel_ip_address.empty()) {
        throw std::runtime_error("Tunnel IP address is empty in config");
    }
    if (config_values.tunnel_netmask.empty()) {
        throw std::runtime_error("Tunnel netmask is empty in config");
    }
    if (config_values.tunnel_ip_address == config_values.ip_address) {
        throw std::runtime_error("Tunnel IP address cannot be the same as IP address");
    }
}

std::string ConfigParser::get(const std::string& key) const {
    auto it = config.find(key);
    if (it != config.end()) {
        return it->second;
    }
    throw std::runtime_error("Key not found in config: " + key);
}

ConfigValues ConfigParser::getConfig() {
    return config_values;
}
