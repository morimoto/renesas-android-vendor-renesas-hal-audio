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
#include <stdlib.h>
#include <assert.h>

inline void audio_buffer_shrink(void *out_buffer, const size_t out_channels,
                                const void *in_buffer, const size_t in_channels,
                                const size_t frame_count, const size_t format_bytes)
{
    assert(in_channels > out_channels);
    size_t in_pos = 0;
    size_t out_pos = 0;
    const size_t in_end_pos = frame_count * in_channels * format_bytes;
    while (in_pos < in_end_pos) {
        memcpy(&((uint8_t *)out_buffer)[out_pos],
               &((const uint8_t *)in_buffer)[in_pos],
               out_channels * format_bytes);
        in_pos += in_channels * format_bytes;
        out_pos += out_channels * format_bytes;
    }
}

inline void audio_buffer_expand(void *out_buffer, const size_t out_channels,
                                const void *in_buffer, const size_t in_channels,
                                const size_t frame_count, const size_t format_bytes)
{
    assert(in_channels < out_channels);
    size_t in_pos = 0;
    size_t out_pos = 0;
    const size_t in_end_pos = frame_count * in_channels;
    while (in_pos < in_end_pos) {
        for (size_t i = out_pos; i < (out_pos + out_channels); i += in_channels) {
            memcpy(&((uint8_t *)out_buffer)[i * format_bytes],
                   &((const uint8_t *)in_buffer)[in_pos * format_bytes],
                   in_channels * format_bytes);
        }
        in_pos += in_channels;
        out_pos += out_channels;
    }
}

inline void audio_buffer_adjust(void *out_buffer, const size_t out_channels,
                                const void *in_buffer, const size_t in_channels,
                                const size_t frame_count, const size_t format_bytes)
{
    if (in_channels < out_channels) {
        audio_buffer_expand(out_buffer, out_channels,
                            in_buffer, in_channels,
                            frame_count, format_bytes);
    } else if (in_channels > out_channels) {
        audio_buffer_shrink(out_buffer, out_channels,
                            in_buffer, in_channels,
                            frame_count, format_bytes);
    } else {
        memcpy(out_buffer, in_buffer, frame_count * in_channels * format_bytes);
    }
}

#endif