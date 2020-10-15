/*
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

#define LOG_TAG "audio_hw_generic"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sched.h>
#include <unistd.h>

#include <log/log.h>
#include <cutils/str_parms.h>
#include <utils/threads.h>

#include "ext_pcm.h"
#include "buffer_utils.h"

static pthread_mutex_t shared_ext_pcm_init_lock = PTHREAD_MUTEX_INITIALIZER;
static struct ext_pcm *shared_ext_pcm = NULL;

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

// pcm macros for struct ext_mixer_thread *
static const size_t mixer_pcm_buffer_count = 3;
#define mixer_frame_count(mixer) \
  ((size_t) ((mixer)->pcm_conf.config.period_size * (mixer)->pcm_conf.config.period_count * mixer_pcm_buffer_count))

#define mixer_out_format_bytes(mixer) \
  ((size_t) (pcm_format_to_bits((mixer)->pcm_conf.config.format) >> 3))
#define mixer_out_channels(mixer) \
  ((size_t) ((mixer)->pcm_conf.config.channels))
#define mixer_out_frames_to_bytes(mixer, frames) \
  (((size_t) (frames)) * mixer_out_channels(mixer) * mixer_out_format_bytes(mixer))
#define mixer_out_bytes_to_frames(mixer, bytes) \
  (((size_t) (bytes)) / (mixer_out_channels(mixer) * mixer_out_format_bytes(mixer)))
#define mixer_out_byte_count(mixer) \
  (mixer_out_frames_to_bytes(mixer, mixer_frame_count(mixer)))

#define mixer_in_channels(mixer) \
  ((size_t) ((mixer)->bus_conf.channels))
#define mixer_in_format_bytes(mixer) \
  ((size_t) ((mixer)->bus_conf.format_bytes))
#define mixer_in_bytes_to_frames(mixer, bytes) \
  (((size_t) (bytes)) / (mixer_in_channels(mixer) * mixer_in_format_bytes(mixer)))
#define mixer_in_frames_to_bytes(mixer, frames) \
  (((size_t) (frames)) * mixer_in_channels(mixer) * mixer_in_format_bytes(mixer))
#define mixer_in_byte_count(mixer) \
  (mixer_in_frames_to_bytes(mixer, mixer_frame_count(mixer)))

static int mixer_open_pcm(struct ext_mixer_thread *mixer) {
  ALOGE("%s: opening PCM", __func__);
  mixer->pcm = pcm_open(mixer->pcm_conf.card,
                        mixer->pcm_conf.device,
                        mixer->pcm_conf.flags,
                        &mixer->pcm_conf.config);
  if(!pcm_is_ready(mixer->pcm)) {
    ALOGE("%s: pcm_open failed (%s)", __func__, pcm_get_error(mixer->pcm));

    atomic_store_explicit(&mixer->error, -1, memory_order_relaxed);
    atomic_store_explicit(&mixer->pcm_init_done, 1, memory_order_release);
    return -EINVAL;
  }

  const size_t read_buffer_size = mixer_in_byte_count(mixer);
  const size_t write_buffer_size = mixer_out_byte_count(mixer);
  mixer->mixed_buffer = calloc(read_buffer_size, 1);
  mixer->read_buffer = calloc(read_buffer_size, 1);
  mixer->write_buffer = calloc(write_buffer_size, 1);
  LOG_FATAL_IF(!mixer->mixed_buffer || !mixer->read_buffer || !mixer->write_buffer,
               "%s: could not allocate mixer buffers",
               __func__);

  atomic_store_explicit(&mixer->pcm_init_done, 1, memory_order_release);
  return 0;
}

static void mixer_close_pcm(struct ext_mixer_thread *mixer) {
  if(mixer->pcm) {
    pcm_close(mixer->pcm);
    mixer->pcm = NULL;
  }
  free(mixer->mixed_buffer);
  free(mixer->read_buffer);
  free(mixer->write_buffer);
}

static int mixer_init_bus(struct ext_mixer_thread *mixer, audio_vbuffer_t **bus) {
  audio_vbuffer_t *new_bus = calloc(1, sizeof(*new_bus));
  if(!new_bus) {
    return -EINVAL;
  }

  int err = audio_vbuffer_init(new_bus,
                               mixer->bus_conf.frame_count,
                               mixer->bus_conf.frame_size);
  if(err) {
    free(new_bus);
    return err;
  }

  *bus = new_bus;
  return 0;
}

static audio_vbuffer_t *get_mixer_write_bus(struct ext_pcm *ext_pcm, const char *bus_address) {
  pthread_mutex_lock(&ext_pcm->mixer.bus_map_lock);
  audio_vbuffer_t *bus = (audio_vbuffer_t *) hashmapGet(ext_pcm->mixer.bus_map, (void*)bus_address);
  if (!bus) {
    if(mixer_init_bus(&ext_pcm->mixer, &bus)) {
      LOG_ALWAYS_FATAL("mixer: could not allocate bus");
    }

    hashmapPut(ext_pcm->mixer.bus_map, (void*)bus_address, bus);
  }
  pthread_mutex_unlock(&ext_pcm->mixer.bus_map_lock);
  return bus;
}

static void remove_mixer_write_bus(struct ext_pcm *ext_pcm, const char *bus_address) {
  pthread_mutex_lock(&ext_pcm->mixer.bus_map_lock);
  audio_vbuffer_t *removed_bus = (audio_vbuffer_t *) hashmapRemove(ext_pcm->mixer.bus_map,
                                                                   (void *)bus_address);
  pthread_mutex_unlock(&ext_pcm->mixer.bus_map_lock);
  audio_vbuffer_destroy(removed_bus);
  free(removed_bus);
}

static bool mixer_calc_mix_position(__unused void *key, void *value, void *context) {
  struct ext_mixer_thread *mixer = (struct ext_mixer_thread *)context;
  audio_vbuffer_t *vbuf = (audio_vbuffer_t *)value;
  size_t bus_frames = audio_vbuffer_live(vbuf);
  if(bus_frames != 0) {
    mixer->mixed_frames = MIN(mixer->mixed_frames, bus_frames);
  }
  return true;
}

static bool mixer_thread_mix_int16(__unused void *key, void *value, void *context) {
  struct ext_mixer_thread *mixer = (struct ext_mixer_thread *)context;
  audio_vbuffer_t *bus = (audio_vbuffer_t *)value;

  size_t read_frames = audio_vbuffer_read(bus, mixer->read_buffer, mixer->mixed_frames);
  // read_frames should not be less than mixer->mixed_frames unless
  // the bus was empty (e.g. before writing started)
  if(read_frames) {
    int16_t *mixed_buffer_int16 = (int16_t *)mixer->mixed_buffer;
    int16_t *read_buffer_int16 = (int16_t *)mixer->read_buffer;

    size_t int16_count = mixer_in_frames_to_bytes(mixer, mixer->mixed_frames)
                         / sizeof(int16_t);
    for (size_t i = 0; i < int16_count; i++) {
      int32_t mixed = mixed_buffer_int16[i] + read_buffer_int16[i];
      if (mixed > INT16_MAX) mixed_buffer_int16[i] = INT16_MAX;
      else if (mixed < INT16_MIN) mixed_buffer_int16[i] = INT16_MIN;
      else mixed_buffer_int16[i] = (int16_t)mixed;
    }
  }
  return true;
}

static void *mixer_thread_loop(void *context) {
  ALOGD("%s: __enter__", __func__);
  struct ext_mixer_thread *mixer = (struct ext_mixer_thread *)context;

  androidSetThreadPriority(gettid(), ANDROID_PRIORITY_URGENT_AUDIO);

  if(mixer_open_pcm(mixer)) {
    return NULL;
  }

  const size_t mixed_frames_max = pcm_get_buffer_size(mixer->pcm);

  while (!atomic_load_explicit(&mixer->do_exit, memory_order_relaxed)) {
    mixer->mixed_frames = mixed_frames_max;

    // calculate bus position for mix and
    // combine the output from every bus into one output buffer
    pthread_mutex_lock(&mixer->bus_map_lock);
    hashmapForEach(mixer->bus_map, mixer_calc_mix_position, (void*)mixer);

    hashmapForEach(mixer->bus_map, mixer_thread_mix_int16, (void*)mixer);
    pthread_mutex_unlock(&mixer->bus_map_lock);

    size_t mixed_bytes = mixer_in_frames_to_bytes(mixer, mixer->mixed_frames);

    // write result (blocking)
    if(mixer->pcm_write(mixer, mixed_bytes)) {
        atomic_store_explicit(&mixer->error, -1, memory_order_relaxed);
        ALOGE("%s: pcm_write failed %s", __func__, pcm_get_error(mixer->pcm));
        return NULL;
    }

    memset(mixer->mixed_buffer, 0, mixed_bytes);
  }

  mixer_close_pcm(mixer);

  ALOGD("%s __exit__", __func__);
  return NULL;
}

static unsigned int mixer_pcm_write(struct ext_mixer_thread *mixer, size_t bytes) {
  return pcm_write(mixer->pcm, mixer->mixed_buffer, bytes);
}

static unsigned int mixer_pcm_write_expand(struct ext_mixer_thread *mixer, size_t bytes) {
  size_t in_frames = mixer_in_bytes_to_frames(mixer, bytes);
  size_t out_bytes = mixer_out_frames_to_bytes(mixer, in_frames);

  // NOTE: assumes PCM and ext_bus formats are the same
  audio_buffer_expand(mixer->write_buffer, mixer_out_channels(mixer),
                      mixer->mixed_buffer, mixer_in_channels(mixer),
                      in_frames, mixer_in_format_bytes(mixer));

  return pcm_write(mixer->pcm, mixer->write_buffer, out_bytes);
}

static unsigned int mixer_pcm_write_shrink(struct ext_mixer_thread *mixer, size_t bytes) {
  size_t in_frames = mixer_in_bytes_to_frames(mixer, bytes);
  size_t out_bytes = mixer_out_frames_to_bytes(mixer, in_frames);

  // NOTE: assumes PCM and ext_bus formats are the same
  audio_buffer_shrink(mixer->write_buffer, mixer_out_channels(mixer),
                      mixer->mixed_buffer, mixer_in_channels(mixer),
                      in_frames, mixer_in_format_bytes(mixer));

  return pcm_write(mixer->pcm, mixer->write_buffer, out_bytes);
}


static void wait_for_mixer_pcm(struct ext_mixer_thread *mixer) {
  while(!atomic_load_explicit(&mixer->pcm_init_done, memory_order_acquire)) sched_yield();
}

static void ext_pcm_init(struct ext_pcm** ext_pcm, int bus_count,
                         unsigned int card, unsigned int device,
                         unsigned int flags, struct pcm_config *config) {
  struct ext_pcm *new_ext_pcm = calloc(1, sizeof(*new_ext_pcm));
  *ext_pcm = NULL;

  if(!new_ext_pcm) {
    ALOGE("ext_pcm: could not allocate ext_pcm");
    return;
  }

  atomic_store(&new_ext_pcm->ref_count, 0);

  new_ext_pcm->mixer.bus_map = hashmapCreate(bus_count, str_hash_fn, str_eq);
  new_ext_pcm->mixer.pcm = NULL;
  new_ext_pcm->mixer.pcm_conf.card = card;
  new_ext_pcm->mixer.pcm_conf.device = device;
  new_ext_pcm->mixer.pcm_conf.flags = flags;
  new_ext_pcm->mixer.pcm_conf.config = *config;

  new_ext_pcm->mixer.bus_conf.frame_count = mixer_frame_count(&new_ext_pcm->mixer);
  new_ext_pcm->mixer.bus_conf.channels = 2; // stereo only
  new_ext_pcm->mixer.bus_conf.format_bytes = mixer_out_format_bytes(&new_ext_pcm->mixer); // same as PCM
  new_ext_pcm->mixer.bus_conf.frame_size = new_ext_pcm->mixer.bus_conf.channels
                                           * new_ext_pcm->mixer.bus_conf.format_bytes;

  if(mixer_in_channels(&new_ext_pcm->mixer) > mixer_out_channels(&new_ext_pcm->mixer)) {
    new_ext_pcm->mixer.pcm_write = mixer_pcm_write_shrink;
  } else if(mixer_in_channels(&new_ext_pcm->mixer) < mixer_out_channels(&new_ext_pcm->mixer)) {
    new_ext_pcm->mixer.pcm_write = mixer_pcm_write_expand;
  } else {
    new_ext_pcm->mixer.pcm_write = mixer_pcm_write;
  }

  // set thread flags
  atomic_init(&new_ext_pcm->mixer.do_exit, 0);
  atomic_init(&new_ext_pcm->mixer.error, 0);
  atomic_init(&new_ext_pcm->mixer.pcm_init_done, 0);

  pthread_mutex_init(&new_ext_pcm->mixer.bus_map_lock, (const pthread_mutexattr_t *)NULL);
  pthread_create(&new_ext_pcm->mixer.thread, (const pthread_attr_t *)NULL,
                 mixer_thread_loop, &new_ext_pcm->mixer);

  wait_for_mixer_pcm(&new_ext_pcm->mixer);
  *ext_pcm = new_ext_pcm;
}

struct ext_pcm *ext_pcm_open_default(unsigned int card, unsigned int device,
                             unsigned int flags, struct pcm_config *config) {
  pthread_mutex_lock(&shared_ext_pcm_init_lock);
  if (shared_ext_pcm == NULL) {
    ext_pcm_init(&shared_ext_pcm, 8, card, device, flags, config);
    LOG_ALWAYS_FATAL_IF(!shared_ext_pcm, "ext_pcm: %s failed", __func__);
  }
  pthread_mutex_unlock(&shared_ext_pcm_init_lock);

  atomic_fetch_add_explicit(&shared_ext_pcm->ref_count, 1, memory_order_relaxed);
  return shared_ext_pcm;
}

struct ext_pcm *ext_pcm_open_hfp(unsigned int card, unsigned int device,
                                 unsigned int flags, struct pcm_config *config) {

  struct ext_pcm *ext_pcm = NULL;
  ext_pcm_init(&ext_pcm, 1, card, device, flags, config);
  LOG_ALWAYS_FATAL_IF(!ext_pcm, "ext_pcm: %s failed", __func__);

  return ext_pcm;
}

static bool mixer_free_bus(__unused void *key, void *value, void *context) {
  audio_vbuffer_t *bus = (audio_vbuffer_t *)value;
  audio_vbuffer_destroy(bus);
  free(bus);
  return true;
}

int ext_pcm_close(struct ext_pcm *ext_pcm, const char *bus_address) {
  if (ext_pcm == NULL) {
    return -EINVAL;
  }

  unsigned int ref_count = atomic_fetch_sub_explicit(&ext_pcm->ref_count, 1,
                                                     memory_order_relaxed);
  remove_mixer_write_bus(ext_pcm, bus_address);

  if (ref_count <= 1) {
    // stop thread
    atomic_store_explicit(&ext_pcm->mixer.do_exit, 1, memory_order_relaxed);
    pthread_join(ext_pcm->mixer.thread, NULL);

    // clean map contents and destroy it
    hashmapForEach(ext_pcm->mixer.bus_map, mixer_free_bus, (void *)NULL);
    hashmapFree(ext_pcm->mixer.bus_map);
    pthread_mutex_destroy(&ext_pcm->mixer.bus_map_lock);

    // free ext_pcm
    pthread_mutex_lock(&shared_ext_pcm_init_lock);
    if (ext_pcm == shared_ext_pcm)
      shared_ext_pcm = NULL;
    pthread_mutex_unlock(&shared_ext_pcm_init_lock);

    free(ext_pcm);
  }


  return 0;
}

int ext_pcm_is_ready(struct ext_pcm *ext_pcm) {
  if (ext_pcm == NULL) {
    return -EINVAL;
  }

  return !atomic_load_explicit(&ext_pcm->mixer.error, memory_order_relaxed);
}

audio_vbuffer_t *ext_pcm_get_write_bus(struct ext_pcm *ext_pcm, const char *bus_address) {
  if (ext_pcm == NULL) {
    return NULL;
  }

  return get_mixer_write_bus(ext_pcm, bus_address);
}

const char *ext_pcm_get_error(struct ext_pcm *ext_pcm) {
  if (ext_pcm == NULL || ext_pcm->mixer.pcm == NULL) {
    return NULL;
  }

  return pcm_get_error(ext_pcm->mixer.pcm);
}

size_t ext_pcm_frames_to_bytes(struct ext_pcm *ext_pcm,
                               size_t frames) {
  if (ext_pcm == NULL) {
    return 0;
  }

  return mixer_in_frames_to_bytes(&ext_pcm->mixer, frames);
}

size_t ext_pcm_bytes_to_frames(struct ext_pcm *ext_pcm,
                               size_t bytes) {
  if (ext_pcm == NULL) {
    return 0;
  }

  return mixer_in_bytes_to_frames(&ext_pcm->mixer, bytes);
}
