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

#define LOG_TAG "audio_hw_generic"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <log/log.h>

#include "audio_vbuffer.h"
#include "buffer_utils.h"

size_t vbuf_writable_frames(size_t head, size_t tail,
                            size_t bytes, size_t frame_size) {
  size_t res;
  if(head >= tail) {
    res = (tail + bytes) - head - 1;
  } else {
    res = tail - head - 1;
  }

  return res / frame_size;
}

size_t vbuf_readable_frames(size_t head, size_t tail,
                            size_t bytes, size_t frame_size) {
  size_t res;
  if(head >= tail) {
    res = head - tail;
  } else {
    res = (bytes + head) - tail;
  }

  return res / frame_size;
}

int audio_vbuffer_init(audio_vbuffer_t *audio_vbuffer, size_t frame_count,
                       size_t frame_size) {
  if (!audio_vbuffer) {
    return -EINVAL;
  }

  audio_vbuffer->frame_size = frame_size;
  audio_vbuffer->frame_count = frame_count;
  size_t bytes = (frame_count * frame_size) + 1;
  audio_vbuffer->data = (uint8_t *) calloc(bytes, 1);
  if (!audio_vbuffer->data) {
    return -ENOMEM;
  }
  audio_vbuffer->buffer_size = bytes;
  atomic_init(&audio_vbuffer->head, 0);
  atomic_init(&audio_vbuffer->tail, 0);
  return 0;
}

int audio_vbuffer_destroy(audio_vbuffer_t *audio_vbuffer) {
  if (!audio_vbuffer) {
    return -EINVAL;
  }
  free(audio_vbuffer->data);
  return 0;
}

int audio_vbuffer_live(audio_vbuffer_t *audio_vbuffer) {
  if (!audio_vbuffer) {
    return -EINVAL;
  }
  size_t tail = atomic_load_explicit(&audio_vbuffer->tail, memory_order_relaxed);
  size_t head = atomic_load_explicit(&audio_vbuffer->head, memory_order_acquire);

  return vbuf_readable_frames(head, tail, audio_vbuffer->buffer_size,
                              audio_vbuffer->frame_size);
}

size_t audio_vbuffer_write(audio_vbuffer_t *audio_vbuffer, const void *buffer,
                           const size_t frame_count) {
  size_t head = atomic_load_explicit(&audio_vbuffer->head, memory_order_relaxed);
  size_t tail = atomic_load_explicit(&audio_vbuffer->tail, memory_order_acquire);
  size_t byte_count = frame_count * audio_vbuffer->frame_size;

  if(frame_count > vbuf_writable_frames(head, tail, audio_vbuffer->buffer_size,
                                                    audio_vbuffer->frame_size)) {
    ALOGD("{%s} audio_vbuffer is too full", __func__);
    return 0;
  }

  size_t next_head = head + byte_count;
  if(next_head > audio_vbuffer->buffer_size) {
    size_t chunk_end = audio_vbuffer->buffer_size - head;
    size_t chunk_start = byte_count - chunk_end;

    memcpy(&audio_vbuffer->data[head], buffer, chunk_end);
    memcpy(audio_vbuffer->data, &((uint8_t*)buffer)[chunk_end], chunk_start);

    next_head -= audio_vbuffer->buffer_size;
  } else {
    memcpy(&audio_vbuffer->data[head], buffer, byte_count);
    if(next_head == audio_vbuffer->buffer_size) {
      next_head = 0;
    }
  }

  atomic_store_explicit(&audio_vbuffer->head, next_head, memory_order_release);
  return frame_count;
}

size_t audio_vbuffer_read(audio_vbuffer_t *audio_vbuffer, void *buffer,
                          const size_t frame_count) {
  size_t tail = atomic_load_explicit(&audio_vbuffer->tail, memory_order_relaxed);
  size_t head = atomic_load_explicit(&audio_vbuffer->head, memory_order_acquire);
  size_t byte_count = frame_count * audio_vbuffer->frame_size;

  if(frame_count > vbuf_readable_frames(head, tail, audio_vbuffer->buffer_size,
                                                    audio_vbuffer->frame_size)) {
    ALOGD("{%s} audio_vbuffer has less frames than needed", __func__);
    return 0;
  }

  size_t next_tail = tail + byte_count;
  if(next_tail > audio_vbuffer->buffer_size) {
    size_t chunk_end = audio_vbuffer->buffer_size - tail;
    size_t chunk_start = byte_count - chunk_end;

    memcpy(buffer, &audio_vbuffer->data[tail], chunk_end);
    memcpy(&((uint8_t*)buffer)[chunk_end], audio_vbuffer->data, chunk_start);

    next_tail -= audio_vbuffer->buffer_size;
  } else {
    memcpy(buffer, &audio_vbuffer->data[tail], byte_count);
    if(next_tail == audio_vbuffer->buffer_size) {
      next_tail = 0;
    }
  }

  atomic_store_explicit(&audio_vbuffer->tail, next_tail, memory_order_release);
  return frame_count;
}