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

#ifndef NERFNET_NET_AUTONEGOTIATOR_RADIO_INTERFACE_H_
#define NERFNET_NET_AUTONEGOTIATOR_RADIO_INTERFACE_H_

#include <optional>
#include "nerfnet/net/radio_interface.h"

#include <atomic>
#include <deque>
#include <mutex>
#include <optional>
#include <RF24/RF24.h>
#include <thread>
#include <vector>

#include "nerfnet/util/non_copyable.h"

namespace nerfnet
{
  enum RadioRole
  {
    UNKNOWN,
    PRIMARY,
    SECONDARY
  };

  // The bidirectional radio interface.
  class AutoNegotioator : public RadioInterface
  {
  public:
    // Setup the bidirectional radio link.
    AutoNegotioator(uint16_t ce_pin, int tunnel_fd,
                    uint32_t primary_addr, uint32_t secondary_addr,
                    uint8_t channel, uint64_t poll_interval_us);

    // Runs the interface.
    void Run();

    // returns true when autonegotiotion is complete
    bool IsComplete();

    // returns the role of the radio
    RadioRole GetRole();

  private:
    // Radio Packet types
    enum packet_type
    {
      UNKNOWN,
      DISCOVERY,
      DISCOVERY_ACK
    };
// Radio Packet
#pragma pack(push, 1)
    struct RadioPacket
    {
      uint8_t type;      // packet type
      uint8_t spare[31];   // the bytes
    };
#pragma pack(pop)

    // The role of the radio
    RadioRole radio_role_;


    uint64_t last_discovery_send_time = 0;

    uint64_t poll_period = 500000 + (std::rand() % (1000000 - 500000 + 1));
  };

} // namespace nerfnet

#endif // NERFNET_NET_PRIMARY_RADIO_INTERFACE_H_
