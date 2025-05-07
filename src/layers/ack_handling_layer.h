#ifndef ACK_LAYER_H
#define ACK_LAYER_H

#include <vector>
#include <cstdint>
#include <unordered_map>
#include <functional>
#include "ILayer.h"
#include "message_definitions.h"
// Responsible for splitting up messages into packets for transmission
class AckLayer : public ILayer{
public:
    AckLayer(uint32_t packet_queue_size);
    
    void Run();

    void ReceiveFromDownstream(const std::vector<uint8_t>& data) override;
    void ReceiveFromUpstream(const std::vector<uint8_t>& data) override;
    void Reset() override;

    void Enable(bool enabled)
    {
        enabled_ = enabled;
    }
private:
    uint32_t max_number_of_packets_ = 1;
    bool enabled_ = true;
    struct AckPacket
    {
        DataPacket packet;
        uint64_t last_time_sent_;
        uint32_t times_sent_;
    };

    std::deque<DataPacket> fragmented_packets_;
    std::vector<AckPacket> pending_packets_;

    uint8_t packet_number_ = 0;
};

#endif // ACK_LAYER_H
