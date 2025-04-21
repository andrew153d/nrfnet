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

#ifndef NERFNET_NET_MS_BIDIRECTIONAL_RADIO_INTERFACE_H_
#define NERFNET_NET_MS_BIDIRECTIONAL_RADIO_INTERFACE_H_

#include <optional>
#include "nerfnet/net/radio_interface.h"

#include <atomic>
#include <deque>
#include <mutex>
#include <optional>
#include <RF24/RF24.h>
#include <thread>
#include <vector>

#include "nerfnet/net/primary_radio_interface.h"
#include "nerfnet/net/secondary_radio_interface.h"

#include "nerfnet/util/non_copyable.h"

namespace nerfnet
{

  // The bidirectional radio interface.
  class BidirectionalRadioInterface : public RadioInterface
  {
  public:
    // Setup the bidirectional radio link.
    BidirectionalRadioInterface(uint16_t ce_pin, int tunnel_fd,
                                uint32_t primary_addr, uint32_t secondary_addr,
                                uint8_t channel, uint64_t poll_interval_us);

    // Runs the interface.
    void Run();

  private:
    // Radio Packet types
    enum packet_type
    {
      UNKNOWN,
      DISCOVERY,
      DISCOVERY_ACK,
      DATA,
      DATA_ACK,
    };
// Radio Packet
#pragma pack(push, 1)
    struct RadioPacket
    {
      uint8_t type : 3;      // packet type
      uint16_t id : 7;       // packet number, used to make sure packets arrive in order
      uint8_t size : 5;      // number of valid bytes in the payload
      bool final_packet : 1; // the final packet in a network frame
      uint8_t payload[30];   // the bytes
    };
#pragma pack(pop)

    enum role
    {
      PRIMARY,
      SECONDARY
    };

    // The role of the radio
    role radio_role_;

    //CE pin
    uint16_t ce_pin_;

    // tunnel file descriptor
    int tunnel_fd_;

    // radio channel
    uint8_t channel_;

    // tunnel logging
    bool tunnel_logs_enabled_ = false;

    // poll interval
    uint64_t poll_interval_us_;
  };

} // namespace nerfnet

#endif // NERFNET_NET_PRIMARY_RADIO_INTERFACE_H_
