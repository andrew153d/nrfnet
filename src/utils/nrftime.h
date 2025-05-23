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

#ifndef NERFNET_UTIL_TIME_H_
#define NERFNET_UTIL_TIME_H_

#include <cstdint>

namespace nerfnet {

// Sleeps for the privided number of microseconds.
void SleepUs(uint64_t delay);

// Returns the current time in microseconds.
uint64_t TimeNowUs();

uint64_t TimeNowS();

}  // namespace nerfnet

#endif  // NERFNET_UTIL_TIME_H_
