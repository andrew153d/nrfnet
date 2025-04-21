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
#include <tclap/CmdLine.h>
#include <unistd.h>

#include "nerfnet/net/config_parser.h"
#include "nerfnet/net/primary_radio_interface.h"
#include "nerfnet/net/secondary_radio_interface.h"
#include "nerfnet/net/common_radio_interface.h"
#include "nerfnet/net/ms_bidirectional_radio_interface.h"
#include "nerfnet/util/log.h"
#include "nerfnet/util/time.h"

using namespace std;

// A description of the program.
constexpr char kDescription[] =
    "A tool for creating a network tunnel over cheap NRF24L01 radios.";

// The version of the program.
constexpr char kVersion[] = "0.0.1";

// Sets flags for a given interface. Quits and logs the error on failure.
void SetInterfaceFlags(const std::string_view &device_name, int flags)
{
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

void SetIPAddress(const std::string_view &device_name,
                  const std::string_view &ip, const std::string &ip_mask)
{
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  CHECK(fd >= 0, "Failed to open socket: %s (%d)", strerror(errno), errno);

  struct ifreq ifr = {};
  strncpy(ifr.ifr_name, std::string(device_name).c_str(), IFNAMSIZ);

  ifr.ifr_addr.sa_family = AF_INET;
  CHECK(inet_pton(AF_INET, std::string(ip).c_str(),
                  &reinterpret_cast<struct sockaddr_in *>(&ifr.ifr_addr)->sin_addr) == 1,
        "Failed to assign IP address: %s (%d)", strerror(errno), errno);
  int status = ioctl(fd, SIOCSIFADDR, &ifr);
  CHECK(status >= 0, "Failed to set tunnel interface ip: %s (%d)",
        strerror(errno), errno);

  ifr.ifr_netmask.sa_family = AF_INET;
  CHECK(inet_pton(AF_INET, std::string(ip_mask).c_str(),
                  &reinterpret_cast<struct sockaddr_in *>(&ifr.ifr_netmask)->sin_addr) == 1,
        "Failed to assign IP mask: %s (%d)", strerror(errno), errno);
  status = ioctl(fd, SIOCSIFNETMASK, &ifr);
  CHECK(status >= 0, "Failed to set tunnel interface mask: %s (%d)",
        strerror(errno), errno);
  close(fd);
}

// Opens the tunnel interface to listen on. Always returns a valid file
// descriptor or quits and logs the error.
int OpenTunnel(const std::string_view &device_name)
{
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

int main(int argc, char **argv)
{
  // Load configuration file
  ConfigParser config("/etc/nrfmesh/nrfmesh.conf");
  config.load();

  TCLAP::CmdLine cmd(kDescription, ' ', kVersion);
  TCLAP::ValueArg<int> mode_arg("", "mode",
                                "Set the mode of operation (0: PRIMARY, 1: SECONDARY, 2: COMMON).",
                                false, 0, "integer", cmd);

  cmd.parse(argc, argv);

  // return 0;

  std::string tunnel_ip = config.getConfig().tunnel_ip_address; // tunnel_ip_arg.getValue();
  // if (!tunnel_ip_arg.isSet())
  // {
  //   if (primary_arg.getValue())
  //   {
  //     tunnel_ip = "192.168.10.1";
  //   }
  //   else if (secondary_arg.getValue())
  //   {
  //     tunnel_ip = "192.168.10.2";
  //   }
  //   else if (common_arg.getValue())
  //   {
  //     tunnel_ip = config.get("ip_address");
  //   }
  // }

  // Setup tunnel.
  int tunnel_fd = OpenTunnel(config.getConfig().interface_name);
  LOGI("tunnel '%s' opened", config.getConfig().interface_name.c_str());
  SetInterfaceFlags(config.getConfig().interface_name, IFF_UP);
  LOGI("tunnel '%s' up", config.getConfig().interface_name.c_str());
  SetIPAddress(config.getConfig().interface_name, tunnel_ip,
               config.getConfig().tunnel_netmask);
  LOGI("tunnel '%s' configured with '%s' mask '%s'",
       config.getConfig().interface_name.c_str(), tunnel_ip.c_str(),
       config.getConfig().tunnel_netmask.c_str());

  if ((!mode_arg.isSet() && config.getConfig().mode == PRIMARY) || (mode_arg.isSet() && mode_arg.getValue() == 0))
  {
    LOGI("Starting primary radio interface");
    nerfnet::PrimaryRadioInterface radio_interface(
        config.getConfig().ce_pin, tunnel_fd,
        0xFFAB, 0xFFAB,
        config.getConfig().channel, config.getConfig().poll_interval);
    radio_interface.SetTunnelLogsEnabled(config.getConfig().enable_tunnel_logs);
    radio_interface.Run();
  }
  else if ((!mode_arg.isSet() && config.getConfig().mode == SECONDARY) || (mode_arg.isSet() && mode_arg.getValue() == 1))
  {
    LOGI("Starting secondary radio interface");
    nerfnet::SecondaryRadioInterface radio_interface(
        config.getConfig().ce_pin, tunnel_fd,
        0xFFAB, 0xFFAB,
        config.getConfig().channel);
    radio_interface.SetTunnelLogsEnabled(config.getConfig().enable_tunnel_logs);
    radio_interface.Run();
  }
  else if ((!mode_arg.isSet() && config.getConfig().mode == COMMON) || (mode_arg.isSet() && mode_arg.getValue() == 2))
  {
    LOGI("Starting common radio interface");
    nerfnet::CommonRadioInterface radio_interface(
        config.getConfig().device_id,
        config.getConfig().ce_pin, tunnel_fd,
        0xFFAB, 0XBBAF,
        config.getConfig().channel, config.getConfig().poll_interval);
    radio_interface.SetTunnelLogsEnabled(config.getConfig().enable_tunnel_logs);
    radio_interface.Run();
  }else if ((!mode_arg.isSet() && config.getConfig().mode == MS_BIDIRECTIONAL) || (mode_arg.isSet() && mode_arg.getValue() == 3))
  {
    LOGI("Starting msbidirectional radio interface");
    nerfnet::BidirectionalRadioInterface radio_interface(
        config.getConfig().ce_pin, tunnel_fd,
        0xFFAB, 0xFFAB,
        config.getConfig().channel, config.getConfig().poll_interval);
    radio_interface.SetTunnelLogsEnabled(config.getConfig().enable_tunnel_logs);
    radio_interface.Run();
  }
  else
  {
    LOGE("Invalid mode specified");
    return -1;
  }

  return 0;
}
