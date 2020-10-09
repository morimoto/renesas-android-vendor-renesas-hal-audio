/*
 * Copyright (C) 2019 GlobalLogic
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef BUFFER_UTILS_H
#define BUFFER_UTILS_H

#include <stdint.h>

void audio_buffer_shrink(void *out_buffer, const size_t out_channels,
                         const void *in_buffer, const size_t in_channels,
                         const size_t frame_count, const size_t format_bytes);
void audio_buffer_expand(void *out_buffer, const size_t out_channels,
                         const void *in_buffer, const size_t in_channels,
                         const size_t frame_count, const size_t format_bytes);
void audio_buffer_adjust(void *out_buffer, const size_t out_channels,
                         const void *in_buffer, const size_t in_channels,
                         const size_t frame_count, const size_t format_bytes);

#endif