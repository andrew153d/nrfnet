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

#include "nerfnet/net/common_radio_interface.h"

#include <unistd.h>

#include "nerfnet/util/log.h"
#include "nerfnet/util/macros.h"
#include "nerfnet/util/time.h"
#include <sstream>
#include <iomanip>
#include <stdio.h>
#include <iostream>

namespace nerfnet

{
  // CommonRadioInterface(ce_pin, tunnel_fd, primary_addr, secondary_addr, channel)
  CommonRadioInterface::CommonRadioInterface(
      uint8_t device_id,
      uint16_t ce_pin, int tunnel_fd,
      uint32_t primary_addr, uint32_t secondary_addr, uint8_t channel,
      uint64_t poll_interval_us)
      : device_id_(device_id),
        radio_(ce_pin, 0),
        tunnel_fd_(tunnel_fd),
        primary_addr_(primary_addr),
        secondary_addr_(secondary_addr),
        // tunnel_thread_(&CommonRadioInterface::TunnelThread, this),
        next_id_(1),
        tunnel_logs_enabled_(true),
        poll_interval_us_(poll_interval_us),
        current_poll_interval_us_(poll_interval_us_),
        connection_reset_required_(true)
  {

    CHECK(channel < 128, "Channel must be between 0 and 127");
    CHECK(radio_.begin(), "Failed to start NRF24L01");
    radio_.setChannel(channel);
    radio_.setPALevel(RF24_PA_MIN, false); // low power for now
    radio_.setDataRate(RF24_2MBPS);
    radio_.setAddressWidth(3);
    radio_.setAutoAck(1);
    radio_.setRetries(0, 15);
    radio_.setCRCLength(RF24_CRC_8);
    CHECK(radio_.isChipConnected(), "NRF24L01 is unavailable");

    LOGS("\nRADIO AUTO NEGOTIATION\n");

    // Begin auto negotiation section //
    // - Radio opens a pipe with a discovery address
    // - Radio sends out its discovery data and listens for a response from other radios
    // - If no response, it sets its reading address and writing address to defaults
    // - if a radio receives a discovery message, it will respond with its reading address and writing address

    uint32_t base_address = 0xABDFF00;
    uint32_t discovery_pipe_address = base_address;
    uint32_t reading_pipe_address = base_address + 1;
    uint32_t writing_pipe_address = base_address + 2;

    uint32_t discovery_pipe = 2;
    RadioPacket discovery_packet;
    std::memset(&discovery_packet, 0, sizeof(discovery_packet));
    discovery_packet.type = packet_type::DISCOVERY;

    LOGS("opening discovery reading pipe on %d, 0x%x", discovery_pipe, discovery_pipe_address);
    radio_.openReadingPipe(discovery_pipe, discovery_pipe_address);
    radio_.openWritingPipe(discovery_pipe_address);
    radio_.stopListening();
    radio_.write(&discovery_packet, sizeof(discovery_packet)); // transmit discovery payload
    radio_.txStandBy();                                        // ensure the radio is in standby mode before listening
    radio_.startListening();

    // Wait 5 seconds for a returning discovery ack message
    bool configuration_complete = false;
    auto start = nerfnet::TimeNowUs();
    RadioPacket recvd_packet;

    while (!configuration_complete)
    {
      if (radio_.available())
      {
        uint8_t pipe;
        if (radio_.available(&pipe))
        {

          uint8_t bytes = radio_.getPayloadSize();
          radio_.read(&recvd_packet, bytes);

          switch (recvd_packet.type)
          {
          case packet_type::DISCOVERY:
            LOGS("Received Discovery");
            radio_.stopListening();
            RadioPacket send_packet;
            std::memset(&send_packet, 0, sizeof(send_packet));
            send_packet.type = packet_type::DISCOVERY_ACK;
            send_packet.data[0] = reading_pipe_address & 0xFF;
            send_packet.data[1] = writing_pipe_address & 0xFF;
            radio_.write(&send_packet, sizeof(send_packet)); // transmit discovery payload
            radio_.txStandBy();                              // ensure the radio is in standby mode before listening

            configuration_complete = true;

            break;
          case packet_type::DISCOVERY_ACK:
            radio_.stopListening();
            LOGS("Received Discovery Ack");
            reading_pipe_address = recvd_packet.data[1] | base_address;
            writing_pipe_address = recvd_packet.data[0] | base_address;
            configuration_complete = true;
            break;
          }
        }
      }
      // send discovery every half second
      if (nerfnet::TimeNowUs() - start > 1000000)
      {
        start = nerfnet::TimeNowUs();
        radio_.stopListening();                                                  // put radio in TX mode
        bool result = radio_.write(&discovery_packet, sizeof(discovery_packet)); // transmit & save the report
        radio_.txStandBy();
        if (result)
        {
          LOGS("Sent discovery packet");
        }
        else
        {
          LOGS("Failed to send discovery packet");
        }
        radio_.startListening();
      }
    }

    radio_.stopListening();

    radio_.flush_rx();
    radio_.flush_tx();

    LOGS("Reading pipe address: 0x%x", reading_pipe_address);
    LOGS("Writing pipe address: 0x%x", writing_pipe_address);
    radio_.openReadingPipe(kPipeId, reading_pipe_address);
    radio_.openWritingPipe(writing_pipe_address);

    radio_.startListening();

    LOGS("\nRADIO AUTO NEGOTIATION COMPLETE\n");

    // char payload[32] = "Hello World!";
    // start = nerfnet::TimeNowUs();
    // while (1)
    // {
    //   uint8_t available_pipe_ = 0;
    //   if (radio_.available())
    //   {
    //     uint8_t pipe;
    //     if (radio_.available(&pipe))
    //     {                                                 // is there a payload? get the pipe number that received it
    //       uint8_t bytes = radio_.getPayloadSize();        // get the size of the payload
    //       radio_.read(&payload, bytes);                   // fetch payload from FIFO
    //       LOGS("Received %d bytes : %s on pipe %d", bytes, payload, pipe); // print the size of the payload
    //     }
    //   }
    //   if (nerfnet::TimeNowUs() - start > 1000000)
    //   {
    //     start = nerfnet::TimeNowUs();                             // end the timer
    //     radio_.stopListening();                                   // put radio in TX mode
    //     bool result = radio_.write(&payload[0], sizeof(payload)); // transmit & save the report
    //     radio_.txStandBy();
    //     if (result)
    //     {
    //       LOGS("Transmission successful!");
    //     }
    //     else
    //     {
    //       LOGS("Transmission failed or timed out");
    //     }
    //     radio_.startListening();
    //   }
    // }

    pending_network_frame_buffer_.clear();
    std::vector<uint8_t> hello_message = {'H', 'e', 'l', 'l', 'o', ' ', 'f', 'r', 'o', 'm', ' ', 't', 'h', 'e', ' ', 'o', 't', 'h', 'e', 'r', ' ', 's', 'i', 'd', 'e'};
    pending_network_frame_buffer_.emplace_back(hello_message);
  }

  CommonRadioInterface::~CommonRadioInterface()
  {
    running_ = false;
    tunnel_thread_.join();
  }

  /**
   * @brief Main execution loop for the CommonRadioInterface.
   *
   * This function runs an infinite loop that periodically performs operations
   * related to the radio interface. It handles connection resets, tunnel
   * transfers, and transaction failures.
   *
   * The loop performs the following steps:
   * 1. Sleeps for the duration specified by `current_poll_interval_us_`.
   * 2. Acquires a lock on `read_buffer_mutex_` to ensure thread safety.
   * 3. Checks if a connection reset is required:
   *    - If true, attempts to reset the connection. Logs the result and handles
   *      transaction failure if the reset fails.
   *    - If successful, clears the `connection_reset_required_` flag.
   * 4. If no connection reset is required, attempts to perform a tunnel transfer:
   *    - If successful, resets the `poll_fail_count_` and updates the polling
   *      interval to `poll_interval_us_`.
   *    - If unsuccessful, handles the transaction failure.
   *
   * @note This function runs indefinitely and should be executed in a dedicated
   *       thread to avoid blocking other operations.
   */
  void CommonRadioInterface::Run()
  {
    auto last_send_time = TimeNowUs();
    while (1)
    {
      if (TimeNowUs() - last_send_time > 1000000)
      {
        last_send_time = TimeNowUs();
        LOGS("Sending");
        auto result = RadioSend({'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'});
        switch (result)
        {
        case RequestResult::Success:
          LOGS("Sent hello message");
          break;
        case RequestResult::Malformed:
          LOGE("Malformed request");
          break;
        case RequestResult::TransmitError:
          LOGE("Transmit error");
          break;
        default:
          LOGE("Unknown error");
          break;
        }
      }

      uint8_t response[kMaxPacketSize];
      uint8_t pipe;
      auto result = RadioReceive(&response[0], &pipe);
      switch (result)
      {
      case RequestResult::Success:
      {
        LOGS("Received response");
        std::string response_str(response, response + kMaxPacketSize);
        LOGS("Response: %s", response_str.c_str());

        break;
      }
      case RequestResult::Timeout:
        // LOGE("Timeout receiving response");
        break;
      default:
        LOGE("Unknown error");
        break;
      }
    }
    while (1)
    {
      continue;
      std::lock_guard<std::mutex> lock(pending_network_frame_buffer_mutex_);
      if (connection_reset_required_)
      {
        LOGS("Resetting connection");
        if (!ConnectionReset())
        {
          LOGE("Connection reset failed");
          HandleTransactionFailure();
        }
        else
        {
          LOGS("Connection reset successfully");
          connection_reset_required_ = false;
        }
      }
      else if (PerformTunnelTransfer())
      {
        poll_fail_count_ = 0;
        current_poll_interval_us_ = poll_interval_us_;
      }
      else
      {
        HandleTransactionFailure();
      }
    }
  }

  /**
   * @brief Resets the connection state of the CommonRadioInterface.
   *
   * This function resets the internal state of the CommonRadioInterface by
   * clearing the frame buffer, resetting the last acknowledgment ID, and
   * setting the next ID to 1. It then sends a tunnel reset request and waits
   * for a response to confirm the reset.
   *
   * @return true if the connection reset was successful and the response
   *         indicates success; false otherwise.
   */
  bool CommonRadioInterface::ConnectionReset()
  {
    next_id_ = 1;
    last_ack_id_.reset();
    frame_buffer_.clear();

    std::vector<uint8_t> request(kMaxPacketSize, 0x00);
    auto result = RadioSend(request);
    if (result != RequestResult::Success)
    {
      LOGE("Failed to send tunnel reset request");
      return false;
    }

    std::vector<uint8_t> response(kMaxPacketSize, 0x00);
    // result = RadioReceive(response, /*timeout_us=*/100000);
    // if (result != RequestResult::Success)
    // {
    //   LOGE("Failed to receive tunnel reset response");
    //   return false;
    // }

    return response[0] == 0x00;
  }

  /**
   * @brief Performs a tunnel transfer operation between the primary and secondary radios.
   *
   * This function handles the transmission and reception of tunnel packets, ensuring
   * proper acknowledgment and sequential packet validation. It processes the read buffer
   * for outgoing data and updates the frame buffer with incoming data.
   *
   * @return true if the tunnel transfer was successful, false otherwise.
   *
   * The function performs the following steps:
   * - Constructs a tunnel packet with the current ID and acknowledgment ID (if available).
   * - Encodes the tunnel packet and sends it to the secondary radio.
   * - Waits for a response from the secondary radio and decodes the received packet.
   * - Validates the acknowledgment ID and processes the read buffer for outgoing data.
   * - Validates the received packet ID and updates the frame buffer with incoming payload.
   * - Handles retransmissions and writes the tunnel data if the transfer is complete.
   *
   * Error handling:
   * - Logs errors if encoding, sending, receiving, or decoding fails.
   * - Logs errors for missing fields, invalid acknowledgment IDs, or non-sequential packet IDs.
   */
  bool CommonRadioInterface::PerformTunnelTransfer()
  {
    TunnelTxRxPacket tunnel;
    tunnel.id = next_id_;
    if (last_ack_id_.has_value())
    {
      tunnel.ack_id = last_ack_id_.value();
    }

    tunnel.bytes_left = 0;
    // if there are still frames to transfer, get the next bytes to transfer
    if (!pending_network_frame_buffer_.empty())
    {
      auto &frame = pending_network_frame_buffer_.front();
      size_t transfer_size = GetTransferSize(frame);
      tunnel.payload = {frame.begin(), frame.begin() + transfer_size};
      tunnel.bytes_left = std::min(frame.size(), static_cast<size_t>(UINT8_MAX)); // I think this needs to be atleast the mtu of the interface
    }

    std::vector<uint8_t> request;
    CHECK(EncodeTunnelTxRxPacket(tunnel, request),
          "Failed to encode tunnel packet");

    auto result = RadioSend(request);
    if (result != RequestResult::Success)
    {
      LOGE("Failed to send network tunnel txrx request");
      return false;
    }

    uint8_t response[kMaxPacketSize];
    uint8_t pipe;
    result = RadioReceive(response, &pipe);
    if (result != RequestResult::Success)
    {
      LOGE("Failed to receive network tunnel txrx request");
      return false;
    }
    return true;
    // if (!DecodeTunnelTxRxPacket(response, tunnel))
    // {
    //   return false;
    // }

    // if (!tunnel.id.has_value() || !tunnel.ack_id.has_value())
    // {
    //   LOGE("Missing tunnel fields");
    //   return false;
    // }

    // bool success = true;
    // if (tunnel.ack_id.value() != next_id_)
    // {
    //   LOGE("Secondary radio failed to ack, retransmitting: "
    //        "ack_id=%u, next_id=%u",
    //        tunnel.ack_id.value(), next_id_);
    //   success = false;
    // }
    // else
    // {
    //   AdvanceID();
    //   if (!pending_network_frame_buffer_.empty())
    //   {
    //     auto &frame = pending_network_frame_buffer_.front();
    //     frame.erase(frame.begin(), frame.begin() + GetTransferSize(frame));
    //     if (frame.empty())
    //     {
    //       pending_network_frame_buffer_.pop_front();
    //     }
    //   }
    // }

    // if (!ValidateID(tunnel.id.value()))
    // {
    //   LOGE("Received non-sequential packet");
    //   success = false;
    // }
    // else if (!tunnel.payload.empty())
    // {
    //   frame_buffer_.insert(frame_buffer_.end(),
    //                        tunnel.payload.begin(), tunnel.payload.end());
    //   if (tunnel.bytes_left <= kMaxPayloadSize)
    //   {
    //     WriteTunnel();
    //   }
    // }

    //return success;
  }

  /**
   * @brief Handles the failure of a transaction in the radio interface.
   *
   * This function increments the poll failure count and adjusts the polling
   * interval or flags a connection reset if the failure count exceeds a
   * threshold. Specifically:
   * - If the failure count exceeds 10 and the current polling interval is less
   *   than 1 second (1,000,000 microseconds), the polling interval is doubled.
   * - If the failure count exceeds 10 and the polling interval is already at
   *   or above 1 second, a connection reset is marked as required.
   */
  void CommonRadioInterface::HandleTransactionFailure()
  {
    poll_fail_count_++;
    if (poll_fail_count_ > 10)
    {
      if (current_poll_interval_us_ < 1000000)
      {
        current_poll_interval_us_ *= 2;
      }
      else
      {
        connection_reset_required_ = true;
      }
    }
  }

  /**
   * @brief Sends a request via the radio interface.
   *
   * This function stops the radio from listening, validates the size of the
   * request, and attempts to transmit the data. It waits for the radio to
   * enter transmit standby mode before returning the result.
   *
   * @param request A vector of bytes representing the request to be sent.
   *                The size of the request must not exceed `kMaxPacketSize`.
   *
   * @return RequestResult::Success if the request was successfully transmitted.
   * @return RequestResult::Malformed if the request size exceeds `kMaxPacketSize`.
   * @return RequestResult::TransmitError if the radio fails to write the request.
   */
  CommonRadioInterface::RequestResult CommonRadioInterface::RadioSend(
      const std::vector<uint8_t> &request)
  {
    radio_.stopListening();

    if (request.size() > kMaxPacketSize)
    {
      LOGE("Request is too large (%zu vs %zu)", request.size(), kMaxPacketSize);
      radio_.startListening();
      return RequestResult::Malformed;
    }

    if (!radio_.write(request.data(), request.size()))
    {
      LOGE("Failed to write request");
      radio_.startListening();
      return RequestResult::TransmitError;
    }

    while (!radio_.txStandBy())
    {
      LOGS("Waiting for transmit standby");
    }
    radio_.startListening();
    return RequestResult::Success;
  }

  /**
   * @brief Receives a response from the radio interface within a specified timeout.
   *
   * This function starts the radio in listening mode and waits for a response
   * to be available. If a response is received within the specified timeout,
   * it reads the response data into the provided vector. If the timeout is
   * exceeded, the function returns a timeout result.
   *
   * @param response A reference to a vector where the received response data
   *                 will be stored. The size of the vector should be sufficient
   *                 to hold the expected response.
   * @param timeout_us The timeout duration in microseconds. If set to 0, the
   *                   function will wait indefinitely for a response.
   * @return RequestResult::Success if a response is successfully received,
   *         RequestResult::Timeout if the timeout is exceeded.
   */
  CommonRadioInterface::RequestResult CommonRadioInterface::RadioReceive(
      uint8_t *response,
      uint8_t *pipe,
      uint64_t timeout_us)
  {
    uint64_t start_us = TimeNowUs();
    while (!radio_.available() && (timeout_us != 0))
    {
      if (timeout_us != 0 && (start_us + timeout_us) < TimeNowUs())
      {
        // LOGE("Timeout receiving response");
        return RequestResult::Timeout;
      }
    }
    if(radio_.available())
    {
      uint8_t pipe;
      if(radio_.available(&pipe))
      {
        uint8_t bytes = radio_.getPayloadSize();
        radio_.read(response, bytes);
      }
    }else{
      return RequestResult::Timeout;
    }
    return RequestResult::Success;
  }

  /**
   * @brief Retrieves the size of the read buffer.
   *
   * This function returns the current size of the read buffer, which represents
   * the number of elements stored in the buffer. The access to the buffer size
   * is thread-safe as it is protected by a mutex.
   *
   * @return The size of the read buffer.
   */
  size_t CommonRadioInterface::GetReadBufferSize()
  {
    std::lock_guard<std::mutex> lock(pending_network_frame_buffer_mutex_);
    return pending_network_frame_buffer_.size();
  }

  /**
   * @brief Calculates the transfer size for a given frame.
   *
   * This function determines the size of the data that can be transferred
   * based on the size of the input frame and the maximum payload size
   * allowed by the system.
   *
   * @param frame A vector of bytes representing the data frame.
   * @return The transfer size, which is the smaller of the frame size
   *         and the maximum payload size.
   */
  size_t CommonRadioInterface::GetTransferSize(const std::vector<uint8_t> &frame)
  {
    return std::min(frame.size(), static_cast<size_t>(kMaxPayloadSize));
  }

  /**
   * @brief Advances the internal ID counter to the next value.
   *
   * This function increments the `next_id_` member variable. If the value of
   * `next_id_` exceeds the maximum allowed value defined by `kIDMask`, it wraps
   * around and resets to 1. This ensures that the ID remains within a valid range.
   */
  void CommonRadioInterface::AdvanceID()
  {
    next_id_++;
    if (next_id_ > kIDMask)
    {
      next_id_ = 1;
    }
  }

  /**
   * @brief Validates the given ID against the last acknowledged ID.
   *
   * This function checks if the provided ID is valid based on the following conditions:
   * - If there is no previously acknowledged ID (`last_ack_id_` is not set).
   * - If the last acknowledged ID is equal to the maximum ID value (`kIDMask`) and the new ID is 1.
   * - If the new ID is the next sequential ID after the last acknowledged ID.
   *
   * If the ID is valid, it updates the last acknowledged ID (`last_ack_id_`) to the provided ID.
   *
   * @param id The ID to validate.
   * @return true If the ID is valid.
   * @return false If the ID is invalid.
   */
  bool CommonRadioInterface::ValidateID(uint8_t id)
  {
    if (!last_ack_id_.has_value() || (last_ack_id_.value() == kIDMask && id == 1) || (id == (last_ack_id_.value() + 1)))
    {
      last_ack_id_ = id;
      return true;
    }

    return false;
  }

  /**
   * @brief Thread function for handling data tunneling through a file descriptor.
   *
   * This function continuously reads data from a tunnel file descriptor and stores
   * it in a buffer for further processing. It ensures that the buffer does not exceed
   * a maximum number of frames by introducing a delay when the buffer is full.
   *
   * @details
   * - The function reads data from the `tunnel_fd_` file descriptor into a local buffer.
   * - If the read operation fails, an error message is logged, and the function continues.
   * - Successfully read data is added to the `read_buffer_` with thread safety ensured
   *   using a mutex (`read_buffer_mutex_`).
   * - If tunnel logging is enabled, the number of bytes read is logged.
   * - The function ensures that the size of the `read_buffer_` does not exceed
   *   `kMaxBufferedFrames` by introducing a small delay (`SleepUs(1000)`) when necessary.
   *
   * @note The function runs in a loop as long as the `running_` flag is true.
   *       The loop will terminate when `running_` is set to false.
   */
  void CommonRadioInterface::TunnelThread()
  {
    // The maximum number of network frames to buffer here.
    constexpr size_t kMaxBufferedFrames = 1024;

    running_ = true;
    uint8_t buffer[3200];
    while (running_)
    {
      int bytes_read = read(tunnel_fd_, buffer, sizeof(buffer));
      if (bytes_read < 0)
      {
        LOGE("Failed to read: %s (%d)", strerror(errno), errno);
        continue;
      }

      {
        std::lock_guard<std::mutex> lock(pending_network_frame_buffer_mutex_);
        pending_network_frame_buffer_.emplace_back(&buffer[0], &buffer[bytes_read]);
        if (tunnel_logs_enabled_ || true)
        {
          LOGS("Read %zu bytes from the tunnel", pending_network_frame_buffer_.back().size());
        }
      }

      while (GetReadBufferSize() > kMaxBufferedFrames && running_)
      {
        SleepUs(1000);
      }
    }
  }

  /**
   * @brief Decodes a TunnelTxRxPacket from the given request data.
   *
   * This function processes a request packet and extracts the tunnel information,
   * including the ID, acknowledgment ID, payload size, and payload data. It ensures
   * that the request packet size matches the expected maximum packet size and
   * handles cases where the ID or acknowledgment ID is not present.
   *
   * @param request The input vector containing the raw packet data.
   * @param tunnel The TunnelTxRxPacket object to populate with the decoded data.
   * @return true if the packet was successfully decoded; false if the packet size
   *         is invalid.
   *
   * @note The function assumes that the request vector contains at least the
   *       expected number of bytes (kMaxPacketSize). If the size is less than
   *       expected, the function logs an error and returns false.
   */
  bool CommonRadioInterface::DecodeTunnelTxRxPacket(
      const std::vector<uint8_t> &request, TunnelTxRxPacket &tunnel)
  {
    if (request.size() != kMaxPacketSize)
    {
      LOGE("Received short TxRx packet");
      return false;
    }

    tunnel.id.reset();
    uint8_t id_value = request[0] & kIDMask;
    if (id_value != 0)
    {
      tunnel.id = id_value;
    }

    tunnel.ack_id.reset();
    uint8_t ack_id_value = (request[0] >> 4) & kIDMask;
    if (ack_id_value != 0)
    {
      tunnel.ack_id = ack_id_value;
    }

    tunnel.payload.clear();
    uint8_t size_value = request[1];
    tunnel.bytes_left = size_value;
    if (size_value > 0)
    {
      size_value = std::min(size_value, static_cast<uint8_t>(kMaxPayloadSize));
      tunnel.payload = {request.begin() + 2, request.begin() + 2 + size_value};
    }

    return true;
  }

  /**
   * @brief Encodes a TunnelTxRxPacket into a request vector.
   *
   * This function takes a TunnelTxRxPacket structure and encodes its fields
   * into a request vector of bytes, which can then be transmitted over the
   * network. The encoded packet includes the ID, acknowledgment ID, payload
   * size, and payload data.
   *
   * @param tunnel The TunnelTxRxPacket structure containing the data to encode.
   * @param request A reference to a vector where the encoded packet will be stored.
   *                The vector will be resized to the fixed packet size (kMaxPacketSize).
   *
   * @return True if the packet was successfully encoded, false otherwise.
   *         Returns false if the payload size exceeds the maximum allowed size.
   *
   * @note The request vector will be resized to kMaxPacketSize and initialized
   *       with zeros before encoding.
   * @note The payload size must not exceed kMaxPayloadSize.
   */
  bool CommonRadioInterface::EncodeTunnelTxRxPacket(const TunnelTxRxPacket &tunnel, std::vector<uint8_t> &request)
  {
    request.resize(kMaxPacketSize, 0x00);
    if (tunnel.id.has_value())
    {
      request[0] = tunnel.id.value();
    }

    if (tunnel.ack_id.has_value())
    {
      request[0] |= (tunnel.ack_id.value() << 4);
    }

    if (tunnel.payload.size() > kMaxPayloadSize)
    {
      LOGE("TxRx packet payload is too large");
      return false;
    }

    request[1] = tunnel.bytes_left;
    for (size_t i = 0; i < tunnel.payload.size(); i++)
    {
      request[2 + i] = tunnel.payload[i];
    }

    return true;
  }

  /**
   * @brief Writes the contents of the frame buffer to the tunnel file descriptor.
   *
   * This function writes the data stored in the `frame_buffer_` to the tunnel
   * represented by `tunnel_fd_`. After writing, the `frame_buffer_` is cleared.
   * If tunnel logging is enabled, the number of bytes written is logged. If the
   * write operation fails, an error message is logged with the corresponding
   * error details.
   *
   * @note Ensure that `tunnel_fd_` is a valid file descriptor before calling
   * this function.
   */
  void CommonRadioInterface::WriteTunnel()
  {
    int bytes_written = write(tunnel_fd_,
                              frame_buffer_.data(), frame_buffer_.size());
    if (tunnel_logs_enabled_)
    {
      LOGS("Writing %d bytes to the tunnel", frame_buffer_.size());
    }

    frame_buffer_.clear();
    if (bytes_written < 0)
    {
      LOGE("Failed to write to tunnel %s (%d)", strerror(errno), errno);
    }
  }

} // namespace nerfnet
