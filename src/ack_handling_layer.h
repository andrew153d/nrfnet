#ifndef ACK_HANDLING_LAYER_H
#define ACK_HANDLING_LAYER_H

#include <vector>
#include <cstdint>
#include <unordered_map>
#include "ILayer.h"

class AckHandlingLayer : public ILayer {
public:
    AckHandlingLayer(ILayer* upstream, ILayer* downstream);

    // Sends a message and waits for an acknowledgment
    void SendMessageWithAck(const std::vector<uint8_t>& message);

    // Processes a received acknowledgment
    void ProcessAck(const std::vector<uint8_t>& ack);

    // Sets the callback to notify when a message is acknowledged
    void SetAckCallback(std::function<void(uint32_t)> callback);

private:
    struct PendingMessage {
        std::vector<uint8_t> message;
        uint32_t message_id;
        size_t retry_count;
    };

    std::unordered_map<uint32_t, PendingMessage> pending_messages_;
    uint32_t GenerateMessageId();

    ILayer* upstream_;
    ILayer* downstream_;
    std::function<void(uint32_t)> ack_callback_;
};

#endif // ACK_HANDLING_LAYER_H
