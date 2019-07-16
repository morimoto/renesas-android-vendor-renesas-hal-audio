/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef AUDIO_VBUFFER_H
#define AUDIO_VBUFFER_H

#include <pthread.h>

typedef struct audio_vbuffer {
  pthread_mutex_t lock;
  uint8_t *data;
  size_t frame_size;
  size_t frame_count;
  size_t head;
  size_t tail;
  size_t live;
  size_t channels;
  size_t format_bytes;
} audio_vbuffer_t;

// Initialize a virtual buffer
int audio_vbuffer_init(audio_vbuffer_t *audio_vbuffer, size_t frame_count,
                       size_t format_bytes, size_t channels);

// Destroy a virtual buffer
int audio_vbuffer_destroy(audio_vbuffer_t *audio_vbuffer);

// Get the number of live (unread) frames in vbuffer
int audio_vbuffer_live(audio_vbuffer_t *audio_vbuffer);

// Get the number of dead (read) frames in vbuffer
int audio_vbuffer_dead(audio_vbuffer_t *audio_vbuffer);

// Write to vbuffer
// NOTE: this function assumes input buffer has the same format as vbuffer
size_t audio_vbuffer_write(audio_vbuffer_t *audio_vbuffer, const void *buffer,
                           size_t frame_count);

// Read from vbuffer
// NOTE: this function assumes output buffer has the same format as vbuffer
size_t audio_vbuffer_read(audio_vbuffer_t *audio_vbuffer, void *buffer,
                          size_t frame_count);

// Write to vbuffer with frame duplication
// Example: in case of 2 channel (stereo) input buffer and 8 channel vbuffer
// this function will duplicate input buffer frames 4 times for each input frame.
size_t audio_vbuffer_write_adjust(audio_vbuffer_t *audio_vbuffer, const void *buffer,
                                  size_t frame_count, size_t input_channels);

// Read from vbuffer with discard
// Example: in case of 2 channel (stereo) output buffer and 6 channel vbuffer
// this function will discard last 4 vbuffer channels for each output frame.
size_t audio_vbuffer_read_adjust(audio_vbuffer_t *audio_vbuffer, void *buffer,
                                 size_t frame_count, size_t output_channels);

#endif  // AUDIO_VBUFFER_H
