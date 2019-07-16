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

#define LOG_TAG "audio_hw_generic"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <log/log.h>
#include <cutils/str_parms.h>

#include "ext_pcm.h"

static pthread_mutex_t ext_pcm_init_lock = PTHREAD_MUTEX_INITIALIZER;
static struct ext_pcm *shared_ext_pcm = NULL;
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

/* copied from libcutils/str_parms.c */
static bool str_eq(void *key_a, void *key_b) {
  return !strcmp((const char *)key_a, (const char *)key_b);
}

/**
 * use djb hash unless we find it inadequate.
 * copied from libcutils/str_parms.c
 */
#ifdef __clang__
__attribute__((no_sanitize("integer")))
#endif
static int str_hash_fn(void *str) {
  uint32_t hash = 5381;
  char *p;
  for (p = str; p && *p; p++) {
    hash = ((hash << 5) + hash) + *p;
  }
  return (int)hash;
}

static bool check_mixer_buffers(__unused void *key, void *value, void *context) {
  struct ext_mixer_pipeline *pipeline = (struct ext_mixer_pipeline *)value;
  int *counter = (int *)context;

  if (pipeline->position > 0) {
    ++(*counter);
  }
  return true;
}

static bool mixer_thread_mix(__unused void *key, void *value, void *context) {
  struct ext_mixer_pipeline *pipeline_out = (struct ext_mixer_pipeline *)context;
  struct ext_mixer_pipeline *pipeline_in = (struct ext_mixer_pipeline *)value;
  pipeline_out->position = MAX(pipeline_out->position, pipeline_in->position);
  for (int i = 0; i < pipeline_out->position; i++) {
    float mixed = pipeline_out->buffer[i] + pipeline_in->buffer[i];
    if (mixed > INT16_MAX) pipeline_out->buffer[i] = INT16_MAX;
    else if (mixed < INT16_MIN) pipeline_out->buffer[i] = INT16_MIN;
    else pipeline_out->buffer[i] = (int16_t)mixed;
  }
  memset(pipeline_in, 0, sizeof(struct ext_mixer_pipeline));
  return true;
}

static void *mixer_thread_loop(void *context) {
  ALOGD("%s: __enter__", __func__);
  struct ext_pcm *ext_pcm = (struct ext_pcm *)context;

  pthread_mutex_lock(&ext_pcm->mixer_lock);
  do {
    ext_pcm->mixer_pipeline.position = 0;
    // Combine the output from every pipeline into one output buffer
    hashmapForEach(ext_pcm->mixer_pipeline_map, mixer_thread_mix,
        &ext_pcm->mixer_pipeline);
    if (ext_pcm->mixer_pipeline.position > 0) {
      pcm_write(ext_pcm->pcm, (void *)ext_pcm->mixer_pipeline.buffer,
          ext_pcm->mixer_pipeline.position * sizeof(int16_t));
    }
    memset(&ext_pcm->mixer_pipeline, 0, sizeof(struct ext_mixer_pipeline));
    // will unlock and lock automatically
    pthread_cond_wait(&ext_pcm->mixer_wake, &ext_pcm->mixer_lock);
  } while (!ext_pcm->mixer_exit_flag);
  pthread_mutex_unlock(&ext_pcm->mixer_lock);
  return NULL;
}
static int mixer_pipeline_write(struct ext_pcm *ext_pcm, const char *bus_address,
                                const void *data, unsigned int count) {
  pthread_mutex_lock(&ext_pcm->mixer_lock);
  struct ext_mixer_pipeline *pipeline = hashmapGet(
      ext_pcm->mixer_pipeline_map, bus_address);
  if (!pipeline) {
    pipeline = calloc(1, sizeof(struct ext_mixer_pipeline));
    hashmapPut(ext_pcm->mixer_pipeline_map, bus_address, pipeline);
  }
  unsigned int byteCount = MIN(count,
      (MIXER_BUFFER_SIZE - pipeline->position) * sizeof(int16_t));
  unsigned int int16Count = byteCount / sizeof(int16_t);
  if (int16Count > 0) {
    memcpy(&pipeline->buffer[pipeline->position], data, byteCount);
    pipeline->position += int16Count;
  }
  pthread_mutex_unlock(&ext_pcm->mixer_lock);

  int filled_buffers = 0;
  hashmapForEach(ext_pcm->mixer_pipeline_map, check_mixer_buffers,
        &filled_buffers);

  if (filled_buffers == ext_pcm->ref_count) {
    pthread_cond_signal(&ext_pcm->mixer_wake);
  }
  return 0;
}

struct ext_pcm *ext_pcm_open_default(unsigned int card, unsigned int device,
                             unsigned int flags, struct pcm_config *config) {
  pthread_mutex_lock(&ext_pcm_init_lock);
  if (shared_ext_pcm == NULL) {
    shared_ext_pcm = calloc(1, sizeof(struct ext_pcm));
    pthread_mutex_init(&shared_ext_pcm->lock, (const pthread_mutexattr_t *) NULL);
    shared_ext_pcm->pcm = pcm_open(card, device, flags, config);
    pthread_mutex_init(&shared_ext_pcm->mixer_lock, (const pthread_mutexattr_t *)NULL);
    pthread_cond_init(&shared_ext_pcm->mixer_wake, NULL);
    pthread_create(&shared_ext_pcm->mixer_thread, (const pthread_attr_t *)NULL,
            mixer_thread_loop, shared_ext_pcm);
    shared_ext_pcm->mixer_pipeline_map = hashmapCreate(8, str_hash_fn, str_eq);
    shared_ext_pcm->ref_count = 0;
  }
  pthread_mutex_unlock(&ext_pcm_init_lock);

  pthread_mutex_lock(&shared_ext_pcm->lock);
  shared_ext_pcm->ref_count += 1;
  shared_ext_pcm->mixer_exit_flag = false;
  pthread_mutex_unlock(&shared_ext_pcm->lock);

  return shared_ext_pcm;
}

struct ext_pcm *ext_pcm_open_hfp(unsigned int card, unsigned int device,
                             unsigned int flags, struct pcm_config *config) {

  struct ext_pcm *ext_pcm = NULL;
  pthread_mutex_lock(&ext_pcm_init_lock);

  ext_pcm = calloc(1, sizeof(struct ext_pcm));
  pthread_mutex_init(&ext_pcm->lock, (const pthread_mutexattr_t *) NULL);
  ext_pcm->pcm = pcm_open(card, device, flags, config);
  pthread_mutex_init(&ext_pcm->mixer_lock, (const pthread_mutexattr_t *)NULL);
  pthread_cond_init(&ext_pcm->mixer_wake, NULL);
  pthread_create(&ext_pcm->mixer_thread, (const pthread_attr_t *)NULL,
          mixer_thread_loop, ext_pcm);
  ext_pcm->mixer_pipeline_map = hashmapCreate(1, str_hash_fn, str_eq);
  ext_pcm->ref_count = 1;
  pthread_mutex_unlock(&ext_pcm_init_lock);

  pthread_mutex_lock(&ext_pcm->lock);
  ext_pcm->mixer_exit_flag = false;
  pthread_mutex_unlock(&ext_pcm->lock);

  return ext_pcm;
}

static bool mixer_free_pipeline(__unused void *key, void *value, void *context) {
  struct ext_mixer_pipeline *pipeline = (struct ext_mixer_pipeline *)value;
  free(pipeline);
  return true;
}

int ext_pcm_close(struct ext_pcm *ext_pcm, const char *bus_address) {
  if (ext_pcm == NULL || ext_pcm->pcm == NULL) {
    return -EINVAL;
  }

  pthread_mutex_lock(&ext_pcm->lock);
  ext_pcm->ref_count -= 1;
  hashmapRemove(ext_pcm->mixer_pipeline_map, bus_address);
  pthread_mutex_unlock(&ext_pcm->lock);

  pthread_mutex_lock(&ext_pcm_init_lock);
  if (ext_pcm->ref_count <= 0) {
    pthread_mutex_lock(&ext_pcm->lock);
    ext_pcm->mixer_exit_flag = true;
    pthread_cond_signal(&ext_pcm->mixer_wake);
    pthread_mutex_unlock(&ext_pcm->lock);
    pcm_close(ext_pcm->pcm);
    hashmapForEach(ext_pcm->mixer_pipeline_map, mixer_free_pipeline,
        (void *)NULL);
    hashmapFree(ext_pcm->mixer_pipeline_map);
    pthread_join(ext_pcm->mixer_thread, NULL);
    pthread_cond_destroy(&ext_pcm->mixer_wake);
    pthread_mutex_destroy(&ext_pcm->mixer_lock);
    pthread_mutex_destroy(&ext_pcm->lock);
    if (ext_pcm == shared_ext_pcm)
      shared_ext_pcm = NULL;
    free(ext_pcm);
  }
  pthread_mutex_unlock(&ext_pcm_init_lock);
  return 0;
}

int ext_pcm_is_ready(struct ext_pcm *ext_pcm) {
  if (ext_pcm == NULL || ext_pcm->pcm == NULL) {
    return 0;
  }

  return pcm_is_ready(ext_pcm->pcm);
}

int ext_pcm_write(struct ext_pcm *ext_pcm, const char *address,
                  const void *data, unsigned int count) {
  if (ext_pcm == NULL || ext_pcm->pcm == NULL) {
    return -EINVAL;
  }

  return mixer_pipeline_write(ext_pcm, address, data, count);
}

const char *ext_pcm_get_error(struct ext_pcm *ext_pcm) {
  if (ext_pcm == NULL || ext_pcm->pcm == NULL) {
    return NULL;
  }

  return pcm_get_error(ext_pcm->pcm);
}

unsigned int ext_pcm_frames_to_bytes(struct ext_pcm *ext_pcm,
                                     unsigned int frames) {
  if (ext_pcm == NULL || ext_pcm->pcm == NULL) {
    return -EINVAL;
  }

  return pcm_frames_to_bytes(ext_pcm->pcm, frames);
}

unsigned int ext_pcm_bytes_to_frames(struct ext_pcm *ext_pcm,
                                     unsigned int bytes) {
  if (ext_pcm == NULL || ext_pcm->pcm == NULL) {
    return -EINVAL;
  }

  return pcm_bytes_to_frames(ext_pcm->pcm, bytes);
}
