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

// FIFO single producer - single consumer audio ringbuffer
typedef struct audio_vbuffer {
  uint8_t *data;
  size_t frame_size;
  size_t frame_count;
  size_t buffer_size;
  size_t head;
  size_t tail;

  pthread_mutex_t lock;
  pthread_cond_t has_free;
} audio_vbuffer_t;

// Initialize a virtual buffer
int audio_vbuffer_init(audio_vbuffer_t *audio_vbuffer, size_t frame_count,
                       size_t frame_size);

// Destroy a virtual buffer
int audio_vbuffer_destroy(audio_vbuffer_t *audio_vbuffer);

// Get the number of live (unread) frames in vbuffer
int audio_vbuffer_live(audio_vbuffer_t *audio_vbuffer);

// Write to vbuffer
// NOTE: this function assumes input buffer has the same format as vbuffer
size_t audio_vbuffer_write(audio_vbuffer_t *audio_vbuffer, const void *buffer, const size_t frame_count);

// Write to vbuffer and block until it is ready again
// NOTE: this function assumes input buffer has the same format as vbuffer
size_t audio_vbuffer_write_block(audio_vbuffer_t *audio_vbuffer, const void *buffer, const size_t frame_count);

// Read from vbuffer
// NOTE: this function assumes output buffer has the same format as vbuffer
size_t audio_vbuffer_read(audio_vbuffer_t *audio_vbuffer, void *buffer, const size_t frame_count);

// Read from vbuffer and notifies to producer
// NOTE: this function assumes output buffer has the same format as vbuffer
size_t audio_vbuffer_read_notify(audio_vbuffer_t *audio_vbuffer, void *buffer, const size_t frame_count);

// TODO: blocking read

#endif  // AUDIO_VBUFFER_H
