#ifndef MESSAGE_FRAGMENTATION_LAYER_H
#define MESSAGE_FRAGMENTATION_LAYER_H

#include <vector>
#include <cstdint>
#include <unordered_map>
#include <functional>
#include "ILayer.h"
#include "message_definitions.h"
// Responsible for splitting up messages into packets for transmission
class MessageFragmentationLayer : public ILayer{
public:
    MessageFragmentationLayer();
    
    void ReceiveFromDownstream(const std::vector<uint8_t>& data) override;
    void ReceiveFromUpstream(const std::vector<uint8_t>& data) override;

private:
    uint8_t packet_number_ = 0;
    std::vector<DataPacket> fragmented_packets_;
};

#endif // MESSAGE_FRAGMENTATION_LAYER_H
