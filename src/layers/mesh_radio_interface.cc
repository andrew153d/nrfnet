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
#include <algorithm>
#include "message_definitions.h"
namespace nerfnet
{

  MeshRadioInterface::MeshRadioInterface(
      uint16_t ce_pin, int tunnel_fd,
      uint32_t primary_addr, uint32_t secondary_addr, uint8_t channel,
      uint64_t poll_interval_us,
      uint32_t discovery_address,
      uint8_t power_level,
      bool lna,
      uint8_t data_rate)
      : radio_(ce_pin, 0),
        ce_pin_(ce_pin),
        channel_(channel)
  {

    CHECK(channel_ < 128, "Channel must be between 0 and 127");
    CHECK(radio_.begin(), "Failed to start NRF24L01");

    radio_.setChannel(channel_);
    radio_.setPALevel(power_level, lna);
    radio_.setDataRate((rf24_datarate_e)data_rate);
    radio_.setAddressWidth(3);
    radio_.enableDynamicPayloads();
    radio_.enableAckPayload();
    radio_.setAutoAck(false);
    radio_.setRetries(0, 0);
    radio_.setCRCLength(RF24_CRC_8);

    CHECK(radio_.isChipConnected(), "NRF24L01 is unavailable");

    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    node_id_ = min_discovery_node_id_ + (std::rand() % (256 - min_discovery_node_id_));

    LOGI("Starting mesh radio interface with node id %d | 0x%X", node_id_, node_id_);

    for (int i = 0; i < 5; i++)
    {
      reading_pipe_addresses_[i] = 0;
    }

    reading_pipe_addresses_[0] = base_address_ + discovery_address_offset_;
    reading_pipe_addresses_[1] = base_address_ + (node_id_ << 8) + 0x01;

    LOGI("Discovery address: 0x%X", reading_pipe_addresses_[0]);
    LOGI("Secondary address: 0x%X", reading_pipe_addresses_[1]);

    radio_.openReadingPipe(0, reading_pipe_addresses_[0]);
    radio_.openReadingPipe(1, reading_pipe_addresses_[1]);

    radio_.flush_rx();
    radio_.flush_tx();

    SetRadioState(RadioState::Continuous);
    SetCommsState(CommsState::Discovery);
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

  void MeshRadioInterface::SetNodeId(uint8_t node_id)
  {
    radio_.stopListening();
    SleepUs(1000);
    node_id_ = node_id;
    SendNodeIdAnnouncement();
    writing_pipe_address_ = 0;
    LOGI("Opening reading pipes");
    for (int i = 1; i < 6; i++)
    {
      reading_pipe_addresses_[i] = base_address_ + (node_id_ << 8) + i;
      radio_.openReadingPipe(i, reading_pipe_addresses_[i]);
      // LOGI("Opened reading pipe %d: 0x%X", i, reading_pipe_addresses_[i]);
    }
    SleepUs(1000);
    radio_.startListening();
  }

  void MeshRadioInterface::SetRadioState(RadioState state)
  {
    if (state == radio_state)
      return;
    last_state_change_time_ = TimeNowUs();
    switch (state)
    {
    case RadioNone:
      LOGI("Setting radio state to RadioNone");
      break;
    case Listening:
      // LOGI("Setting radio state to Listening");
      break;
    case Sending:
      // LOGI("Setting radio state to Sending");
      break;
    case Continuous:
      LOGI("Setting radio state to Continuous");
      break;
    default:
      CHECK(false, "Unknown comms state");
      break;
    }
    radio_state = state;
  }

  void MeshRadioInterface::SetCommsState(CommsState state)
  {
    if (state == comms_state_)
      return;
    last_state_change_time_ = TimeNowUs();
    switch (state)
    {
    case CommsNone:
      LOGI("Setting comms state to CommsNone");
      break;
    case Timing:
      LOGI("Setting comms state to Timing");
      break;
    case Discovery:
      LOGI("Setting comms state to Discovery");
      break;
    case Running:
      LOGI("Setting comms state to Running");
      break;
    default:
      CHECK(false, "Unknown comms state");
      break;
    }
    comms_state_ = state;
  }

  void MeshRadioInterface::Run()
  {
    switch (radio_state)
    {
    case Listening:
      Receiver();
      break;
    case Sending:
      Sender();
      break;
    case Continuous:
      ContinuousSenderReceiver();
      break;
    case RadioNone:
      // Do nothing
      break;
    default:
      CHECK(false, "Unknown radio state");
      break;
    };

    switch (comms_state_)
    {
    case Timing:
      TimingTask();
      break;
    case Discovery:
      DiscoveryTask();
      break;
    case Running:
      break;
    case CommsNone:
      // Do nothing
      break;
    default:
    {
      CHECK(false, "Unknown comms state");
    }
    break;
    };
  }

  void MeshRadioInterface::TimingTask()
  {
    if (TimeNowUs() - last_state_change_time_ > 5000000) // Wait for 3 seconds without a timing packet
    {
      LOGW("No timing messages received, going to discovery state");
      SetCommsState(Discovery);
      SetRadioState(Listening);
    }

    if (TimeNowUs() - discovery_message_timer_ > 1000000)
    {
      discovery_message_timer_ = TimeNowUs();
      radio_.stopListening();
      radio_.openWritingPipe(reading_pipe_addresses_[0]);
      radio_.flush_tx();
      radio_.flush_rx();
      TimeSynchPacket time_synch_packet;
      std::memset(&time_synch_packet, 0, sizeof(time_synch_packet));
      time_synch_packet.packet_type = static_cast<uint8_t>(PacketType::TimeSynch);
      time_synch_packet.source_node_id = node_id_;
      time_synch_packet.time_sending_left = 0;
      time_synch_packet.checksum = CalculateChecksum(*reinterpret_cast<GenericPacket *>(&time_synch_packet));
      radio_.writeFast(reinterpret_cast<uint8_t *>(&time_synch_packet), sizeof(time_synch_packet));
      radio_.txStandBy();
      radio_.startListening();
    }
    if (radio_.available())
    {
      uint8_t buffer[32];
      radio_.read(buffer, sizeof(buffer));
      // Process the received packet
      GenericPacket *received_packet = reinterpret_cast<GenericPacket *>(buffer);
      if (!ValidateChecksum(*received_packet))
      {
        LOGE("Invalid checksum");
        radio_.flush_rx();
        return;
      }
      // Process the received packet
      switch (received_packet->packet_type)
      {
      case static_cast<uint8_t>(PacketType::TimeSynch):
        LOGI("Received timing packet");
        break;
      case static_cast<uint8_t>(PacketType::TimeSynchAck):
      {
        TimeSynchPacket *time_synch_ack_packet = reinterpret_cast<TimeSynchPacket *>(buffer);
        LOGI("Received timing ack packet with time of %llu", time_synch_ack_packet->time_sending_left);
        SetCommsState(Discovery);
        // Other radio is listening right now
        SetRadioState(Sending);
        // This should align the send and receive time slots for the two devices
        last_state_change_time_ = TimeNowUs() + time_synch_ack_packet->time_sending_left - send_receive_period_us_;
        break;
      }
      default:
        LOGW("Timing Handler received unknown packet type");
        break;
      }
    }
  }

  void MeshRadioInterface::DiscoveryTask()
  {
    if (TimeNowUs() - discovery_message_timer_ > discovery_message_rate_us_ && comms_state_ == Discovery)
    {
      discovery_message_timer_ = TimeNowUs();

      if (number_of_discovery_messages_sent_ > max_discovery_messages_ && discovery_ack_received_time_us_ == 0)
      {
        LOGI("No neighbors found, setting up node id to 0");
        SetNodeId(0);
        SetRadioState(Listening);
        SetCommsState(Running);
        return;
      }

      PacketFrame packet;
      packet.remote_pipe_address = reading_pipe_addresses_[0]; // pipe 0 is used for discovery
      DiscoveryPacket *discovery_packet = reinterpret_cast<DiscoveryPacket *>(&packet.data[0]);
      std::memset(discovery_packet, 0, sizeof(DiscoveryPacket));
      discovery_packet->packet_type = static_cast<uint8_t>(PacketType::Discovery);
      discovery_packet->source_node_id = node_id_;
      InsertChecksum(*reinterpret_cast<GenericPacket *>(discovery_packet));
      packets_to_send_.emplace_back(packet);
      number_of_discovery_messages_sent_++;
    }

    if (discovery_ack_received_time_us_ != 0)
    {
      if (TimeNowUs() - discovery_ack_received_time_us_ > discovery_ack_timeout_us_)
      {
        LOGI("Done listening for neighbors");
        // Look for next available node id to assign, making sure to not assign one in the neighbor list
        for (int i = 0; i < min_discovery_node_id_; i++)
        {
          if (std::find(neighbor_node_ids_.begin(), neighbor_node_ids_.end(), i) == neighbor_node_ids_.end())
          {
            SetNodeId(i);
            LOGI("Setting up node id to 0x%X", node_id_);
            discovery_ack_received_time_us_ = 0;
            SetRadioState(Listening);
            SetCommsState(Running);
            return;
          }
        }

        CHECK(false, "No available node ids to assign");
      }
    }
  }

  void MeshRadioInterface::HandleDiscoveryPacket(const DiscoveryPacket &packet)
  {
    LOGI("Received discovery packet from 0x%X", packet.source_node_id);

    if (comms_state_ == Discovery)
    {
      if (packet.source_node_id == node_id_)
      {
        // LOGI("Received discovery from self, ignoring");
        return;
      }
      if (packet.source_node_id < node_id_)
      {
        // LOGI("Received discovery from node 0x%X, but this node is lower than me, resetting discovery counter", packet.source_node_id);
        discovery_message_timer_ = 0;
        number_of_discovery_messages_sent_ = 0;
        return;
      }
      return;
    }

    // Send back a packets with neightbor node ids, split them up between multiple packets if needed
    PacketFrame packet_frame;
    packet_frame.remote_pipe_address = base_address_ + (packet.source_node_id << 8) + 0x01; // send to pipe one
    DiscoveryAckPacket *ack_packet = reinterpret_cast<DiscoveryAckPacket *>(&packet_frame.data[0]);
    std::memset(ack_packet, 0, sizeof(DiscoveryAckPacket));
    ack_packet->packet_type = static_cast<uint8_t>(PacketType::DiscoverResponse);
    ack_packet->source_node_id = node_id_;
    ack_packet->num_valid_neighbors = neighbor_node_ids_.size();
    int i = 0;
    for (auto it = neighbor_node_ids_.begin(); it != neighbor_node_ids_.end(); ++it)
    {
      if (i < 29)
      {
        ack_packet->neighbors[i] = *it;
        i++;
      }
      else
      {
        LOGW("Too many neighbors to send in one packet, splitting up");
        break;
      }
    }
    InsertChecksum(*reinterpret_cast<GenericPacket *>(ack_packet));
    // LOGI("Sending discovery ack packet to 0x%X", packet_frame.remote_pipe_address);
    packets_to_send_.emplace_back(packet_frame);
    return;
  }

  void MeshRadioInterface::HandleDiscoveryAckPacket(const DiscoveryAckPacket &packet)
  {
    LOGI("Received %d neighbors from 0x%X", packet.num_valid_neighbors, packet.source_node_id);

    if (discovery_ack_received_time_us_ == 0)
    {
      discovery_ack_received_time_us_ = TimeNowUs();
    }

    // Add the node ids to the neighbor list
    neighbor_node_ids_.insert(packet.source_node_id);
    for (int i = 0; i < packet.num_valid_neighbors; i++)
    {
      neighbor_node_ids_.insert(packet.neighbors[i]);
    }

    return;
  }

  void MeshRadioInterface::HandleNodeIdAnnouncementPacket(const DiscoveryPacket &packet)
  {
    LOGI("Received node id announcement packet from 0x%X", packet.source_node_id);
    if (packet.source_node_id == node_id_)
    {
      LOGI("Received node id announcement from self, ignoring");
      return;
    }
    neighbor_node_ids_.insert(packet.source_node_id);
    LOGI("Added node id 0x%X to neighbor list", packet.source_node_id);
  }

  void MeshRadioInterface::SendNodeIdAnnouncement()
  {
    PacketFrame packet;
    // Send to the discovery address
    packet.remote_pipe_address = base_address_ + discovery_address_offset_; // pipe 0 is used for discovery
    DiscoveryPacket *discovery_packet = reinterpret_cast<DiscoveryPacket *>(&packet.data[0]);
    std::memset(discovery_packet, 0, sizeof(DiscoveryPacket));
    discovery_packet->packet_type = static_cast<uint8_t>(PacketType::NodeIdAnnouncement);
    discovery_packet->source_node_id = node_id_;
    InsertChecksum(*reinterpret_cast<GenericPacket *>(discovery_packet));
    packets_to_send_.emplace_back(packet);
  }

  void MeshRadioInterface::Receiver()
  {
    if (nerfnet::TimeNowUs() - last_state_change_time_ > send_receive_period_us_)
    {
      SetRadioState(Sending);
      return;
    }

    if (radio_.available())
    {
      GenericPacket received_packet;
      std::memset(&received_packet, 0, sizeof(received_packet));
      INCREMENT_STATS(&stats, radio_packets_received);
      radio_.read(reinterpret_cast<uint8_t *>(&received_packet), 32);

      if (!ValidateChecksum(received_packet))
      {
        LOGE("Invalid checksum");
        radio_.flush_rx();
        return;
      }

      switch ((PacketType)received_packet.packet_type)
      {
      case PacketType::Discovery:
        HandleDiscoveryPacket(*reinterpret_cast<DiscoveryPacket *>(&received_packet));
        break;
      case PacketType::DiscoverResponse:
        HandleDiscoveryAckPacket(*reinterpret_cast<DiscoveryAckPacket *>(&received_packet));
        break;
      case PacketType::Data:
      case PacketType::DataAck:
      {
        DataPacket *data_packet = reinterpret_cast<DataPacket *>(&received_packet);
        SendUpstream(DataPacketToVector(*data_packet));
        break;
      }
      case PacketType::NodeIdAnnouncement:
        HandleNodeIdAnnouncementPacket(*reinterpret_cast<DiscoveryPacket *>(&received_packet));
        break;
      case PacketType::Status:
      {
        LOGW("Received status packet");
      }
      case PacketType::TimeSynch:
        LOGI("Received time synch packet");
        radio_.stopListening();
        radio_.flush_tx();
        TimeSynchPacket time_synch_ack_packet;
        std::memset(&time_synch_ack_packet, 0, sizeof(time_synch_ack_packet));
        time_synch_ack_packet.packet_type = static_cast<uint8_t>(PacketType::TimeSynchAck);
        time_synch_ack_packet.source_node_id = node_id_;
        time_synch_ack_packet.time_sending_left = (uint64_t)((float)last_state_change_time_ + (float)send_receive_period_us_ - (float)TimeNowUs());
        time_synch_ack_packet.checksum = CalculateChecksum(*reinterpret_cast<GenericPacket *>(&time_synch_ack_packet));
        radio_.writeFast(reinterpret_cast<uint8_t *>(&time_synch_ack_packet), sizeof(time_synch_ack_packet));
        radio_.txStandBy();
        radio_.startListening();
        break;
      case PacketType::TimeSynchAck:
        LOGI("Received time synch ack packet");
        break;
      default:
        LOGE("Unknown packet type: %d", received_packet.packet_type);
        break;
      }
    }
  }

  void MeshRadioInterface::Sender()
  {
    if (nerfnet::TimeNowUs() - last_state_change_time_ > send_receive_period_us_)
    {
      SetRadioState(Listening);
      return;
    }
    if (packets_to_send_.empty())
      return;

    std::optional<PacketFrame> packet1 = std::nullopt;
    std::optional<PacketFrame> packet2 = std::nullopt;
    std::optional<PacketFrame> packet3 = std::nullopt;

    if (!packets_to_send_.empty())
    {
      packet1 = packets_to_send_.front();
      packets_to_send_.pop_front();
    }
    if (!packets_to_send_.empty() && packets_to_send_.front().remote_pipe_address == packet1->remote_pipe_address)
    {
      packet2 = packets_to_send_.front();
      packets_to_send_.pop_front();
    }
    if (!packets_to_send_.empty() && packets_to_send_.front().remote_pipe_address == packet1->remote_pipe_address)
    {
      packet3 = packets_to_send_.front();
      packets_to_send_.pop_front();
    }

    if (!packet1 && !packet2 && !packet3)
    {
      // radio_.startListening();
      continuous_comms_last_change_time_us_ = TimeNowUs();
      return;
    }

    if (packet1->remote_pipe_address != writing_pipe_address_)
    {
      writing_pipe_address_ = packet1->remote_pipe_address;
      radio_.openWritingPipe(writing_pipe_address_);
      LOGI("Opened writing pipe: 0x%X", writing_pipe_address_);
    }
    uint32_t start_time = TimeNowUs();
    radio_.stopListening();
    radio_.flush_tx();
    if (packet1)
    {
      INCREMENT_STATS(&stats, radio_packets_sent);
      radio_.writeFast(packet1->data, 32);
    }
    if (packet2)
    {
      INCREMENT_STATS(&stats, radio_packets_sent);
      radio_.writeFast(packet2->data, 32);
    }
    if (packet3)
    {
      INCREMENT_STATS(&stats, radio_packets_sent);
      radio_.writeFast(packet3->data, 32);
    }

    if (!radio_.txStandBy())
    {
      LOGE("Failed to write packet (timeout)");
      continuous_comms_last_change_time_us_ = TimeNowUs() - 100;
    }
    else
    {
      continuous_comms_last_change_time_us_ = TimeNowUs();
    }

    radio_.startListening();

    uint32_t time_elapsed = continuous_comms_last_change_time_us_ - start_time;
    if (packet1 && packet2 && packet3)
    {
      // LOGI("Sent 3 packets in %d us", time_elapsed);
    }
  }

  void MeshRadioInterface::ContinuousSenderReceiver()
  {

    // Receiver
    if (radio_.available())
    {
      GenericPacket received_packet;
      std::memset(&received_packet, 0, sizeof(received_packet));
      INCREMENT_STATS(&stats, radio_packets_received);
      radio_.read(reinterpret_cast<uint8_t *>(&received_packet), 32);
      if (!ValidateChecksum(received_packet))
      {
        LOGE("Invalid checksum");
        radio_.flush_rx();
        return;
      }
      switch ((PacketType)received_packet.packet_type)
      {
      case PacketType::Discovery:
        HandleDiscoveryPacket(*reinterpret_cast<DiscoveryPacket *>(&received_packet));
        break;
      case PacketType::DiscoverResponse:
        HandleDiscoveryAckPacket(*reinterpret_cast<DiscoveryAckPacket *>(&received_packet));
        break;
      case PacketType::Data:
      case PacketType::DataAck:
      {
        DataPacket *data_packet = reinterpret_cast<DataPacket *>(&received_packet);
        SendUpstream(DataPacketToVector(*data_packet));
        break;
      }
      case PacketType::NodeIdAnnouncement:
        HandleNodeIdAnnouncementPacket(*reinterpret_cast<DiscoveryPacket *>(&received_packet));
        break;
      case PacketType::Status:
      {
        LOGW("Received status packet");
      }
      default:
        LOGE("Unknown packet type: %d", received_packet.packet_type);
        break;
      }
    }
    // Sender
    if (packets_to_send_.empty())
      return;
    if (TimeNowUs() - continuous_comms_last_change_time_us_ < continuous_listen_time_us_)
      return;

    std::optional<PacketFrame> packet1 = std::nullopt;
    std::optional<PacketFrame> packet2 = std::nullopt;
    std::optional<PacketFrame> packet3 = std::nullopt;

    if (!packets_to_send_.empty())
    {
      packet1 = packets_to_send_.front();
      packets_to_send_.pop_front();
    }
    if (!packets_to_send_.empty() && packets_to_send_.front().remote_pipe_address == packet1->remote_pipe_address)
    {
      packet2 = packets_to_send_.front();
      packets_to_send_.pop_front();
    }
    if (!packets_to_send_.empty() && packets_to_send_.front().remote_pipe_address == packet1->remote_pipe_address)
    {
      packet3 = packets_to_send_.front();
      packets_to_send_.pop_front();
    }

    if (!packet1 && !packet2 && !packet3)
    {
      continuous_comms_last_change_time_us_ = TimeNowUs();
      return;
    }

    if (packet1->remote_pipe_address != writing_pipe_address_)
    {
      writing_pipe_address_ = packet1->remote_pipe_address;
      radio_.openWritingPipe(writing_pipe_address_);
      LOGI("Opened writing pipe: 0x%X", writing_pipe_address_);
    }
    uint32_t start_time = TimeNowUs();
    radio_.stopListening();
    radio_.flush_tx();
    if (packet1)
    {
      INCREMENT_STATS(&stats, radio_packets_sent);
      radio_.writeFast(packet1->data, 32);
    }
    if (packet2)
    {
      INCREMENT_STATS(&stats, radio_packets_sent);
      radio_.writeFast(packet2->data, 32);
    }
    if (packet3)
    {
      INCREMENT_STATS(&stats, radio_packets_sent);
      radio_.writeFast(packet3->data, 32);
    }

    if (!radio_.txStandBy())
    {
      LOGE("Failed to write packet (timeout)");
      continuous_comms_last_change_time_us_ = TimeNowUs() - 100;
    }
    else
    {
      continuous_comms_last_change_time_us_ = TimeNowUs();
    }

    radio_.startListening();

    uint32_t time_elapsed = continuous_comms_last_change_time_us_ - start_time;
    if (packet1 && packet2 && packet3)
    {
      // LOGI("Sent 3 packets in %d us", time_elapsed);
    }
  }

  void MeshRadioInterface::ReceiveFromUpstream(const std::vector<uint8_t> &data)
  {
    // return;
    //  LOGI("Mesh Radio Received %zu bytes from upstream", data.size());

    if (!neighbor_node_ids_.empty())
    {
      DataPacket outgoing_packet = VectorToDataPacket(data);
      PacketFrame packet;
      packet.remote_pipe_address = base_address_ + ((*neighbor_node_ids_.begin()) << 8) + 0x01; // send to pipe one
      DataPacket *data_packet = reinterpret_cast<DataPacket *>(&packet.data[0]);
      *data_packet = outgoing_packet;
      CHECK(data_packet->packet_type == (uint8_t)PacketType::Data || data_packet->packet_type == (uint8_t)PacketType::DataAck,
            "Type must be data of ack data");
      // data_packet->packet_type = static_cast<uint8_t>(PacketType::Data);
      //  data_packet->source_id = node_id_;
      InsertChecksum(*reinterpret_cast<GenericPacket *>(data_packet));
      // LOGI("Packet: %d bytes, final: %d", data_packet->valid_bytes, data_packet->final_packet);
      // LOGI("First few bytes of packet: %02X %02X %02X %02X %02X",
      //   packet.data[0], packet.data[1], packet.data[2], packet.data[3], packet.data[4]);
      packets_to_send_.emplace_back(packet);
    }
    else
    {
      LOGE("Neighbor node IDs list is empty. Cannot send data.");
    }
  }

  void MeshRadioInterface::Reset()
  {
    packets_to_send_.clear();
    neighbor_node_ids_.clear();
    discovery_message_timer_ = 0;
    number_of_discovery_messages_sent_ = 0;
    discovery_ack_received_time_us_ = 0;
    SetCommsState(Discovery);
    writing_pipe_address_ = 0;
    for (int i = 0; i < 5; i++)
    {
      reading_pipe_addresses_[i] = 0;
    }
    radio_.stopListening();
    radio_.flush_rx();
    radio_.flush_tx();
    radio_.startListening();
  }

  void MeshRadioInterface::InsertChecksum(GenericPacket &packet)
  {
    packet.checksum = CalculateChecksum(packet);
  }

  bool MeshRadioInterface::ValidateChecksum(GenericPacket &packet)
  {
    return packet.checksum == CalculateChecksum(packet);
  }

  uint8_t MeshRadioInterface::CalculateChecksum(GenericPacket &packet)
  {
    int checksum_bit_length = 4;
    uint8_t *packet_ptr = reinterpret_cast<uint8_t *>(&packet);
    int checksum = 0;
    checksum += (packet_ptr[0] >> 4) & 0x0F;
    for (int i = 1; i < sizeof(GenericPacket); i++)
    {
      checksum += packet_ptr[i] & 0x0F;
      checksum += (packet_ptr[i] >> 4) & 0x0F;
    }
    checksum = checksum % (1 << checksum_bit_length);
    return checksum;
  };
} // namespace nerfnet
