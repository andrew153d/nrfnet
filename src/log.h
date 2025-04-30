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

// ANSI color codes
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_WHITE   "\033[37m"

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

// Common logging macro with color support.
#define LOG(color, fmt, ...) \
  fprintf(stdout, "%s" fmt "%s\n", color, ##__VA_ARGS__, COLOR_RESET)

// Logging macros for error, warning, info, and verbose with colors.
#define LOGV(fmt, ...) LOG(COLOR_CYAN, fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) LOG(COLOR_WHITE, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) LOG(COLOR_YELLOW, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) LOG(COLOR_RED, fmt, ##__VA_ARGS__)

#endif  // NERFNET_UTIL_LOG_H_
