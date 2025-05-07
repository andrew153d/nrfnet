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

#ifndef NERFNET_NET_MESH_RADIO_INTERFACE_H_
#define NERFNET_NET_MESH_RADIO_INTERFACE_H_

#include <optional>
#include <functional>

#include "radio_interface.h"
#include <vector>
#include <deque>
#include <unordered_set>
#include <RF24/RF24.h>
#include "ILayer.h"

namespace nerfnet
{
  // The mesh mode radio interface.
  class MeshRadioInterface : public ILayer
  {
  public:
    // Setup the mesh radio link.
    MeshRadioInterface(uint16_t ce_pin, int tunnel_fd,
                       uint32_t primary_addr, uint32_t secondary_addr,
                       uint8_t channel, uint64_t poll_interval_us);

    // Runs the interface
    void Run();

    

  private:
    // The radio interface
    RF24 radio_;

    // The CE pin for the radio
    uint16_t ce_pin_;

    // The radio channel
    uint8_t channel_;

    // The node id for this radio
    uint8_t node_id_ = 0;

#pragma region Discovery

    // The rate at which the radio will send discovery messages.
    const uint64_t discovery_message_rate_us_ = 100000; // 100ms

    // The number of time the radio will send discovery messages before giving up
    const uint8_t max_discovery_messages_ = 3;

    // The amount of time the radio will wait to receive neighbor node ids before determining its own node id
    const uint64_t discovery_ack_timeout_us_ = 1000000; // 1s

    // The list of neighbor node ids.
    std::unordered_set<uint8_t> neighbor_node_ids_;

    // The number of discovery messages sent.
    uint8_t number_of_discovery_messages_sent_ = 0;

    // The time since the first discovery ack was received
    uint64_t discovery_ack_received_time_us_ = 0;

#pragma endregion

    // The minimum time the radio will be in a listening state
    const uint64_t min_listen_time_us_ = 2000; // 4ms

    // The last time the radio was put into a listening state
    uint64_t last_listen_time_us_ = 0;

    // The base address for all the radio pipes.
    const uint32_t base_address_ = 0xFFAB0000;

    // The offset for the discovery address.
    const uint32_t discovery_address_offset_ = 0xBA;

    // The address for the secondary radio, variable used to set writing pipe only when it needs to be changed
    uint32_t writing_pipe_address_ = 0;

    // The address for the primary radio, variable used to set reading pipe only when it needs to be changed
    uint32_t reading_pipe_addresses_[6];

    // The lower address from the initial randomly assigned node id.
    // All nodes above are considered discovery nodes.
    // All nodes below are considered as non-discovery nodes.
    const uint8_t min_discovery_node_id_ = 101;

    enum RadioState
    {
      Discovery,
      Listening,
      Sending,
    };

    RadioState radio_state_ = Discovery;
    uint64_t discovery_message_timer_ = 0;

#pragma region PacketDefenitions

    struct __attribute__((packed)) GenericPacket
    {
      uint8_t checksum : 4;
      uint8_t packet_type : 4;
      uint8_t padding2[31];
    };
    static_assert(sizeof(GenericPacket) == 32, "GenericPacket size must be 32 bytes");

    struct DiscoveryPacket
    {
      uint8_t checksum : 4;
      uint8_t packet_type : 4;
      uint8_t source_node_id;
      uint8_t padding;
      uint8_t payload[29];
    };
    static_assert(sizeof(DiscoveryPacket) == 32, "DiscoveryPacket size must be 32 bytes");

    struct DiscoveryAckPacket
    {
      uint8_t checksum : 4;
      uint8_t packet_type : 4;
      uint8_t source_node_id;
      uint8_t num_valid_neighbors;
      uint8_t neighbors[29];
    };
    static_assert(sizeof(DiscoveryAckPacket) == 32, "DiscoveryAckPacket size must be 32 bytes");

#pragma endregion

    struct PacketFrame
    {
      uint8_t packet_type;
      uint64_t last_time_sent;
      uint32_t remote_pipe_address;
      uint8_t data[32];
    };

    std::deque<PacketFrame> packets_to_send_;

    // The interval between poll operations to the secondary radio.
    const uint64_t poll_interval_us_;

    // Logic for poll backoff when the secondary radio is not responding.
    int poll_fail_count_;
    uint64_t current_poll_interval_us_;
    bool connection_reset_required_;

    void SetNodeId(uint8_t node_id);
    void SetRadioState(RadioState state);

    void Sender();

    void DiscoveryTask();
    void HandleDiscoveryPacket(const DiscoveryPacket &packet);
    void HandleDiscoveryAckPacket(const DiscoveryAckPacket &packet);
    void SendNodeIdAnnouncement();
    void HandleNodeIdAnnouncementPacket(const DiscoveryPacket &packet);


    void ReceiveFromDownstream(const std::vector<uint8_t>& data) override {}
    void ReceiveFromUpstream(const std::vector<uint8_t>& data) override;

    void InsertChecksum(GenericPacket &packet);
    bool ValidateChecksum(GenericPacket &packet);
    uint8_t CalculateChecksum(GenericPacket &packet);
  };

} // namespace nerfnet

#endif // NERFNET_NET_MESH_RADIO_INTERFACE_H_
