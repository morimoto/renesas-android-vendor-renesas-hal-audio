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

#ifndef AUDIO_HW_H
#define AUDIO_HW_H

#include <pthread.h>

#include <cutils/hashmap.h>
#include <hardware/audio.h>
#include <tinyalsa/asoundlib.h>
#include <audio_utils/resampler.h>

#include "platform/audio_hal_types.h"
#include "audio_vbuffer.h"
#include "ext_pcm.h"

struct hfp_call {
    struct generic_stream_in *mic_input;
    struct generic_stream_in *hfp_input;
    struct generic_stream_out *hfp_output;
    struct generic_stream_out *headset_output;

    unsigned int hfp_volume;

    uint8_t stream_flag;
};

struct generic_audio_device {
  struct audio_hw_device device;  // Constant after init
  pthread_mutex_t lock;
  bool master_mute;             // Proteced by this->lock
  bool mic_mute;                // Proteced by this->lock
  struct device_card *device_cards;
  Hashmap *out_bus_stream_map;  // Extended field. Constant after init
  audio_mode_t mode;

  struct hfp_call hfp_call;

  int64_t sleep_ms;
};

struct generic_stream_out {
  struct audio_stream_out stream;  // Constant after init
  pthread_mutex_t lock;
  struct generic_audio_device *dev;  // Constant after init
  audio_devices_t device;            // Protected by this->lock
  struct audio_config req_config;    // Constant after init
  struct pcm_config pcm_config;      // Constant after init
  char *bus_address;                 // Extended field. Constant after init
  struct audio_gain gain_stage;      // Constant after init
  float amplitude_ratio;             // Protected by this->lock

  // Time & Position Keeping
  bool standby;                    // Protected by this->lock
  uint64_t underrun_position;      // Protected by this->lock
  struct timespec underrun_time;   // Protected by this->lock
  uint64_t last_write_time_us;     // Protected by this->lock
  uint64_t frames_total_buffered;  // Protected by this->lock
  uint64_t frames_written;         // Protected by this->lock
  uint64_t frames_rendered;        // Protected by this->lock

  // Mixer
  struct ext_pcm* ext_pcm;
  audio_vbuffer_t *write_bus;

  // Resampling
  struct resampler_itfe *resampler; // Protected by this->lock
  void *resampler_buffer;           // Protected by this->lock
  //size_t resampler_buffer_frame_count;
};

struct generic_stream_in {
  struct audio_stream_in stream;  // Constant after init
  pthread_mutex_t lock;
  struct generic_audio_device *dev;  // Constant after init
  audio_devices_t device;            // Protected by this->lock
  struct audio_config req_config;    // Constant after init
  struct pcm *pcm;                   // Protected by this->lock
  struct pcm_config pcm_config;      // Constant after init
  audio_vbuffer_t ringbuffer;        // Protected by this->lock
  char *bus_address;                 // Extended field. Constant after init

  // Time & Position Keeping
  bool standby;                       // Protected by this->lock
  int64_t standby_position;           // Protected by this->lock
  struct timespec standby_exit_time;  // Protected by this->lock
  int64_t standby_frames_read;        // Protected by this->lock

  // Worker
  pthread_t worker_thread;     // Constant after init
  pthread_cond_t worker_wake;  // Protected by this->lock
  bool worker_standby;         // Protected by this->lock
  bool worker_exit;            // Protected by this->lock

  // Resampling
  struct resampler_itfe *resampler; // Protected by this->lock
  void *resampler_buffer; // Protected by this->lock
  // size_t resampler_buffer_frame_count;

  // Channels adjust (SK/KF)
  void *adjustment_buffer;     // size of audio_vbuffer frame count
};

#endif  // AUDIO_HW_H
