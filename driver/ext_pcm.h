/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "hal_dependencies.h"

#define MIXER_BUFFER_SIZE (OUT_PERIOD_SIZE * OUT_PERIOD_COUNT * OUT_CHANNELS_DEFAULT)

// mixer sleep time
// about 1 frame periods in 48000 hz
#define MIXER_SLEEP_US 20000

struct ext_mixer_pipeline {
  int16_t buffer[MIXER_BUFFER_SIZE];
  unsigned int position;
};

struct ext_mixer_thread {
  pthread_mutex_t lock;
  struct ext_mixer_pipeline pipeline;
  pthread_t thread;
  Hashmap *pipeline_map;
  bool exit_flag;
};

struct ext_pcm {
  struct pcm *pcm;
  unsigned int ref_count;
  struct ext_mixer_thread mixer;
};

struct ext_pcm *ext_pcm_open_default(unsigned int card, unsigned int device,
                             unsigned int flags, struct pcm_config *config);
struct ext_pcm *ext_pcm_open_hfp(unsigned int card, unsigned int device,
                             unsigned int flags, struct pcm_config *config);
int ext_pcm_close(struct ext_pcm *ext_pcm, const char *bus_address);
int ext_pcm_is_ready(struct ext_pcm *ext_pcm);
int ext_pcm_write(struct ext_pcm *ext_pcm, const char *bus_address,
                  const void *data, unsigned int count);
const char *ext_pcm_get_error(struct ext_pcm *ext_pcm);
unsigned int ext_pcm_frames_to_bytes(struct ext_pcm *ext_pcm,
                                     unsigned int frames);
unsigned int ext_pcm_bytes_to_frames(struct ext_pcm *ext_pcm,
                                     unsigned int bytes);

#endif  // EXT_PCM_H
