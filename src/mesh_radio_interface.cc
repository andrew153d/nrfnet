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

#include "mesh_radio_interface.h"

#include <unistd.h>

#include "log.h"
#include "macros.h"
#include "nrftime.h"

namespace nerfnet
{

  MeshRadioInterface::MeshRadioInterface(
      uint16_t ce_pin, int tunnel_fd,
      uint32_t primary_addr, uint32_t secondary_addr, uint8_t channel,
      uint64_t poll_interval_us)
      : radio_(ce_pin, 0),
        poll_interval_us_(poll_interval_us),
        current_poll_interval_us_(poll_interval_us_),
        connection_reset_required_(true),
        ce_pin_(ce_pin),
        channel_(channel)
  {
    last_send_time_us_ = 0;
    LOGI("Starting mesh radio interface");
  }

  // Method of operation
  /*
  Discovery:
    Every radio will always open pipe 0 at 0xFFABA01 for discovery messages only
    While a radio is in discovery mode, it will send out a discovery packet every 1 second.
    The packet will contain a packet type(discovery), its node_id which will be unknown, and a pipe address other nodes may send data back to
    The radio will listen for a response from other radios. If a response is received, the radio will stop sending discovery packets and start listening for data on the pipe address.
    Other radios will send back a message that will contain the senders node id as well as all neighbor node_ids.
    After waiting some time, the radio will assign itself a node_id and update its neighbor list.

  */

  void MeshRadioInterface::Run()
  {
    uint32_t discovery_address = 0xFFFABABA;

    CHECK(channel_ < 128, "Channel must be between 0 and 127");
    CHECK(radio_.begin(), "Failed to start NRF24L01");
    radio_.setChannel(channel_);
    radio_.setPALevel(RF24_PA_MAX);
    radio_.setDataRate(RF24_2MBPS);
    radio_.setAddressWidth(3);
    radio_.setAutoAck(1);
    radio_.setRetries(0, 15);
    radio_.setCRCLength(RF24_CRC_8);
    CHECK(radio_.isChipConnected(), "NRF24L01 is unavailable");

    radio_.openWritingPipe(discovery_address);
    radio_.openReadingPipe(1, discovery_address);

    // Transmit discovery packet
    radio_.startListening();

    // if (request.size() > kMaxPacketSize) {
    //   LOGE("Request is too large (%zu vs %zu)", request.size(), kMaxPacketSize);
    // }

    while (1)
    {
      if(TimeNowUs() - last_send_time_us_ > 3000000)
      {
        last_send_time_us_ = TimeNowUs();
        std::array<char, 32> discovery_packet = {static_cast<char>(0x1F), static_cast<char>(0x00), static_cast<char>(0x00)};
        packets_to_send_.emplace_back(discovery_packet);

        LOGI("Added new discovery packet to send queue");
      }
      if(radio_.available())
      {
        std::vector<uint8_t> response(32);
        radio_.read(response.data(), response.size());
        LOGI("Received %d bytes from the tunnel", response.size());
      }
      Sender();
    }
  }

  void MeshRadioInterface::Sender()
  {
    if(packets_to_send_.empty())
    {
      return;
    }

    radio_.stopListening();

    auto element = packets_to_send_.front();

    if (!radio_.write(element.data(), element.size()))
    {
      LOGE("Failed to write request");
    }

    while (!radio_.txStandBy())
    {
      LOGI("Waiting for transmit standby");
    }

    packets_to_send_.pop_front();
    radio_.startListening();
  }

  bool MeshRadioInterface::ConnectionReset()
  {
    return false;
  }

  bool MeshRadioInterface::PerformTunnelTransfer()
  {

    return true;
  }

  void MeshRadioInterface::HandleTransactionFailure()
  {
  }

} // namespace nerfnet
