#include "config_parser.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>
ConfigParser::ConfigParser(const std::string& filePath) : filePath(filePath) {
    // Initialize default values
    interface_name.reset();
    mode.reset();
    channel.reset();
    tunnel_ip_address.reset();
    tunnel_netmask.reset();
    poll_interval.reset();
    enable_tunnel_logs.reset();
    ce_pin.reset();
}

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
    if(config.find("interface_name") != config.end()) {
        interface_name = get("interface_name");
    }

    if(config.find("mode") != config.end()) {
        std::string mode_string = get("mode");
        if(mode_string=="primary") {
            mode = RadioMode::Primary;
        } else if(mode_string=="secondary") {
            mode = RadioMode::Secondary;
        } else if(mode_string=="automatic") {
            mode = RadioMode::Automatic;
        } else if(mode_string=="mesh") {
            mode = RadioMode::Mesh;
        } else {
            mode = RadioMode::NotSet;
        }
    }

    if(config.find("channel") != config.end()) {
        channel = std::stoi(get("channel"));
    }
    if(config.find("tunnel_ip_address") != config.end()) {
        tunnel_ip_address = get("tunnel_ip_address");
    }
    if(config.find("tunnel_netmask") != config.end()) {
        tunnel_netmask = get("tunnel_netmask");
    }
    if(config.find("poll_interval") != config.end()) {
        poll_interval = std::stoull(get("poll_interval"));
    }
    if(config.find("enable_tunnel_logs") != config.end()) {
        enable_tunnel_logs = (get("enable_tunnel_logs") == "true");
    }
    if(config.find("ce_pin") != config.end()) {
        ce_pin = std::stoi(get("ce_pin"));
    }
    if(config.find("discovery_address") != config.end()) {
        discovery_address = std::stoul(get("discovery_address"), nullptr, 16);
    }
    if(config.find("power_level") != config.end()) {
        power_level = std::stoi(get("power_level"));
    }
    if(config.find("low_noise_amplifier") != config.end()) {
        low_noise_amplifier = (get("low_noise_amplifier") == "true");
    }
    if(config.find("data_rate") != config.end()) {
        data_rate = std::stoi(get("data_rate"));
    }
    if(config.find("address_width") != config.end()) {
        address_width = std::stoi(get("address_width"));
    }

    // Validate that all of the parameters are set
    if (!interface_name) {
        throw std::runtime_error("Missing required parameter: interface_name");
    }
    if (!mode) {
        throw std::runtime_error("Missing required parameter: mode");
    }
    if (!channel) {
        throw std::runtime_error("Missing required parameter: channel");
    }
    if (!tunnel_ip_address) {
        throw std::runtime_error("Missing required parameter: tunnel_ip_address");
    }
    if (!tunnel_netmask) {
        throw std::runtime_error("Missing required parameter: tunnel_netmask");
    }
    if (!poll_interval) {
        throw std::runtime_error("Missing required parameter: poll_interval");
    }
    if (!enable_tunnel_logs) {
        throw std::runtime_error("Missing required parameter: enable_tunnel_logs");
    }
    if (!ce_pin) {
        throw std::runtime_error("Missing required parameter: ce_pin");
    }
    if (!discovery_address) {
        throw std::runtime_error("Missing required parameter: discovery_address");
    }
    if (!power_level) {
        throw std::runtime_error("Missing required parameter: power_level");
    }
    if (!low_noise_amplifier) {
        throw std::runtime_error("Missing required parameter: low_noise_amplifier");
    }
    if (!data_rate) {
        throw std::runtime_error("Missing required parameter: data_rate");
    }
    if (!address_width) {
        throw std::runtime_error("Missing required parameter: address_width");
    }
}

void ConfigParser::print()
{
    LOGI("Configuration values:");
    for (const auto& pair : config) {
        LOGI("%s: %s", pair.first.c_str(), pair.second.c_str());
    }
}

std::string ConfigParser::get(const std::string& key) const {
    auto it = config.find(key);
    if (it != config.end()) {
        return it->second;
    }
    throw std::runtime_error("Key not found in config: " + key);
}
