/*
 * Copyright 2020 Andrew Rossignol andrew.rossignol@gmail.com
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <arpa/inet.h>
#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <RF24/RF24.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "primary_radio_interface.h"
#include "secondary_radio_interface.h"
#include "mesh_radio_interface.h"
#include "tunnel_interface.h"
#include "log.h"
#include "config_parser.h"

// A description of the program.
constexpr char kDescription[] =
    "A tool for creating a network tunnel over cheap NRF24L01 radios.";

// The version of the program.
constexpr char kVersion[] = "0.0.1";

// Auto Negotiation of the radio interface.
RadioMode AutoNegotiateRadioInterface(uint16_t ce_pin, uint16_t channel, uint32_t discovery_address = 0xFFFABABA) {
  
  enum class PacketType {
    Discovery = 0x1F,
    DiscoverResponse = 0xA5,
  };

  RadioMode mode = RadioMode::NotSet;
  RF24 radio_(ce_pin, 0);
  uint32_t kMaxPacketSize = 32;
  std::vector<uint8_t> request(kMaxPacketSize, 0);
  request[0] = static_cast<uint8_t>(PacketType::Discovery);

  CHECK(channel < 128, "Channel must be between 0 and 127");
  CHECK(radio_.begin(), "Failed to start NRF24L01");
  radio_.setChannel(channel);
  radio_.setPALevel(RF24_PA_MAX);
  radio_.setDataRate(RF24_2MBPS);
  radio_.setAddressWidth(3);
  radio_.setAutoAck(1);
  radio_.setRetries(0, 15);
  radio_.setCRCLength(RF24_CRC_8);
  CHECK(radio_.isChipConnected(), "NRF24L01 is unavailable");

  radio_.openWritingPipe(discovery_address);
  radio_.openReadingPipe(1, discovery_address);

  //Transmit discovery packet
  radio_.stopListening();

  if (request.size() > kMaxPacketSize) {
    LOGE("Request is too large (%zu vs %zu)", request.size(), kMaxPacketSize);
  }

  if (!radio_.write(request.data(), request.size())) {
    LOGE("Failed to write request");
  }

  while (!radio_.txStandBy()) {
    LOGI("Waiting for transmit standby");
  }

  //Receive
  radio_.startListening();
  while(1){
    if(radio_.available()){
      std::vector<uint8_t> response(kMaxPacketSize);
      radio_.read(response.data(), response.size());
      LOGI("Received %d bytes from the tunnel", response.size());
      if (response.size() > 0) {
        if (response[0] == static_cast<uint8_t>(PacketType::DiscoverResponse)) {
          mode = RadioMode::Primary;
          break;
        } else if (response[0] == static_cast<uint8_t>(PacketType::Discovery)) {
          mode = RadioMode::Secondary;
          // Send back a discovery response
          std::vector<uint8_t> response_packet(kMaxPacketSize, 0);
          response_packet[0] = static_cast<uint8_t>(PacketType::DiscoverResponse);
          radio_.stopListening();
          if (!radio_.write(response_packet.data(), response_packet.size())) {
            LOGE("Failed to send discovery response");
          }
          break;
        }
      }
    }
  }

  return mode;
}


// Sets flags for a given interface. Quits and logs the error on failure.
void SetInterfaceFlags(const std::string_view& device_name, int flags) {
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  CHECK(fd >= 0, "Failed to open socket: %s (%d)", strerror(errno), errno);

  struct ifreq ifr = {};
  ifr.ifr_flags = flags;
  strncpy(ifr.ifr_name, std::string(device_name).c_str(), IFNAMSIZ);
  int status = ioctl(fd, SIOCSIFFLAGS, &ifr);
  CHECK(status >= 0, "Failed to set tunnel interface: %s (%d)",
      strerror(errno), errno);
  close(fd);
}

void SetIPAddress(const std::string_view& device_name,
                  const std::string_view& ip, const std::string& ip_mask) {
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  CHECK(fd >= 0, "Failed to open socket: %s (%d)", strerror(errno), errno);

  struct ifreq ifr = {};
  strncpy(ifr.ifr_name, std::string(device_name).c_str(), IFNAMSIZ);

  ifr.ifr_addr.sa_family = AF_INET;
  CHECK(inet_pton(AF_INET, std::string(ip).c_str(),
        &reinterpret_cast<struct sockaddr_in*>(&ifr.ifr_addr)->sin_addr) == 1,
      "Failed to assign IP address: %s (%d)", strerror(errno), errno);
  int status = ioctl(fd, SIOCSIFADDR, &ifr);
  CHECK(status >= 0, "Failed to set tunnel interface ip: %s (%d)",
      strerror(errno), errno);

  ifr.ifr_netmask.sa_family = AF_INET;
  CHECK(inet_pton(AF_INET, std::string(ip_mask).c_str(),
        &reinterpret_cast<struct sockaddr_in*>(&ifr.ifr_netmask)->sin_addr) == 1,
      "Failed to assign IP mask: %s (%d)", strerror(errno), errno);
  status = ioctl(fd, SIOCSIFNETMASK, &ifr);
  CHECK(status >= 0, "Failed to set tunnel interface mask: %s (%d)",
      strerror(errno), errno);
  close(fd);
}

// Opens the tunnel interface to listen on. Always returns a valid file
// descriptor or quits and logs the error.
int OpenTunnel(const std::string_view& device_name) {
  int fd = open("/dev/net/tun", O_RDWR);
  CHECK(fd >= 0, "Failed to open tunnel file: %s (%d)", strerror(errno), errno);

  struct ifreq ifr = {};
  ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
  strncpy(ifr.ifr_name, std::string(device_name).c_str(), IFNAMSIZ);

  int status = ioctl(fd, TUNSETIFF, &ifr);
  CHECK(status >= 0, "Failed to set tunnel interface: %s (%d)",
      strerror(errno), errno);
  return fd;
}

int main(int argc, char** argv) {
  // Load configuration file
  ConfigParser config("/etc/nrfnet/nrfnet.conf");
  config.load();
  // Print configuration values
  config.print();

  RadioMode mode = config.mode.value();

  std::string tunnel_ip = config.tunnel_ip_address.value();

  // Setup tunnel.
  int tunnel_fd = OpenTunnel(config.interface_name.value());
  LOGI("tunnel '%s' opened", config.interface_name.value().c_str());
  SetInterfaceFlags(config.interface_name.value(), IFF_UP);
  LOGI("tunnel '%s' up", config.interface_name.value().c_str());
  SetIPAddress(config.interface_name.value(), tunnel_ip,
   config.tunnel_netmask.value());
  LOGI("tunnel '%s' configured with '%s' mask '%s'",
       config.interface_name.value().c_str(), tunnel_ip.c_str(),
       config.tunnel_netmask.value().c_str());

  
    

  if(mode == RadioMode::Mesh) {
    nerfnet::MeshRadioInterface radio_interface(
    config.ce_pin.value(), 0,
    0x55, 0x66,
    config.channel.value(), config.poll_interval.value());
    
    nerfnet::TunnelInterface tunnel_interface(radio_interface, tunnel_fd);
    tunnel_interface.Run();
  } else if(mode == RadioMode::Automatic)
  {
    LOGI("Negotiating Radio Roles");
    mode = AutoNegotiateRadioInterface(config.ce_pin.value(), config.channel.value());
    CHECK(mode != RadioMode::NotSet, "Failed to negotiate radio roles");
    LOGI("Negotiated Radio Roles: %s", mode == RadioMode::Primary ? "Primary" : "Secondary");
  }

  if (mode == RadioMode::Primary) {
    nerfnet::PrimaryRadioInterface radio_interface(
        config.ce_pin.value(), tunnel_fd,
        0x55, 0x66,
        config.channel.value(), config.poll_interval.value());
    radio_interface.SetTunnelLogsEnabled(config.enable_tunnel_logs.value());
    radio_interface.Run();
  } else if (mode == RadioMode::Secondary) {
    nerfnet::SecondaryRadioInterface radio_interface(
        config.ce_pin.value(), tunnel_fd,
        0x55, 0x66,
        config.channel.value());
    radio_interface.SetTunnelLogsEnabled(config.enable_tunnel_logs.value());
    radio_interface.Run();
  } else {
    CHECK(false, "Primary or secondary mode must be enabled");
  }

  return 0;
}
