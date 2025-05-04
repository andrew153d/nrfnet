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

#ifndef NERFNET_NET_RADIO_INTERFACE_H_
#define NERFNET_NET_RADIO_INTERFACE_H_

#include <atomic>
#include <deque>
#include <mutex>
#include <optional>
#include <RF24/RF24.h>
#include <thread>
#include <vector>


namespace nerfnet {

// The interface to send/receive data using an RF24 radio.
class RadioInterface {
 public:
  // Setup the radio interface.
  RadioInterface(uint16_t ce_pin, int tunnel_fd,
                 uint32_t primary_addr, uint32_t secondary_addr,
                 uint8_t channel);
  ~RadioInterface();

  // The possible results of a request operation.
  enum class RequestResult {
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

 protected:
  // The number of microseconds to poll over.
  static constexpr uint32_t kPollIntervalUs = 1000;

  // The maximum size of a packet.
  static constexpr size_t kMaxPacketSize = 32;
  static constexpr size_t kMaxPayloadSize = kMaxPacketSize - 2;

  // The default pipe to use for sending data.
  static constexpr uint8_t kPipeId = 1;

  // The mask for IDs.
  static constexpr uint8_t kIDMask = 0x0f;

  // A tunnel Tx/Rx request exchanged between systems.
  struct TunnelTxRxPacket {
    std::optional<uint8_t> id;
    std::optional<uint8_t> ack_id;

    uint8_t bytes_left = 0;
    std::vector<uint8_t> payload;
  };

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
  std::mutex read_buffer_mutex_;
  std::deque<std::vector<uint8_t>> read_buffer_;

  // The frame buffer for the currently incoming frame. Written out to
  // the tunnel interface when completely received.
  std::vector<uint8_t> frame_buffer_;

  // The next ID for packet ID generation.
  uint8_t next_id_;

  // The last ID that needs to be acknowledged.
  std::optional<uint8_t> last_ack_id_;

  // Whether to log successful tunnel read/write operations.
  bool tunnel_logs_enabled_;

  // Sends a message over the radio.
  RequestResult Send(const std::vector<uint8_t>& request);

  // Reads a message from the radio.
  RequestResult Receive(std::vector<uint8_t>& response,
                        uint64_t timeout_us = 0);

  // Returns the size of the read buffer.
  size_t GetReadBufferSize();

  // Returns the size of the next payload to send.
  size_t GetTransferSize(const std::vector<uint8_t>& frame);

  // Advances the packet ID counter.
  void AdvanceID();

  // Returns true if the supplied ID is the next ID.
  bool ValidateID(uint8_t id);

  // Reads from the tunnel and buffers data read.
  void TunnelThread();

  // Encode/decode functions for TunnelTxRxPackets.
  bool DecodeTunnelTxRxPacket(const std::vector<uint8_t>& request,
      TunnelTxRxPacket& tunnel);
  bool EncodeTunnelTxRxPacket(const TunnelTxRxPacket& tunnel,
      std::vector<uint8_t>& request);

  // Writes the current frame buffer to the tunnel.
  void WriteTunnel();
};

}  // namespace nerfnet

#endif  // NERFNET_NET_RADIO_INTERFACE_H_
