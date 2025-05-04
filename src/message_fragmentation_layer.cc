#include "message_fragmentation_layer.h"
#include <cstring>
#include <iostream>
#include "message_definitions.h"
#include <cmath>
#include "nrftime.h"
// #include <cstdlib>
// #include <ctime>

MessageFragmentationLayer::MessageFragmentationLayer() {
    std::srand(static_cast<unsigned int>(nerfnet::TimeNowUs()));
    packet_number_ = static_cast<uint8_t>(std::rand() % 256);
    LOGI("MessageFragmentationLayer initialized with packet number %d", packet_number_);
}

void MessageFragmentationLayer::ReceiveFromDownstream(const std::vector<uint8_t> &data)
{
    CHECK(data.size() == PACKET_SIZE, "Message Fragment data size must be 32 bytes");
    DataPacket packet = VectorToDataPacket(data);
    fragmented_packets_.push_back(packet);
    //LOGI("MessageFragmentationLayer Received packet %d with size %zu, final: %d", packet.payload[10], packet.valid_bytes, packet.final_packet);
    if(packet.final_packet) {
        //LOGI("MessageFragmentationLayer Received final packet with %zu bytes", packet.valid_bytes);
        std::vector<uint8_t> payload;

        // Combine all fragmented packets
        for (const auto &frag_packet : fragmented_packets_) {
            INCREMENT_STATS(&stats, fragments_received);
            payload.insert(payload.end(), frag_packet.payload, frag_packet.payload + frag_packet.valid_bytes);
        }

        fragmented_packets_.clear();

        SendUpstream(payload);
    } else {
        //LOGI("MessageFragmentationLayer Received non-final packet with %zu bytes", packet.valid_bytes);
    }
}

void MessageFragmentationLayer::ReceiveFromUpstream(const std::vector<uint8_t> &data)
{
    // We will split up the message into smaller packets and send them downstream
    //LOGI("MessageFragmentationLayer Received %zu bytes from upstream", data.size());
    int number_of_packets = std::ceil(static_cast<double>(data.size()) / PACKET_PAYLOAD_SIZE);
    ///LOGI("MessageFragmentationLayer splitting into %d packets", number_of_packets);

    for (int i = 0; i < number_of_packets; ++i) {
        size_t offset = i * PACKET_PAYLOAD_SIZE;
        size_t packet_size = std::min(static_cast<size_t>(PACKET_PAYLOAD_SIZE), data.size() - offset);
        
        DataPacket packet;
        
        std::vector<uint8_t> payload(data.begin() + offset, data.begin() + offset + packet_size);
        std::memcpy(packet.payload, payload.data(), packet_size);
        
        packet.valid_bytes = packet_size;

        if(i == number_of_packets - 1) {
            packet.final_packet = true;
        } else {
            packet.final_packet = false;
        }
        //packet.payload[10] = packet_number_++;
        //LOGI("Pushing Packet %d with size %zu, final: %d, num: %d", i, packet.valid_bytes, packet.final_packet, packet.payload[10]);
        INCREMENT_STATS(&stats, fragments_sent);
        SendDownstream(DataPacketToVector(packet));
    }
}
