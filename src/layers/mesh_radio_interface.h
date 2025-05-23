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
    MeshRadioInterface(uint16_t ce_pin,
                       int tunnel_fd,
                       uint32_t primary_addr,
                       uint32_t secondary_addr,
                       uint8_t channel,
                       uint64_t poll_interval_us,
                       uint32_t discovery_address,
                       uint8_t power_level,
                       bool lna,
                       uint8_t data_rate);

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
    const uint64_t discovery_message_rate_us_ = 1000000; // 1000ms

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
    const uint64_t continuous_listen_time_us_ = 10000; // 10ms

    // The minimum time the radio will be in a listening state
    const uint64_t send_receive_period_us_ = 5000; // 5ms

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

    enum CommsState
    {
      CommsNone,
      Timing,
      Discovery,
      Running,
    };

    enum RadioState
    {
      RadioNone,
      Listening,
      Sending,
      Continuous
    };

    CommsState comms_state_ = CommsNone;
    RadioState radio_state = RadioNone;

    uint64_t last_state_change_time_ = 0;

    uint64_t discovery_message_timer_ = 0;

#pragma region PacketDefenitions

    struct __attribute__((packed)) GenericPacket
    {
      uint8_t checksum : 4;
      uint8_t packet_type : 4;
      uint8_t padding2[31];
    };
    static_assert(sizeof(GenericPacket) == 32, "GenericPacket size must be 32 bytes");

    struct __attribute__((packed)) DiscoveryPacket
    {
      uint8_t checksum : 4;
      uint8_t packet_type : 4;
      uint8_t source_node_id;
      uint8_t padding;
      uint8_t payload[29];
    };
    static_assert(sizeof(DiscoveryPacket) == 32, "DiscoveryPacket size must be 32 bytes");

    struct __attribute__((packed)) DiscoveryAckPacket
    {
      uint8_t checksum : 4;
      uint8_t packet_type : 4;
      uint8_t source_node_id;
      uint8_t num_valid_neighbors;
      uint8_t neighbors[29];
    };
    static_assert(sizeof(DiscoveryAckPacket) == 32, "DiscoveryAckPacket size must be 32 bytes");

    struct __attribute__((packed)) TimeSynchPacket
    {
      uint8_t checksum : 4;
      uint8_t packet_type : 4;
      uint8_t source_node_id;
      uint64_t time_sending_left;
      uint8_t padding[22];
    };
    static_assert(sizeof(TimeSynchPacket) == 32, "TimeSynchPacket size must be 32 bytes");

    struct PacketFrame
    {
      uint8_t packet_type;
      uint64_t last_time_sent;
      uint32_t remote_pipe_address;
      uint8_t data[32];
    };
#pragma endregion

    std::deque<PacketFrame> packets_to_send_;

    void SetNodeId(uint8_t node_id);

    void SetRadioState(RadioState state);
    void SetCommsState(CommsState state);
    // The last time the radio was put into a listening state, used in the continuous sender receiver
    uint64_t continuous_comms_last_change_time_us_ = 0;

    // This function will listen for atleast listen_time_us_ before sending three packets from packets_to_send_;
    void ContinuousSenderReceiver();

    void Sender();
    void Receiver();

    void DiscoveryTask();

    void TimingTask();

    void HandleDiscoveryPacket(const DiscoveryPacket &packet);
    void HandleDiscoveryAckPacket(const DiscoveryAckPacket &packet);
    void HandleNodeIdAnnouncementPacket(const DiscoveryPacket &packet);

    void SendNodeIdAnnouncement();

    void ReceiveFromDownstream(const std::vector<uint8_t> &data) override {}
    void ReceiveFromUpstream(const std::vector<uint8_t> &data) override;

    void Reset() override;

    void InsertChecksum(GenericPacket &packet);
    bool ValidateChecksum(GenericPacket &packet);
    uint8_t CalculateChecksum(GenericPacket &packet);
  };

} // namespace nerfnet

#endif // NERFNET_NET_MESH_RADIO_INTERFACE_H_
