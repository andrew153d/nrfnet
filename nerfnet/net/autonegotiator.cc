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

#include "nerfnet/net/autonegotiator.h"

#include <unistd.h>

#include "nerfnet/util/log.h"
#include "nerfnet/util/macros.h"
#include "nerfnet/util/time.h"

namespace nerfnet
{
    AutoNegotioator::AutoNegotioator(
        uint16_t ce_pin, int tunnel_fd, uint32_t primary_addr, uint32_t secondary_addr, uint8_t channel,
        uint64_t poll_interval_us)
        : RadioInterface(ce_pin, tunnel_fd, primary_addr, secondary_addr, channel),
          radio_role_(RadioRole::UNKNOWN)
    {
    CHECK(channel < 128, "Channel must be between 0 and 127");
    CHECK(radio_.begin(), "Failed to start NRF24L01");
    radio_.setChannel(channel);
    radio_.setPALevel(RF24_PA_MIN, false); // low power for now
    radio_.setDataRate(RF24_2MBPS);
    radio_.setAddressWidth(3);
    radio_.setAutoAck(1);
    radio_.setRetries(0, 15);
    radio_.setCRCLength(RF24_CRC_8);
    CHECK(radio_.isChipConnected(), "NRF24L01 is unavailable");

    LOGI("\nRADIO AUTO NEGOTIATION\n");

    // Begin auto negotiation section //
    // - Radio opens a pipe with a discovery address
    // - Radio sends out its discovery data and listens for a response from other radios
    // - If no response, it sets its reading address and writing address to defaults
    // - if a radio receives a discovery message, it will respond with its reading address and writing address

    uint32_t base_address = 0xABDFF00;
    uint32_t discovery_pipe_address = base_address;
    uint32_t reading_pipe_address = base_address + 1;
    uint32_t writing_pipe_address = base_address + 2;

    uint32_t discovery_pipe = 2;
    RadioPacket discovery_packet;
    std::memset(&discovery_packet, 0, sizeof(discovery_packet));
    discovery_packet.type = packet_type::DISCOVERY;

    LOGI("opening discovery reading pipe on %d, 0x%x", discovery_pipe, discovery_pipe_address);

    radio_.openReadingPipe(discovery_pipe, discovery_pipe_address);
    radio_.openWritingPipe(discovery_pipe_address);
    radio_.stopListening();
    radio_.write(&discovery_packet, sizeof(discovery_packet)); // transmit discovery payload
    radio_.txStandBy();                                        // ensure the radio is in standby mode before listening
    radio_.startListening();

    // Wait 5 seconds for a returning discovery ack message
    bool configuration_complete = false;
    auto start = nerfnet::TimeNowUs();
    RadioPacket recvd_packet;

    auto last_discovery_send_time = TimeNowUs();
    while (!configuration_complete)
    {
      if (radio_.available())
      {
        uint8_t pipe;
        if (radio_.available(&pipe))
        {

          uint8_t bytes = radio_.getPayloadSize();
          radio_.read(&recvd_packet, bytes);

          switch (recvd_packet.type)
          {
          case packet_type::DISCOVERY:
            LOGI("Recieved Discovery");
            radio_.stopListening();
            RadioPacket send_packet;
            std::memset(&send_packet, 0, sizeof(send_packet));
            send_packet.type = packet_type::DISCOVERY_ACK;
            //send_packet.payload[0] = reading_pipe_address & 0xFF;
            //send_packet.payload[1] = writing_pipe_address & 0xFF;
            radio_.write(&send_packet, sizeof(send_packet)); // transmit discovery payload
            radio_.txStandBy();                              // ensure the radio is in standby mode before listening

            configuration_complete = true;
            break;
          case packet_type::DISCOVERY_ACK:
            radio_.stopListening();
            LOGI("Received Discovery Ack");
            LOGI("Round Trip Time: %llu us", nerfnet::TimeNowUs() - last_discovery_send_time);
            //reading_pipe_address = recvd_packet.payload[1] | base_address;
            //writing_pipe_address = recvd_packet.payload[0] | base_address;
            configuration_complete = true;
            SleepUs(5246);
            break;
          }
        }
      }
      // send discovery every half second
      if (nerfnet::TimeNowUs() - start > 1000000)
      {
        start = nerfnet::TimeNowUs();
        radio_.stopListening();                                                  // put radio in TX mode
        bool result = radio_.write(&discovery_packet, sizeof(discovery_packet)); // transmit & save the report
        last_discovery_send_time = TimeNowUs();
        radio_.txStandBy();
        if (result)
        {
          LOGI("Sent discovery packet");
        }
        else
        {
          LOGI("Failed to send discovery packet");
        }
        radio_.startListening();
      }
    }

    radio_.stopListening();

    radio_.flush_rx();
    radio_.flush_tx();

    LOGI("Reading pipe address: 0x%x", reading_pipe_address);
    LOGI("Writing pipe address: 0x%x", writing_pipe_address);
    radio_.openReadingPipe(kPipeId, reading_pipe_address);
    radio_.openWritingPipe(writing_pipe_address);

    radio_.startListening();

    LOGI("\nRADIO AUTO NEGOTIATION COMPLETE\n");


    }

    void AutoNegotioator::Run()
    {
    }

    bool AutoNegotioator::IsComplete()
    {
        return radio_role_ != RadioRole::UNKNOWN;
    }

    RadioRole AutoNegotioator::GetRole()
    {
        return radio_role_;
    }
} // namespace nerfnet
