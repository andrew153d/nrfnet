#include "ack_handling_layer.h"
#include <iostream>
#include <chrono>
#include <thread>

AckHandlingLayer::AckHandlingLayer(ILayer* upstream, ILayer* downstream)
    : upstream_(upstream), downstream_(downstream) {}

void AckHandlingLayer::SendMessageWithAck(const std::vector<uint8_t>& message) {
    uint32_t message_id = GenerateMessageId();
    PendingMessage pending_message = {message, message_id, 0};
    pending_messages_[message_id] = pending_message;

    // Simulate sending the message downstream
    std::vector<uint8_t> packet = message;
    packet.insert(packet.begin(), static_cast<uint8_t>(message_id & 0xFF)); // Add message ID
    //downstream_->Send(packet);

    // Retry logic (simplified for demonstration)
    while (pending_messages_.find(message_id) != pending_messages_.end() && pending_message.retry_count < 3) {
        std::this_thread::sleep_for(std::chrono::seconds(1)); // Wait before retrying
        //downstream_->Send(packet);
        pending_message.retry_count++;
    }

    if (pending_messages_.find(message_id) != pending_messages_.end()) {
        std::cerr << "Message ID " << message_id << " failed to be acknowledged after retries." << std::endl;
        pending_messages_.erase(message_id);
    }
}

void AckHandlingLayer::ProcessAck(const std::vector<uint8_t>& ack) {
    if (ack.empty()) return;

    uint32_t message_id = ack[0]; // Extract message ID from the acknowledgment
    if (pending_messages_.find(message_id) != pending_messages_.end()) {
        pending_messages_.erase(message_id);
        if (ack_callback_) {
            ack_callback_(message_id);
        }
    }
}

void AckHandlingLayer::SetAckCallback(std::function<void(uint32_t)> callback) {
    ack_callback_ = callback;
}

uint32_t AckHandlingLayer::GenerateMessageId() {
    static uint32_t current_id = 0;
    return ++current_id;
}
