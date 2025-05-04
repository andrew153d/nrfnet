#ifndef MESSAGE_DEFINITIONS_H
#define MESSAGE_DEFINITIONS_H

#include <cstdint>
#include <vector>
#include "log.h"
// Define message types and structures here
#define PACKET_SIZE 32

#define PACKET_HEADER_SIZE 2
#define PACKET_PAYLOAD_SIZE 30
static_assert(PACKET_HEADER_SIZE + PACKET_PAYLOAD_SIZE == PACKET_SIZE, "Header plus payload size must be 32 bytes");

#define PACKET_CHECKSUM_SIZE_BITS 4
#define PACKET_TYPE_SIZE_BITS 4
#define PACKET_VALID_BYTES_BITS 5
#define FINAL_PACKET_SIZE_BITS 1

union DataPacket
{
    struct
    {
        uint8_t checksum : PACKET_CHECKSUM_SIZE_BITS;
        uint8_t packet_type : PACKET_TYPE_SIZE_BITS;
        uint8_t valid_bytes : PACKET_VALID_BYTES_BITS;
        bool final_packet : FINAL_PACKET_SIZE_BITS;
        uint8_t padding: 2;
        uint8_t payload[PACKET_PAYLOAD_SIZE];
    };
    uint8_t raw_data[PACKET_SIZE];
};
    static_assert(sizeof(DataPacket) == PACKET_SIZE, "DataPacket size must be 32 bytes");



//Converts vector to DataPacket
inline DataPacket VectorToDataPacket(const std::vector<uint8_t>& data) {
    CHECK(data.size() == PACKET_SIZE, "Data size must be 32 bytes");
    DataPacket packet;
    std::copy(data.begin(), data.end(), packet.raw_data);
    return packet;
}
// Converts DataPacket to vector
inline std::vector<uint8_t> DataPacketToVector(const DataPacket& packet) {
    std::vector<uint8_t> data(PACKET_SIZE);
    std::copy(packet.raw_data, packet.raw_data + PACKET_SIZE, data.begin());
    return data;
}
#endif // MESSAGE_DEFINITIONS_H
