/*
 * Stereo channels mixer
 *
 * Copyright (C) 2018 The Android Open Source Project
 * Copyright (C) 2020 GlobalLogic
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

#ifndef EXT_PCM_H
#define EXT_PCM_H

#include <pthread.h>

#include <cutils/hashmap.h>
#include <tinyalsa/asoundlib.h>
#include <stdatomic.h>

#include "audio_vbuffer.h"

struct ext_mixer_thread {
  pthread_t thread;
  Hashmap *bus_map;
  pthread_mutex_t bus_map_lock;
  void *mixed_buffer; // audio_vbuffer format
  void *read_buffer; // audio_vbuffer format
  void *write_buffer; // PCM format (if different)
  size_t mixed_frames;
  struct pcm *pcm;
  struct pcm_settings {
    unsigned int card;
    unsigned int device;
    unsigned int flags;
    struct pcm_config config;
  } pcm_conf;

  unsigned int (*pcm_write)(struct ext_mixer_thread*, size_t /*bytes*/);

  struct bus_settings {
    size_t frame_count;
    size_t channels;
    size_t format_bytes;
    size_t frame_size;
  } bus_conf;
  atomic_bool do_exit;
  atomic_flag thread_notify;
  atomic_bool pcm_init_done;
  atomic_int error;
};

struct ext_pcm {
  struct ext_mixer_thread mixer;
  atomic_uint ref_count;
};

struct ext_pcm *ext_pcm_open_default(unsigned int card, unsigned int device,
                             unsigned int flags, struct pcm_config *config);
struct ext_pcm *ext_pcm_open_hfp(unsigned int card, unsigned int device,
                             unsigned int flags, struct pcm_config *config);
int ext_pcm_close(struct ext_pcm *ext_pcm, const char *bus_address);
int ext_pcm_is_ready(struct ext_pcm *ext_pcm);

audio_vbuffer_t *ext_pcm_get_write_bus(struct ext_pcm *ext_pcm, const char *bus_address);
size_t ext_pcm_write_bus(audio_vbuffer_t *ext_pcm_bus,
                         const void *data, size_t count);

const char *ext_pcm_get_error(struct ext_pcm *ext_pcm);
size_t ext_pcm_frames_to_bytes(struct ext_pcm *ext_pcm,
                                     size_t frames);
size_t ext_pcm_bytes_to_frames(struct ext_pcm *ext_pcm,
                                     size_t bytes);

#endif  // EXT_PCM_H
