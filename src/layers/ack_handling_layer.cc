#include "ack_handling_layer.h"
#include <cstring>
#include <iostream>
#include "message_definitions.h"
#include <cmath>
#include "nrftime.h"
// #include <cstdlib>
// #include <ctime>

AckLayer::AckLayer(uint32_t packet_queue_size)
{
    max_number_of_packets_ = packet_queue_size;
    // Initialize the random number generator
    std::srand(static_cast<unsigned int>(nerfnet::TimeNowUs()));
    packet_number_ = static_cast<uint8_t>(std::rand() % 256);
    LOGI("AckLayer initialized with packet number: %d", packet_number_);
}

void AckLayer::ReceiveFromDownstream(const std::vector<uint8_t> &data)
{
    if (!enabled_)
    {
        SendUpstream(data);
        return;
    }
    DataPacket packet = VectorToDataPacket(data);
    switch (packet.packet_type)
    {
    case static_cast<uint8_t>(PacketType::Data):
    {
        // Handle data packet
        LOGI("Received packet %d", packet.number);
        SendUpstream(data);
        // Send an ack packet back
        DataPacket ack_packet = packet;
        ack_packet.packet_type = static_cast<uint8_t>(PacketType::DataAck);
        INCREMENT_STATS(&stats, ack_messages_received);
        SendDownstream(DataPacketToVector(ack_packet));
        break;
    }
    case static_cast<uint8_t>(PacketType::DataAck):
    {
        // Handle ack packet
        LOGI("Received ack packet for packet %d", packet.number);

        auto it = std::find_if(pending_packets_.begin(), pending_packets_.end(),
                               [&packet](const AckPacket &pending)
                               {
                                   return pending.packet.valid_bytes == packet.valid_bytes &&
                                          std::memcmp(pending.packet.payload, packet.payload, packet.valid_bytes) == 0;
                               });

        if (it != pending_packets_.end())
        {
            pending_packets_.erase(it);
        }
        else
        {
            LOGW("No matching packet found in pending queue for ack");
        }
        break;
    }
    default:
        LOGE("Unknown ack packet type: %d", packet.packet_type);
        break;
    }
}

void AckLayer::ReceiveFromUpstream(const std::vector<uint8_t> &data)
{
    if (!enabled_)
    {
        SendDownstream(data);
        return;
    }
    fragmented_packets_.emplace_back(VectorToDataPacket(data));
}

void AckLayer::Run()
{
    if (!fragmented_packets_.empty() && pending_packets_.size() < max_number_of_packets_)
    {
        // put another packet in the pending queue
        DataPacket packet = fragmented_packets_.front();
        fragmented_packets_.pop_front();
        AckPacket ack_packet;
        ack_packet.packet = packet;
        ack_packet.packet.number = packet_number_++;
        LOGI("Adding packet %d to pending queue", ack_packet.packet.number);
        INCREMENT_STATS(&stats, ack_messages_sent);
        SendDownstream(DataPacketToVector(ack_packet.packet));
        ack_packet.last_time_sent_ = nerfnet::TimeNowUs();
        ack_packet.times_sent_ = 1;
        pending_packets_.push_back(ack_packet);
    }

    // Send the pending packets
    for (auto it = pending_packets_.begin(); it != pending_packets_.end();)
    {
        if (it->times_sent_ > 10)
        {
            LOGE("Packet failed to send after 3 attempts, dropping");
            pending_packets_.erase(it); // Update iterator after erase
            return;
        }

        if (nerfnet::TimeNowUs() - it->last_time_sent_ > 20000) // 1 second
        {
            LOGI("Sending packet %d", it->packet.number);
            INCREMENT_STATS(&stats, ack_messages_resent);
            SendDownstream(DataPacketToVector(it->packet));
            it->last_time_sent_ = nerfnet::TimeNowUs();
            it->times_sent_++;
        }
        ++it;
    }
}
