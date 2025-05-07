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

#ifndef NERFNET_UTIL_LOG_H_
#define NERFNET_UTIL_LOG_H_

#include <cstdio>
#include <cstdlib>
#include <deque>
#include <string>
#include <utility>
#include "nrftime.h"
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <stdbool.h>
#include "nrftime.h"
// ANSI color codes
#define COLOR_RESET "\033[0m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_CYAN "\033[36m"
#define COLOR_WHITE "\033[37m"

#define NUM_LINES_LOGGED 20
#define ENABLE_TABLE_PRINTING

// Clear the console and move cursor to top-left
#define CLEAR_SCREEN() printf("\033[2J\033[H")

namespace Logger
{
  struct Statistics
  {
    uint32_t packets_sent = 0;
    uint32_t packets_received = 0;
    uint32_t packet_size = 0;
    uint32_t fragments_sent = 0;
    uint32_t fragments_received = 0;
    uint32_t ack_messages_sent = 0;
    uint32_t ack_messages_received = 0;
    uint32_t ack_messages_resent = 0;
    uint32_t radio_packets_sent = 0;
    uint32_t radio_packets_received = 0;
    float error_rate = 0.0f;
    std::deque<std::string> messages;
  };

  class LogPrinter
  {
  public:
    LogPrinter()
    {
      thread_ = std::thread(&LogPrinter::log_thread, this);
      printf(COLOR_GREEN "Logger thread started\n" COLOR_RESET);
    }

    ~LogPrinter()
    {
      {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_thread_ = true;
      }
      cv_.notify_all();
      if (thread_.joinable())
      {
        thread_.join();
      }
    }

    void log_thread()
    {
      while (true)
      {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!cv_.wait_for(lock, std::chrono::milliseconds(1500), [this]
                         { return this->dataChanged; }))
        {
          // Run the error rate calculation every 100ms
          while (error_times_.size() > 0 && error_times_.front() < nerfnet::TimeNowUs() - 1000000) // The number of errors in the last second
          {
            error_times_.pop_front();
          }
          float alpha = 0.1f;
          stats.error_rate = (1.0f - alpha) * stats.error_rate + alpha * static_cast<float>(error_times_.size());
        }
        dataChanged = false;
        if (stop_thread_)
        {
          break;
        }

        while (log_queue_.size() > NUM_LINES_LOGGED)
        {
          log_queue_.pop_front();
        }

        std::string string_message = "";

        // Print the top lines of the table
        string_message += "┌──────────────────────────────────────────┐\n";
        string_message += "│           Statistics Table               │\n";
        string_message += "├──────────────────────────────┬───────────┤\n";
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "│ %-28s │ %-10u│\n", "Packets Sent", stats.packets_sent);
        string_message += buffer;
        snprintf(buffer, sizeof(buffer), "│ %-28s │ %-10u│\n", "Packets Received", stats.packets_received);
        string_message += buffer;
        snprintf(buffer, sizeof(buffer), "│ %-28s │ %-10u│\n", "Packet Size", stats.packet_size);
        string_message += buffer;
        snprintf(buffer, sizeof(buffer), "│ %-28s │ %-10u│\n", "Fragments Sent", stats.fragments_sent);
        string_message += buffer;
        snprintf(buffer, sizeof(buffer), "│ %-28s │ %-10u│\n", "Fragments Received", stats.fragments_received);
        string_message += buffer;
        snprintf(buffer, sizeof(buffer), "│ %-28s │ %-10u│\n", "Ack Messages Sent", stats.ack_messages_sent);
        string_message += buffer;
        snprintf(buffer, sizeof(buffer), "│ %-28s │ %-10u│\n", "Ack Messages Received", stats.ack_messages_received);
        string_message += buffer;
        snprintf(buffer, sizeof(buffer), "│ %-28s │ %-10u│\n", "Ack Messages Resent", stats.ack_messages_resent);
        string_message += buffer;
        snprintf(buffer, sizeof(buffer), "│ %-28s │ %-10u│\n", "Radio Packets Sent", stats.radio_packets_sent);
        string_message += buffer;
        snprintf(buffer, sizeof(buffer), "│ %-28s │ %-10u│\n", "Radio Packets Received", stats.radio_packets_received);
        string_message += buffer;
        snprintf(buffer, sizeof(buffer), "│ %-28s │ %-10.2f│\n", "Error Rate", stats.error_rate);
        string_message += buffer;
        string_message += "└──────────────────────────────┴───────────┘\n";

        for (const auto &message : log_queue_)
        {
          string_message += message;
          string_message += "\n";
        }

        CLEAR_SCREEN();

        printf("%s", string_message.c_str());
      }
      printf(COLOR_RED "Logger thread exiting\n" COLOR_RESET);
    }

    void print()
    {
      std::lock_guard<std::mutex> lock(mutex_);
      // while(log_queue_.size() > NUM_LINES_LOGGED)
      // {
      //   log_queue_.pop_front();
      // }
      CLEAR_SCREEN();
      for (const auto &message : log_queue_)
      {
        printf("%s\n", message.c_str());
      }
    }

    void update()
    {
      // std::lock_guard<std::mutex> lock(mutex_);
      // print();
    }

    void log(const std::string &message)
    {

      std::lock_guard<std::mutex> lock(mutex_);
      log_queue_.push_back(message);
      if (message.find(COLOR_RED) != std::string::npos)
      {
        error_times_.push_back(nerfnet::TimeNowUs());
      }
      cv_.notify_all();
    }

    Statistics stats;

    std::deque<std::string> log_queue_;
    std::deque<std::uint64_t> error_times_;

    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;

    bool stop_thread_ = false;
    bool dataChanged = false;
  };

}
extern Logger::LogPrinter logger;

// Helper macros to update stats and print table
#ifdef ENABLE_TABLE_PRINTING
#define UPDATE_STATS(stats_ptr, field, value) \
  do                                          \
  {                                           \
    logger.stats.field = (value);             \
    logger.update();                          \
  } while (0)

#define INCREMENT_STATS(stats_ptr, field) \
  do                                      \
  {                                       \
    ++(logger.stats.field);               \
    logger.update();                      \
    logger.dataChanged = true;            \
    logger.cv_.notify_all();              \
  } while (0)
#else
#define UPDATE_STATS(stats_ptr, field, value) \
  do                                          \
  {                                           \
    logger.stats.field = (value);             \
  } while (0)
#define INCREMENT_STATS(stats_ptr, field) \
  do                                      \
  {                                       \
    ++(logger.stats.field);               \
  } while (0)
#endif

// Check a condition and quit if it evaluates to false with an error log.
#define CHECK(cond, fmt, ...)             \
  do                                      \
  {                                       \
    if (!(cond))                          \
    {                                     \
      LOGE("FATAL: " fmt, ##__VA_ARGS__); \
      exit(-1);                           \
    }                                     \
  } while (0)

// Check that a util::Status object is ok, otherwise fail.
#define CHECK_OK(status, fmt, ...) \
  CHECK(status.ok(), fmt ": %s", ##__VA_ARGS__, status.ToString().c_str())

// TODO(aarossig): Allow disabling LOGV at compile time.

// Modify the LOG macro to add formatted messages to the deque
#ifdef ENABLE_TABLE_PRINTING
#define LOG(color, fmt, ...)                                             \
  do                                                                     \
  {                                                                      \
    char buffer[256];                                                    \
    snprintf(buffer, sizeof(buffer), fmt, ##__VA_ARGS__);                \
    std::string colored_msg = std::string(color) + buffer + COLOR_RESET; \
    logger.log(colored_msg);                                             \
    logger.dataChanged = true;                                           \
    logger.cv_.notify_all();                                             \
  } while (0)
#else
#define LOG(color, fmt, ...)                              \
  do                                                      \
  {                                                       \
    char buffer[256];                                     \
    snprintf(buffer, sizeof(buffer), fmt, ##__VA_ARGS__); \
    printf("%s%s%s\n", color, buffer, COLOR_RESET);       \
  } while (0)
#endif

// Logging macros for error, warning, info, and verbose with colors.
#define LOGV(fmt, ...) LOG(COLOR_CYAN, fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) LOG(COLOR_WHITE, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) LOG(COLOR_YELLOW, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) LOG(COLOR_RED, fmt, ##__VA_ARGS__)

#endif // NERFNET_UTIL_LOG_H_
