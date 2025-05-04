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

// ANSI color codes
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_WHITE   "\033[37m"

#define NUM_LINES_LOGGED 15

// Forward declaration for table printing
struct Stats;
void print_stats_table(const Stats* stats);

struct Stats
{
  uint32_t packets_sent = 0;
  uint32_t packets_received = 0;
  uint32_t fragments_sent = 0;
  uint32_t fragments_received = 0;
  std::deque<std::string> messages;
  // Call this after updating any stat
  void update_and_print() const {
    print_stats_table(this);
  }
};

extern Stats stats;

// Clear the console and move cursor to top-left
#define CLEAR_SCREEN() printf("\033[2J\033[H")

// Print the stats table
inline void print_stats_table(const Stats* stats) {
  CLEAR_SCREEN();
  if (!stats) return;
  printf(COLOR_GREEN "=============== Stats Table ==============" COLOR_RESET "\n");
  printf(COLOR_WHITE "| %-22s | %-13s |\n", "Stat", "Value");
  printf("|------------------------|---------------|\n");
  printf("| %-22s | %-13u |\n", "Packets Sent", stats->packets_sent);
  printf("| %-22s | %-13u |\n", "Packets Received", stats->packets_received);
  printf("| %-22s | %-13u |\n", "Fragments Sent", stats->fragments_sent);
  printf("| %-22s | %-13u |\n", "Fragments Received", stats->fragments_received);
  printf(COLOR_GREEN "==========================================\n" COLOR_RESET);
  //printf(COLOR_GREEN "Messages:\n" COLOR_RESET);
  for (const auto& msg : stats->messages) {
    printf(COLOR_WHITE "%s\n" COLOR_RESET, msg.c_str());
  }
}

// Helper macros to update stats and print table
#define UPDATE_STATS(stats_ptr, field, value) \
  do { \
    (stats_ptr)->field = (value); \
    print_stats_table(stats_ptr); \
  } while (0)

#define INCREMENT_STATS(stats_ptr, field) \
  do { \
    ++((stats_ptr)->field); \
    print_stats_table(stats_ptr); \
  } while (0)



// Check a condition and quit if it evaluates to false with an error log.
#define CHECK(cond, fmt, ...)                               \
    do {                                                    \
      if (!(cond)) {                                        \
        LOGE("FATAL: " fmt, ##__VA_ARGS__);                 \
        exit(-1);                                           \
      }                                                     \
    } while (0)

// Check that a util::Status object is ok, otherwise fail.
#define CHECK_OK(status, fmt, ...) \
    CHECK(status.ok(), fmt ": %s", ##__VA_ARGS__, status.ToString().c_str())

// TODO(aarossig): Allow disabling LOGV at compile time.

// Modify the LOG macro to add formatted messages to the deque
#define LOG(color, fmt, ...) \
  do { \
    if (stats.messages.size() >= NUM_LINES_LOGGED) { \
      stats.messages.pop_front(); \
    } \
    char buffer[256]; \
    snprintf(buffer, sizeof(buffer), fmt, ##__VA_ARGS__); \
    std::string colored_msg = std::string(color) + buffer + COLOR_RESET; \
    stats.messages.push_back(colored_msg); \
    print_stats_table(&stats); \
  } while (0)

// Logging macros for error, warning, info, and verbose with colors.
#define LOGV(fmt, ...) LOG(COLOR_CYAN, fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) LOG(COLOR_WHITE, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) LOG(COLOR_YELLOW, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) LOG(COLOR_RED, fmt, ##__VA_ARGS__)

#endif  // NERFNET_UTIL_LOG_H_
