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

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define count_writable_frames(vbuf, frame_count, frames) \
do {\
  if ((vbuf->live) == 0 || (vbuf->head) > (vbuf->tail)) {\
    frames = MIN(frame_count, ((vbuf->frame_count) - (vbuf->head)));\
  } else if (vbuf->head < vbuf->tail) {\
    frames = MIN(frame_count, ((vbuf->tail) - (vbuf->head)));\
  } else {\
    frames = 0;\
  }\
} while (0)

#define count_readable_frames(vbuf, frame_count, frames) \
do {\
  if ((vbuf->live) == (vbuf->frame_count) || (vbuf->tail) > (vbuf->head)) {\
    frames = MIN(frame_count, ((vbuf->frame_count) - (vbuf->tail)));\
  } else if ((vbuf->tail) < (vbuf->head)) {\
    frames = MIN(frame_count, ((vbuf->head) - (vbuf->tail)));\
  } else {\
    frames = 0;\
  }\
} while (0)

int audio_vbuffer_init(audio_vbuffer_t *audio_vbuffer, size_t frame_count,
                       size_t format_bytes, size_t channels) {
  if (!audio_vbuffer) {
    return -EINVAL;
  }

  size_t frame_size = format_bytes * channels;

  audio_vbuffer->frame_size = frame_size;
  audio_vbuffer->frame_count = frame_count;
  audio_vbuffer->channels = channels;
  audio_vbuffer->format_bytes = format_bytes;
  size_t bytes = frame_count * frame_size;
  audio_vbuffer->data = (uint8_t *) calloc(bytes, 1);
  if (!audio_vbuffer->data) {
    return -ENOMEM;
  }
  audio_vbuffer->head = 0;
  audio_vbuffer->tail = 0;
  audio_vbuffer->live = 0;
  pthread_mutex_init(&audio_vbuffer->lock, (const pthread_mutexattr_t *)NULL);
  return 0;
}

int audio_vbuffer_destroy(audio_vbuffer_t *audio_vbuffer) {
  if (!audio_vbuffer) {
    return -EINVAL;
  }
  free(audio_vbuffer->data);
  pthread_mutex_destroy(&audio_vbuffer->lock);
  return 0;
}

int audio_vbuffer_live(audio_vbuffer_t *audio_vbuffer) {
  if (!audio_vbuffer) {
    return -EINVAL;
  }
  pthread_mutex_lock(&audio_vbuffer->lock);
  int live = audio_vbuffer->live;
  pthread_mutex_unlock(&audio_vbuffer->lock);
  return live;
}

int audio_vbuffer_dead(audio_vbuffer_t *audio_vbuffer) {
  if (!audio_vbuffer) {
    return -EINVAL;
  }
  pthread_mutex_lock(&audio_vbuffer->lock);
  int dead = audio_vbuffer->frame_count - audio_vbuffer->live;
  pthread_mutex_unlock(&audio_vbuffer->lock);
  return dead;
}

size_t audio_vbuffer_write(audio_vbuffer_t *audio_vbuffer, const void *buffer,
                           size_t frame_count) {
  size_t frames_written = 0;
  pthread_mutex_lock(&audio_vbuffer->lock);

  while (frame_count != 0) {
    int frames = 0;

    count_writable_frames(audio_vbuffer, frame_count, frames);

    if (!frames) {
      ALOGD("{%s} audio_vbuffer is still full, breaking...", __func__);
      break;
    }

    ALOGV("{%s} passing buffer through", __func__);
    memcpy(
      &audio_vbuffer->data[audio_vbuffer->head * audio_vbuffer->frame_size],
      &((uint8_t *)buffer)[frames_written * audio_vbuffer->frame_size],
      frames * audio_vbuffer->frame_size);

    audio_vbuffer->live += frames;
    frames_written += frames;
    frame_count -= frames;
    audio_vbuffer->head =
        (audio_vbuffer->head + frames) % audio_vbuffer->frame_count;
  }

  pthread_mutex_unlock(&audio_vbuffer->lock);
  return frames_written;
}

size_t audio_vbuffer_read(audio_vbuffer_t *audio_vbuffer, void *buffer,
                          size_t frame_count) {
  size_t frames_read = 0;
  pthread_mutex_lock(&audio_vbuffer->lock);

  while (frame_count != 0) {
    int frames = 0;

    count_readable_frames(audio_vbuffer, frame_count, frames);
    if (!frames) {
      ALOGD("{%s} audio_vbuffer is still empty, breaking...", __func__);
      break;
    }

    ALOGV("{%s} passing buffer through", __func__);
    memcpy(
      &((uint8_t *)buffer)[frames_read * audio_vbuffer->frame_size],
      &audio_vbuffer->data[audio_vbuffer->tail * audio_vbuffer->frame_size],
      frames * audio_vbuffer->frame_size);

    audio_vbuffer->live -= frames;
    frames_read += frames;
    frame_count -= frames;
    audio_vbuffer->tail =
        (audio_vbuffer->tail + frames) % audio_vbuffer->frame_count;
  }

  pthread_mutex_unlock(&audio_vbuffer->lock);
  return frames_read;
}
size_t audio_vbuffer_write_adjust(audio_vbuffer_t *audio_vbuffer, const void *buffer,
                                  size_t frame_count, const size_t input_channels) {
  size_t frames_written = 0;
  pthread_mutex_lock(&audio_vbuffer->lock);

  while (frame_count != 0) {
    int frames = 0;

    count_writable_frames(audio_vbuffer, frame_count, frames);
    if (!frames) {
      ALOGD("{%s} audio_vbuffer is full, breaking...", __func__);
      break;
    }

    // expand to to input_channels by copying
    ALOGV("{%s} expanding buffer from %zu to %zu channels (%d frames, %zu written frames)",
          __func__, input_channels, audio_vbuffer->channels, frames, frames_written);
    size_t vbuffer_start = audio_vbuffer->head * audio_vbuffer->frame_size;
    size_t buffer_start = (frames_written * audio_vbuffer->frame_size * input_channels) / audio_vbuffer->channels;

    audio_buffer_adjust(audio_vbuffer->data + vbuffer_start, audio_vbuffer->channels,
                        (uint8_t *)buffer + buffer_start, input_channels,
                        frames, audio_vbuffer->format_bytes);

    audio_vbuffer->live += frames;
    frames_written += frames;
    frame_count -= frames;
    audio_vbuffer->head = (audio_vbuffer->head + frames) % audio_vbuffer->frame_count;
  }

  pthread_mutex_unlock(&audio_vbuffer->lock);
  return frames_written;
}

size_t audio_vbuffer_read_adjust(audio_vbuffer_t *audio_vbuffer, void *buffer,
                                 size_t frame_count, size_t output_channels) {
  size_t frames_read = 0;
  pthread_mutex_lock(&audio_vbuffer->lock);

  while (frame_count != 0) {
    int frames = 0;

    count_readable_frames(audio_vbuffer, frame_count, frames);
    if (!frames) {
      ALOGD("{%s} audio_vbuffer is empty, breaking...", __func__);
      break;
    }

    // shrink to output_channels by discarding surplus channels
    ALOGV("{%s} shrinking buffer from %zu to %zu channels (%d frames, %zu read frames)",
          __func__, audio_vbuffer->channels, output_channels, frames, frames_read);
    size_t vbuffer_start = audio_vbuffer->tail * audio_vbuffer->frame_size;
    size_t buffer_start = (frames_read * audio_vbuffer->frame_size * output_channels) / audio_vbuffer->channels;

    audio_buffer_adjust((uint8_t *)buffer + buffer_start, output_channels,
                        audio_vbuffer->data + vbuffer_start, audio_vbuffer->channels,
                        frames, audio_vbuffer->format_bytes);

    audio_vbuffer->live -= frames;
    frames_read += frames;
    frame_count -= frames;
    audio_vbuffer->tail =
        (audio_vbuffer->tail + frames) % audio_vbuffer->frame_count;
  }

  pthread_mutex_unlock(&audio_vbuffer->lock);
  return frames_read;
}
