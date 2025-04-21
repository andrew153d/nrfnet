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

#ifndef NERFNET_NET_COMMON_RADIO_INTERFACE_H_
#define NERFNET_NET_COMMON_RADIO_INTERFACE_H_

#include <optional>
#include "nerfnet/net/radio_interface.h"

#include <atomic>
#include <deque>
#include <mutex>
#include <optional>
#include <RF24/RF24.h>
#include <thread>
#include <vector>

#include "nerfnet/util/non_copyable.h"

namespace nerfnet
{

  // The primary mode radio interface.
  class CommonRadioInterface
  {
  public:
    // Setup the common radio link.
    CommonRadioInterface(uint8_t device_id,
                         uint16_t ce_pin, int tunnel_fd,
                         uint32_t primary_addr, uint32_t secondary_addr,
                         uint8_t channel, uint64_t poll_interval_us);

    ~CommonRadioInterface();
    // The possible results of a request operation.
    enum class RequestResult
    {
      // The request was successful.
      Success,

      // The request timed out.
      Timeout,

      // The request could not be sent because it was malformed.
      Malformed,

      // There was an error transmitting the request.
      TransmitError,
    };

    void SetTunnelLogsEnabled(bool enabled) { tunnel_logs_enabled_ = enabled; }

    // Runs the interface.
    void Run();

  private:
    // The number of microseconds to poll over.
    static constexpr uint32_t kPollIntervalUs = 1000;

    // The maximum size of a packet.
    static constexpr size_t kMaxPacketSize = 32;
    static constexpr size_t kMaxPayloadSize = kMaxPacketSize - 2;

    // The default pipe to use for sending data.
    static constexpr uint8_t kPipeId = 1;

    // The mask for IDs.
    static constexpr uint8_t kIDMask = 0x0f;

    // Radio Packet types
    enum packet_type
    {
      UNKNOWN,
      DISCOVERY,
      DISCOVERY_ACK,
      DATA,
      DATA_ACK,
    };
// Radio Packet
#pragma pack(push, 1)
    struct RadioPacket
    {
      uint8_t type : 3;     // packet type
      uint16_t id: 7;      // packet number, used to make sure packets arrive in order
      uint8_t size: 5;      // number of valid bytes in the payload
      bool final_packet: 1; //the final packet in a network frame 
      uint8_t payload[30];  //the bytes
    };
#pragma pack(pop)
    static_assert(sizeof(RadioPacket) == 32, "RadioPacket size must be 32 bytes");

    // A tunnel Tx/Rx request exchanged between systems.
    struct RadioFrames
    {
      RadioPacket packet;
      uint8_t times_sent;
      uint64_t last_time_sent;
    };

    // device id
    uint8_t device_id_;

    // The underlying radio.
    RF24 radio_;

    // The file descriptor for the network tunnel.
    const int tunnel_fd_;

    // The addresses to use for this radio pair.
    const uint32_t primary_addr_;
    const uint32_t secondary_addr_;

    // The thread to read from the tunnel interface on.
    std::thread tunnel_thread_;
    std::atomic<bool> running_;

    // The buffer of data read and lock.
    std::mutex pending_network_frame_buffer_mutex_;

    // The buffer of data read from the tunnel interface that will need to be divided and sent across the radio
    std::deque<std::vector<uint8_t>> pending_network_frame_buffer_;

    // The frame buffer for the currently incoming frame. Written out to
    // the tunnel interface when completely received.
    std::vector<uint8_t> frame_buffer_;

    // The buffer of data read from the tunnel interface that has been split into Radio Frames and needs to be sent
    std::deque<RadioFrames> pending_radio_frame_buffer_;

    // The next ID for packet ID generation.
    uint8_t next_id_;

    // Whether to log successful tunnel read/write operations.
    bool tunnel_logs_enabled_;

    // Sends a message over the radio.
    RequestResult RadioSend(const std::vector<uint8_t> &request);

    // Reads a message from the radio.
    RequestResult RadioReceive(uint8_t *response,
                               uint8_t *pipe,
                               uint64_t timeout_us = 0);

    // Returns the size of the read buffer.
    size_t GetReadBufferSize();

    // Returns the size of the next payload to send.
    size_t GetTransferSize(const std::vector<uint8_t> &frame);

    // Advances the packet ID counter.
    void AdvanceID();

    // Returns true if the supplied ID is the next ID.
    bool ValidateID(uint8_t id);

    // Reads from the tunnel and buffers data read.
    void TunnelThread();

    // Writes the current frame buffer to the tunnel.
    void WriteTunnel();

    // The interval between poll operations to the secondary radio.
    const uint64_t poll_interval_us_;

    // Logic for poll backoff when the secondary radio is not responding.
    int poll_fail_count_;
    uint64_t current_poll_interval_us_;
    bool connection_reset_required_;

    // Requests that a new connection be opened.
    bool ConnectionReset();

    // Sends and receives messages to exchange network packets.
    bool PerformTunnelTransfer();

    // Updates the backoff configuration in the light of a failure.
    void HandleTransactionFailure();
  };

} // namespace nerfnet

#endif // NERFNET_NET_PRIMARY_RADIO_INTERFACE_H_
