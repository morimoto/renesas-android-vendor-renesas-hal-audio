/*
 * Copyright (C) 2017 The Android Open Source Project
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

/**
 * Derived from goldfish/audio/audio_hw.c
 * Changes made to adding support of AUDIO_DEVICE_OUT_BUS
 */

#define LOG_TAG "audio_hw_generic"

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>

#include <log/log.h>
#include <cutils/str_parms.h>
#include <cutils/list.h>

#include <hardware/hardware.h>
#include <system/audio.h>

#include <audio_utils/channels.h>

#include "audio_hw.h"
#include "ext_pcm.h"
#include "buffer_utils.h"

// device specific definitions
#include "platform_dependencies.h"

#define OUT_PERIOD_MS 15
#define OUT_PERIOD_COUNT 4

#define OUT_PERIOD_SIZE 512
#define IN_PERIOD_MS 15
#define IN_PERIOD_COUNT 4
#define IN_PERIOD_SIZE 512

// defined externally
#ifndef OUT_CHANNELS_DEFAULT
#define OUT_CHANNELS_DEFAULT 8
#endif // OUT_CHANNELS_DEFAULT

#ifndef IN_CHANNELS_DEFAULT
#define IN_CHANNELS_DEFAULT 6
#endif // IN_CHANNELS_DEFAULT

#ifndef IN_CHANNELS_FM
#define IN_CHANNELS_FM 2
#endif // IN_CHANNELS_FM

#ifndef IN_CHANNELS_HFP
#define IN_CHANNELS_HFP 2
#endif // IN_CHANNELS_HFP

#ifndef OUT_CHANNELS_HFP
#define OUT_CHANNELS_HFP 2
#endif // OUT_CHANNELS_HFP

#ifndef PCM_CARD_DEFAULT
#define PCM_CARD_DEFAULT 0
#endif // PCM_CARD_DEFAULT

#ifndef PCM_DEVICE_DEFAULT
#define PCM_DEVICE_DEFAULT 0
#endif // PCM_DEVICE_DEFAULT

#ifndef PCM_CARD_FM
#define PCM_CARD_FM UINT32_MAX
#endif // PCM_CARD_FM

#ifndef PCM_DEVICE_FM
#define PCM_DEVICE_FM UINT32_MAX
#endif // PCM_DEVICE_FM

#ifndef PCM_CARD_HFP
#define PCM_CARD_HFP UINT32_MAX
#endif // PCM_CARD_FM

#ifndef PCM_DEVICE_HFP
#define PCM_DEVICE_HFP UINT32_MAX
#endif // PCM_DEVICE_FM

#ifndef DEFAULT_OUT_SAMPLING_RATE
#define DEFAULT_OUT_SAMPLING_RATE   48000
#endif // DEFAULT_OUT_SAMPLING_RATE

#ifndef DEFAULT_IN_SAMPLING_RATE
#define DEFAULT_IN_SAMPLING_RATE   48000
#endif // DEFAULT_IN_SAMPLING_RATE

#ifndef DEFAULT_HFP_SAMPLING_RATE
#define DEFAULT_HFP_SAMPLING_RATE   16000
#endif // DEFAULT_HFP_SAMPLING_RATE

#define PTHREAD_MUTEX_LOCK(lock) \
    pthread_mutex_lock(lock);

#define PTHREAD_MUTEX_UNLOCK(lock) \
    pthread_mutex_unlock(lock);

#define SLEEP_MAX 200000
#define SLEEP_MS 10000

#define HFP_IN_ACTIVE_FLAG 0x1
#define MIC_IN_ACTIVE_FLAG 0x2
#define HFP_OUT_ACTIVE_FLAG 0x4
#define ALL_ACTIVE_FLAG (HFP_IN_ACTIVE_FLAG | MIC_IN_ACTIVE_FLAG | HFP_OUT_ACTIVE_FLAG)

#define _bool_str(x) ((x)?"true":"false")

static int adev_get_mic_mute(const struct audio_hw_device *dev, bool *state);

static struct pcm_config pcm_config_out_default = {
    .channels = OUT_CHANNELS_DEFAULT,
    .rate = DEFAULT_OUT_SAMPLING_RATE,
    .period_size = OUT_PERIOD_SIZE,
    .period_count = OUT_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
//    .start_threshold = 0,
};

struct pcm_config pcm_config_out_hfp = {
    .channels = OUT_CHANNELS_HFP,
    .rate = DEFAULT_HFP_SAMPLING_RATE,
    .period_size = OUT_PERIOD_SIZE,
    .period_count = OUT_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE
};

static struct pcm_config pcm_config_in_default = {
    .channels = IN_CHANNELS_DEFAULT,
    .rate = DEFAULT_IN_SAMPLING_RATE,
    .period_size = IN_PERIOD_SIZE,
    .period_count = IN_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
//    .start_threshold = 0,
//    .stop_threshold = INT_MAX,
};

static struct pcm_config pcm_config_in_fm = {
    .channels = IN_CHANNELS_FM,
    .rate = DEFAULT_IN_SAMPLING_RATE,
    .period_size = IN_PERIOD_SIZE,
    .period_count = IN_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
};

struct pcm_config pcm_config_in_hfp = {
    .channels = IN_CHANNELS_HFP,
    .rate = DEFAULT_HFP_SAMPLING_RATE,
    .period_size = IN_PERIOD_SIZE,
    .period_count = IN_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE
};

static pthread_mutex_t adev_init_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned int audio_device_ref_count = 0;

typedef struct {
    struct listnode      list;
    struct audio_patch   patch;
} patch_list;

struct listnode* patch_head = NULL;

static int set_route_by_array(struct mixer *mixer, struct route_setting *route,
                              int enable)
{
    struct mixer_ctl *ctl;
    unsigned int i, j;

    /* Go through the route array and set each value */
    i = 0;
    while (route[i].ctl_name) {
        ctl = mixer_get_ctl_by_name(mixer, route[i].ctl_name);
        if (!ctl)
            return -EINVAL;

        if (route[i].strval) {
            if (enable)
                mixer_ctl_set_enum_by_string(ctl, route[i].strval);
            else
                mixer_ctl_set_enum_by_string(ctl, "Off");
        } else {
            /* This ensures multiple (i.e. stereo) values are set jointly */
            for (j = 0; j < mixer_ctl_get_num_values(ctl); j++) {
                if (enable)
                    mixer_ctl_set_value(ctl, j, route[i].intval);
                else
                    mixer_ctl_set_value(ctl, j, 0);
            }
        }
        i++;
    }

    return 0;
}

static int open_mixers_by_array(struct device_card *cards)
{
    //unsigned int counter = 0;
    if (!cards) {
        return -EINVAL;
    }
    for (unsigned int counter = 0; cards[counter].card != UINT32_MAX; counter++) {
        cards[counter].mixer = mixer_open(cards[counter].card);
        if (!cards[counter].mixer) {
            ALOGE("Unable to open the mixer for card %d, aborting.", cards[counter].card);
            for (unsigned int close_counter = 0; close_counter < counter; close_counter++) {
                mixer_close(cards[close_counter].mixer);
                cards[close_counter].mixer = 0;
            }
            return -EINVAL;
        }
        if (set_route_by_array(cards[counter].mixer, cards[counter].defaults, 1) != 0) {
            ALOGE("Unable to set the route for card %d, aborting.", cards[counter].card);
            for (unsigned int close_counter = 0; close_counter < counter; close_counter++) {
                mixer_close(cards[close_counter].mixer);
                cards[close_counter].mixer = 0;
            }
            return -EINVAL;
        }
    }
    return 0;
}

static void close_mixers_by_array(struct device_card *cards)
{
    unsigned int counter = 0;
    if (!cards) {
        return;
    }
    while (cards[counter].card != UINT32_MAX) {
        if (cards[counter].mixer) {
            mixer_close(cards[counter].mixer);
            cards[counter].mixer = 0;
        }
    }
}

static uint32_t out_get_sample_rate(const struct audio_stream *stream) {
    struct generic_stream_out *out = (struct generic_stream_out *)stream;
    return out->req_config.sample_rate;
}

static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate) {
    return -ENOSYS;
}

static size_t out_get_buffer_size(const struct audio_stream *stream) {
    struct generic_stream_out *out = (struct generic_stream_out *)stream;
    int size = out->pcm_config.period_size *
                audio_stream_out_frame_size(&out->stream);

    return size;
}

static audio_channel_mask_t out_get_channels(const struct audio_stream *stream) {
    struct generic_stream_out *out = (struct generic_stream_out *)stream;
    return out->req_config.channel_mask;
}

static audio_format_t out_get_format(const struct audio_stream *stream) {
    struct generic_stream_out *out = (struct generic_stream_out *)stream;
    return out->req_config.format;
}

static int out_set_format(struct audio_stream *stream, audio_format_t format) {
    return -ENOSYS;
}

static int out_dump(const struct audio_stream *stream, int fd) {
    struct generic_stream_out *out = (struct generic_stream_out *)stream;
    pthread_mutex_lock(&out->lock);
    dprintf(fd, "\tout_dump:\n"
                "\t\taddress: %s\n"
                "\t\tsample rate: %u\n"
                "\t\tbuffer size: %zu\n"
                "\t\tchannel mask: %08x\n"
                "\t\tformat: %d\n"
                "\t\tdevice: %08x\n"
                "\t\tamplitude ratio: %f\n"
                "\t\taudio dev: %p\n\n",
                out->bus_address,
                out_get_sample_rate(stream),
                out_get_buffer_size(stream),
                out_get_channels(stream),
                out_get_format(stream),
                out->device,
                out->amplitude_ratio,
                out->dev);
    pthread_mutex_unlock(&out->lock);
    return 0;
}

static int out_set_parameters(struct audio_stream *stream, const char *kvpairs) {
    struct generic_stream_out *out = (struct generic_stream_out *)stream;
    struct str_parms *parms;
    char value[32];
    int ret;
    long val;
    char *end;

    pthread_mutex_lock(&out->lock);
    if (!out->standby) {
        //Do not support changing params while stream running
        ret = -ENOSYS;
    } else {
        parms = str_parms_create_str(kvpairs);
        ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING,
                                value, sizeof(value));
        if (ret >= 0) {
            errno = 0;
            val = strtol(value, &end, 10);
            if (errno == 0 && (end != NULL) && (*end == '\0') && ((int)val == val)) {
                out->device = (int)val;
                ret = 0;
            } else {
                ret = -EINVAL;
            }
        }
        str_parms_destroy(parms);
    }
    pthread_mutex_unlock(&out->lock);
    return ret;
}

static char *out_get_parameters(const struct audio_stream *stream, const char *keys) {
    struct generic_stream_out *out = (struct generic_stream_out *)stream;
    struct str_parms *query = str_parms_create_str(keys);
    char *str;
    char value[256];
    struct str_parms *reply = str_parms_create();
    int ret;

    ret = str_parms_get_str(query, AUDIO_PARAMETER_STREAM_ROUTING, value, sizeof(value));
    if (ret >= 0) {
        pthread_mutex_lock(&out->lock);
        str_parms_add_int(reply, AUDIO_PARAMETER_STREAM_ROUTING, out->device);
        pthread_mutex_unlock(&out->lock);
        str = strdup(str_parms_to_str(reply));
    } else {
        str = strdup(keys);
    }

    str_parms_destroy(query);
    str_parms_destroy(reply);
    return str;
}

static uint32_t out_get_latency(const struct audio_stream_out *stream) {
    struct generic_stream_out *out = (struct generic_stream_out *)stream;
    return (out->pcm_config.period_size * 1000) / out->pcm_config.rate;
}

static int out_set_volume(struct audio_stream_out *stream,
        float left, float right) {
    return -ENOSYS;
}

static void *out_write_worker(void *args) {
    struct generic_stream_out *out = (struct generic_stream_out *)args;
    struct ext_pcm *ext_pcm = NULL;
    uint8_t *buffer = NULL;
    int buffer_frames;
    int buffer_size;
    bool close_pcm = false;

    while (true) {
        pthread_mutex_lock(&out->lock);

        if (out->worker_standby || out->worker_exit) {
            close_pcm = true;
        }

        while (!close_pcm && audio_vbuffer_live(&out->buffer) == 0) {
            pthread_cond_wait(&out->worker_wake, &out->lock);
            if (out->worker_standby || out->worker_exit) {
                close_pcm = true;
            }
        }

        if (close_pcm) {
            if (ext_pcm) {
                ext_pcm_close(ext_pcm, out->bus_address); // Frees pcm
                ext_pcm = NULL;
                free(buffer);
                buffer=NULL;
            }

            if (out->worker_exit) {
                pthread_mutex_unlock(&out->lock);
                break;
            }

            close_pcm = false;

            pthread_mutex_lock(&out->dev->lock);
            if (out->device == AUDIO_DEVICE_OUT_BLUETOOTH_SCO) {
                out->dev->hfp_call.stream_flag |= HFP_OUT_ACTIVE_FLAG;
            }
            pthread_mutex_unlock(&out->dev->lock);

            pthread_cond_wait(&out->worker_wake, &out->lock);
        }

        if (out->worker_exit) {
            pthread_mutex_unlock(&out->lock);
            break;
        }

        if (!ext_pcm) {
            unsigned int card = PCM_CARD_DEFAULT;
            unsigned int device = PCM_DEVICE_DEFAULT;
            unsigned int flags = PCM_OUT | PCM_MONOTONIC;
            if (out->device == AUDIO_DEVICE_OUT_BLUETOOTH_SCO) {
                card = PCM_CARD_HFP;
                device = PCM_DEVICE_HFP;
                flags = PCM_OUT;
                ext_pcm = ext_pcm_open_hfp(card, device, flags, &out->pcm_config);
            } else {
                ext_pcm = ext_pcm_open_default(card, device, flags, &out->pcm_config);
            }

            if (!ext_pcm_is_ready(ext_pcm)) {
                ALOGE("pcm_open(out) failed: %s: address %s channels %d format %d rate %d period size %d",
                        ext_pcm_get_error(ext_pcm),
                        out->bus_address,
                        out->pcm_config.channels,
                        out->pcm_config.format,
                        out->pcm_config.rate,
                        out->pcm_config.period_size);
                pthread_mutex_unlock(&out->lock);
                break;
            }
            buffer_frames = out->pcm_config.period_size;
            buffer_size = ext_pcm_frames_to_bytes(ext_pcm, buffer_frames);
            buffer = malloc(buffer_size);
            if (!buffer) {
                ALOGE("could not allocate write buffer");
                pthread_mutex_unlock(&out->lock);
                break;
            }
        }

        void *output_buffer;
        int frames = audio_vbuffer_read(&out->buffer, buffer, buffer_frames);
        ALOGV("%s: read %d frames from vbuffer", __func__, frames);

        if (out->resampler) {
            size_t in_frames_count = frames;
            size_t out_frames_count = (frames * out->pcm_config.rate) / out->req_config.sample_rate;
            out->resampler->resample_from_input(out->resampler, (int16_t*)buffer, &in_frames_count,
                                               (int16_t*)out->resampler_buffer, &out_frames_count);
            output_buffer = out->resampler_buffer;
            frames = out_frames_count;
        } else {
            output_buffer = buffer;
        }

        pthread_mutex_unlock(&out->lock);
        int write_error = ext_pcm_write(ext_pcm, out->bus_address,
                                        output_buffer, ext_pcm_frames_to_bytes(ext_pcm, frames));
        if (write_error) {
            ALOGE("pcm_write failed %s address %s", ext_pcm_get_error(ext_pcm), out->bus_address);
            close_pcm = true;
        } else {
            ALOGV("pcm_write succeed address %s", out->bus_address);
        }
    }

    return NULL;
}

// Call with in->lock held
static void get_current_output_position(struct generic_stream_out *out,
        uint64_t *position, struct timespec * timestamp) {
    struct timespec curtime = { .tv_sec = 0, .tv_nsec = 0 };
    clock_gettime(CLOCK_MONOTONIC, &curtime);
    const int64_t now_us = (curtime.tv_sec * 1000000000LL + curtime.tv_nsec) / 1000;
    if (timestamp) {
        *timestamp = curtime;
    }
    int64_t position_since_underrun;
    if (out->standby) {
        position_since_underrun = 0;
    } else {
        const int64_t first_us = (out->underrun_time.tv_sec * 1000000000LL +
                                  out->underrun_time.tv_nsec) / 1000;
        position_since_underrun = (now_us - first_us) *
                out_get_sample_rate(&out->stream.common) /
                1000000;
        if (position_since_underrun < 0) {
            position_since_underrun = 0;
        }
    }
    *position = out->underrun_position + position_since_underrun;

    // The device will reuse the same output stream leading to periods of
    // underrun.
    if (*position > out->frames_written) {
        ALOGW("Not supplying enough data to HAL, expected position %" PRIu64 " , only wrote "
              "%" PRIu64,
              *position, out->frames_written);

        *position = out->frames_written;
        out->underrun_position = *position;
        out->underrun_time = curtime;
        out->frames_total_buffered = 0;
    }
}

// Applies gain naively, assume AUDIO_FORMAT_PCM_16_BIT
static void out_apply_gain(struct generic_stream_out *out, const void *buffer, size_t bytes) {
    int16_t *int16_buffer = (int16_t *)buffer;
    size_t int16_size = bytes / sizeof(int16_t);
    for (int i = 0; i < int16_size; i++) {
         float multiplied = int16_buffer[i] * out->amplitude_ratio;
         if (multiplied > INT16_MAX) int16_buffer[i] = INT16_MAX;
         else if (multiplied < INT16_MIN) int16_buffer[i] = INT16_MIN;
         else int16_buffer[i] = (int16_t)multiplied;
    }
}

static ssize_t out_write(struct audio_stream_out *stream, const void *buffer, size_t bytes) {
    struct generic_stream_out *out = (struct generic_stream_out *)stream;
    const size_t frames =  bytes / audio_stream_out_frame_size(stream);
    const size_t format_bytes = pcm_format_to_bits(out->pcm_config.format) >> 3;

    ALOGV("%s: bytes %d, frames %d", __func__, bytes, frames);
    pthread_mutex_lock(&out->lock);

    if (out->worker_standby) {
        out->worker_standby = false;
    }
    pthread_cond_signal(&out->worker_wake);

    uint64_t current_position;
    struct timespec current_time;

    get_current_output_position(out, &current_position, &current_time);
    const uint64_t now_us = (current_time.tv_sec * 1000000000LL +
                             current_time.tv_nsec) / 1000;
    if (out->standby) {
        out->standby = false;
        out->underrun_time = current_time;
        out->frames_rendered = 0;
        out->frames_total_buffered = 0;
    }

    //apply gain
    out_apply_gain(out, buffer, bytes);

    // write to vbuffer
    size_t frames_written = frames;
    if (out->dev->master_mute) {
        ALOGV("%s: ignored due to master mute", __func__);
    } else {
        const int requested_channels = popcount(out->req_config.channel_mask);

        if (out->pcm_config.channels == requested_channels) {
            frames_written = audio_vbuffer_write(&out->buffer, buffer, frames);
        } else {
            frames_written = audio_vbuffer_write_adjust(&out->buffer, buffer, frames, requested_channels);
        }

        pthread_cond_signal(&out->worker_wake);
    }

    /* Implementation just consumes bytes if we start getting backed up */
    out->frames_written += frames;
    out->frames_rendered += frames;
    out->frames_total_buffered += frames;

    // We simulate the audio device blocking when it's write buffers become
    // full.

    // At the beginning or after an underrun, try to fill up the vbuffer.
    // This will be throttled by the PlaybackThread
    int frames_sleep = out->frames_total_buffered < out->buffer.frame_count ? 0 : frames;
    uint64_t sleep_time_us = frames_sleep * 1000000LL /
                            out_get_sample_rate(&stream->common);

    // If the write calls are delayed, subtract time off of the sleep to
    // compensate
    uint64_t time_since_last_write_us = now_us - out->last_write_time_us;
    if (time_since_last_write_us < sleep_time_us) {
        sleep_time_us -= time_since_last_write_us;
    } else {
        sleep_time_us = 0;
    }
    out->last_write_time_us = now_us + sleep_time_us;

    pthread_mutex_unlock(&out->lock);

    if (sleep_time_us > 0) {
        usleep(sleep_time_us);
    }

    if (frames_written < frames) {
        ALOGW("Hardware backing HAL too slow, could only write %zu of %zu frames",
                frames_written, frames);
    }

    /* Always consume all bytes */
    return bytes;
}

static int out_get_presentation_position(const struct audio_stream_out *stream,
        uint64_t *frames, struct timespec *timestamp) {
    if (stream == NULL || frames == NULL || timestamp == NULL) {
        return -EINVAL;
    }
    struct generic_stream_out *out = (struct generic_stream_out *)stream;

    pthread_mutex_lock(&out->lock);
    get_current_output_position(out, frames, timestamp);
    pthread_mutex_unlock(&out->lock);

    return 0;
}

static int out_get_render_position(const struct audio_stream_out *stream, uint32_t *dsp_frames) {
    if (stream == NULL || dsp_frames == NULL) {
        return -EINVAL;
    }
    struct generic_stream_out *out = (struct generic_stream_out *)stream;
    pthread_mutex_lock(&out->lock);
    *dsp_frames = out->frames_rendered;
    pthread_mutex_unlock(&out->lock);
    return 0;
}

// Must be called with out->lock held
static void do_out_standby(struct generic_stream_out *out) {
    int frames_sleep = 0;
    uint64_t sleep_time_us = 0;
    if (out->standby) {
        return;
    }
    out->worker_standby = true;
    out->standby = true;
    pthread_cond_signal(&out->worker_wake);
}

static int out_standby(struct audio_stream *stream) {
    struct generic_stream_out *out = (struct generic_stream_out *)stream;
    pthread_mutex_lock(&out->lock);
    do_out_standby(out);
    pthread_mutex_unlock(&out->lock);
    return 0;
}

static int out_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect) {
    // out_add_audio_effect is a no op
    return 0;
}

static int out_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect) {
    // out_remove_audio_effect is a no op
    return 0;
}

static int out_get_next_write_timestamp(const struct audio_stream_out *stream,
        int64_t *timestamp) {
    return -ENOSYS;
}

static uint32_t in_get_sample_rate(const struct audio_stream *stream) {
    struct generic_stream_in *in = (struct generic_stream_in *)stream;
    return in->req_config.sample_rate;
}

static int in_set_sample_rate(struct audio_stream *stream, uint32_t rate) {
    return -ENOSYS;
}

static int refine_output_parameters(uint32_t *sample_rate, audio_format_t *format,
        audio_channel_mask_t *channel_mask) {
    static const uint32_t sample_rates [] = {
        8000, 11025, 16000, 22050, 24000, 32000, 44100, 48000
    };
    static const int sample_rates_count = sizeof(sample_rates)/sizeof(uint32_t);
    bool inval = false;
    if (*format != AUDIO_FORMAT_PCM_16_BIT) {
        *format = AUDIO_FORMAT_PCM_16_BIT;
        inval = true;
    }

    int channel_count = popcount(*channel_mask);
    if (channel_count != 1 && channel_count != 2) {
        *channel_mask = AUDIO_CHANNEL_IN_STEREO;
        inval = true;
    }

    int i;
    for (i = 0; i < sample_rates_count; i++) {
        if (*sample_rate < sample_rates[i]) {
            *sample_rate = sample_rates[i];
            inval=true;
            break;
        }
        else if (*sample_rate == sample_rates[i]) {
            break;
        }
        else if (i == sample_rates_count-1) {
            // Cap it to the highest rate we support
            *sample_rate = sample_rates[i];
            inval=true;
        }
    }

    if (inval) {
        return -EINVAL;
    }
    return 0;
}

static int refine_input_parameters(uint32_t *sample_rate, audio_format_t *format,
        audio_channel_mask_t *channel_mask) {
    static const uint32_t sample_rates [] = {8000, 11025, 16000, 22050, 44100, 48000};
    static const int sample_rates_count = sizeof(sample_rates)/sizeof(uint32_t);
    bool inval = false;
    // Only PCM_16_bit is supported. If this is changed, stereo to mono drop
    // must be fixed in in_read
    if (*format != AUDIO_FORMAT_PCM_16_BIT) {
        *format = AUDIO_FORMAT_PCM_16_BIT;
        inval = true;
    }

    int channel_count = popcount(*channel_mask);
    if (channel_count != 1 && channel_count != 2) {
        *channel_mask = AUDIO_CHANNEL_IN_STEREO;
        inval = true;
    }

    int i;
    for (i = 0; i < sample_rates_count; i++) {
        if (*sample_rate < sample_rates[i]) {
            *sample_rate = sample_rates[i];
            inval=true;
            break;
        }
        else if (*sample_rate == sample_rates[i]) {
            break;
        }
        else if (i == sample_rates_count-1) {
            // Cap it to the highest rate we support
            *sample_rate = sample_rates[i];
            inval=true;
        }
    }

    if (inval) {
        return -EINVAL;
    }
    return 0;
}

static size_t get_input_buffer_size(uint32_t sample_rate, audio_format_t format,
        audio_channel_mask_t channel_mask) {
    size_t size;
    size_t device_rate;
    int channel_count = popcount(channel_mask);
    if (refine_input_parameters(&sample_rate, &format, &channel_mask) != 0)
        return 0;

    size = sample_rate*IN_PERIOD_MS/1000;
    // Audioflinger expects audio buffers to be multiple of 16 frames
    size = ((size + 15) / 16) * 16;
    size *= sizeof(short) * channel_count;

    return size;
}

static size_t in_get_buffer_size(const struct audio_stream *stream) {
    struct generic_stream_in *in = (struct generic_stream_in *)stream;
    int size = get_input_buffer_size(in->req_config.sample_rate,
                                 in->req_config.format,
                                 in->req_config.channel_mask);

    return size;
}

static audio_channel_mask_t in_get_channels(const struct audio_stream *stream) {
    struct generic_stream_in *in = (struct generic_stream_in *)stream;
    return in->req_config.channel_mask;
}

static audio_format_t in_get_format(const struct audio_stream *stream) {
    struct generic_stream_in *in = (struct generic_stream_in *)stream;
    return in->req_config.format;
}

static int in_set_format(struct audio_stream *stream, audio_format_t format) {
    return -ENOSYS;
}

static int in_dump(const struct audio_stream *stream, int fd) {
    struct generic_stream_in *in = (struct generic_stream_in *)stream;

    pthread_mutex_lock(&in->lock);
    dprintf(fd, "\tin_dump:\n"
                "\t\tsample rate: %u\n"
                "\t\tbuffer size: %zu\n"
                "\t\tchannel mask: %08x\n"
                "\t\tformat: %d\n"
                "\t\tdevice: %08x\n"
                "\t\taudio dev: %p\n\n",
                in_get_sample_rate(stream),
                in_get_buffer_size(stream),
                in_get_channels(stream),
                in_get_format(stream),
                in->device,
                in->dev);
    pthread_mutex_unlock(&in->lock);
    return 0;
}

static int in_set_parameters(struct audio_stream *stream, const char *kvpairs) {
    struct generic_stream_in *in = (struct generic_stream_in *)stream;
    struct str_parms *parms;
    char value[32];
    int ret;
    long val;
    char *end;

    pthread_mutex_lock(&in->lock);
    if (!in->standby) {
        ret = -ENOSYS;
    } else {
        parms = str_parms_create_str(kvpairs);

        ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING,
                                value, sizeof(value));
        if (ret >= 0) {
            errno = 0;
            val = strtol(value, &end, 10);
            if ((errno == 0) && (end != NULL) && (*end == '\0') && ((int)val == val)) {
                in->device = (int)val;
                ret = 0;
            } else {
                ret = -EINVAL;
            }
        }

        str_parms_destroy(parms);
    }
    pthread_mutex_unlock(&in->lock);
    return ret;
}

static char *in_get_parameters(const struct audio_stream *stream, const char *keys) {
    struct generic_stream_in *in = (struct generic_stream_in *)stream;
    struct str_parms *query = str_parms_create_str(keys);
    char *str;
    char value[256];
    struct str_parms *reply = str_parms_create();
    int ret;

    ret = str_parms_get_str(query, AUDIO_PARAMETER_STREAM_ROUTING, value, sizeof(value));
    if (ret >= 0) {
        str_parms_add_int(reply, AUDIO_PARAMETER_STREAM_ROUTING, in->device);
        str = strdup(str_parms_to_str(reply));
    } else {
        str = strdup(keys);
    }

    str_parms_destroy(query);
    str_parms_destroy(reply);
    return str;
}

static int in_set_gain(struct audio_stream_in *stream, float gain) {
    // TODO(hwwang): support adjusting input gain
    return 0;
}

// Call with in->lock held
static void get_current_input_position(struct generic_stream_in *in,
        int64_t * position, struct timespec * timestamp) {
    struct timespec t = { .tv_sec = 0, .tv_nsec = 0 };
    clock_gettime(CLOCK_MONOTONIC, &t);
    const int64_t now_us = (t.tv_sec * 1000000000LL + t.tv_nsec) / 1000;
    if (timestamp) {
        *timestamp = t;
    }
    int64_t position_since_standby;
    if (in->standby) {
        position_since_standby = 0;
    } else {
        const int64_t first_us = (in->standby_exit_time.tv_sec * 1000000000LL +
                                  in->standby_exit_time.tv_nsec) / 1000;
        position_since_standby = (now_us - first_us) *
                in_get_sample_rate(&in->stream.common) /
                1000000;
        if (position_since_standby < 0) {
            position_since_standby = 0;
        }
    }
    *position = in->standby_position + position_since_standby;
}

// Must be called with in->lock held
static void do_in_standby(struct generic_stream_in *in) {
    if (in->standby) {
        return;
    }
    in->worker_standby = true;
    get_current_input_position(in, &in->standby_position, NULL);
    in->standby = true;
    pthread_cond_signal(&in->worker_wake);
}

static int in_standby(struct audio_stream *stream) {
    struct generic_stream_in *in = (struct generic_stream_in *)stream;
    pthread_mutex_lock(&in->lock);
    do_in_standby(in);
    pthread_mutex_unlock(&in->lock);
    return 0;
}

static void *in_read_worker(void *args) {
    struct generic_stream_in *in = (struct generic_stream_in *)args;
    struct pcm *pcm = NULL;
    uint8_t *buffer = NULL;
    int buffer_frames;
    int buffer_size;
    bool close_pcm = false;

    while (true) {
        pthread_mutex_lock(&in->lock);

        if (in->worker_standby || in->worker_exit) {
            close_pcm = true;
        }

        while (!close_pcm && audio_vbuffer_live(&in->buffer) == 0) {
            pthread_cond_wait(&in->worker_wake, &in->lock);
            if (in->worker_standby || in->worker_exit) {
                close_pcm = true;
            }
        }

        if (close_pcm) {
            if (pcm) {
                ALOGD("%s: closing input pcm", __func__);
                pcm_close(pcm); // Frees pcm
                pcm = NULL;
                free(buffer);
                buffer=NULL;
            }

            if (in->worker_exit) {
                pthread_mutex_unlock(&in->lock);
                break;
            }

            close_pcm = false;
            pthread_cond_wait(&in->worker_wake, &in->lock);
        }

        if (in->worker_exit) {
            pthread_mutex_unlock(&in->lock);
            break;
        }

        if (!pcm) {
            ALOGD("%s: opening input pcm", __func__);
            unsigned int card, device;
            if (in->device == AUDIO_DEVICE_IN_FM_TUNER) {
                card = PCM_CARD_FM;
                device = PCM_DEVICE_FM;
            } else if (in->device == AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET) {
                card = PCM_CARD_HFP;
                device = PCM_DEVICE_HFP;
            } else {
                card = PCM_CARD_DEFAULT;
                device = PCM_DEVICE_DEFAULT;
            }
            pcm = pcm_open(card, device, PCM_IN, &in->pcm_config);
            if (!pcm_is_ready(pcm)) {
                ALOGE("pcm_open(in) failed: %s: channels %d format %d rate %d period size %d",
                        pcm_get_error(pcm),
                        in->pcm_config.channels,
                        in->pcm_config.format,
                        in->pcm_config.rate,
                        in->pcm_config.period_size);
                pthread_mutex_unlock(&in->lock);
                break;
            }
            buffer_frames = in->pcm_config.period_size;
            buffer_size = pcm_frames_to_bytes(pcm, buffer_frames);
            buffer = malloc(buffer_size);
            if (!buffer) {
                ALOGE("could not allocate worker read buffer");
                pthread_mutex_unlock(&in->lock);
                break;
            }
        }
        pthread_mutex_unlock(&in->lock);

        int ret = pcm_read(pcm, buffer, pcm_frames_to_bytes(pcm, buffer_frames));

        if (ret != 0) {
            ALOGW("pcm_read failed %s", pcm_get_error(pcm));
            close_pcm = true;
        }

        ALOGV("%s: read %d frames from input pcm", __func__, buffer_frames);

        size_t frames_written = 0;
        pthread_mutex_lock(&in->lock);
        if (in->resampler) {
            size_t in_frames_count = buffer_frames;
            size_t out_frames_count = (buffer_frames * in->req_config.sample_rate) / in->pcm_config.rate;
            in->resampler->resample_from_input(in->resampler, (int16_t*)buffer, &in_frames_count,
                                            (int16_t*)in->resampler_buffer, &out_frames_count);
            frames_written = audio_vbuffer_write(&in->buffer, in->resampler_buffer, out_frames_count);
        } else {
            frames_written = audio_vbuffer_write(&in->buffer, buffer, buffer_frames);
        }
        pthread_mutex_unlock(&in->lock);

        ALOGV("%s: Wrote %d frames to vbuffer", __func__, frames_written);

        if (frames_written != buffer_frames) {
            ALOGW("in_read_worker only could write %zu / %zu frames",
                    frames_written, buffer_frames);
        }
    }
    return NULL;
}

static void *in_read_worker_bt_call(void *args) {
    struct generic_stream_in *in = (struct generic_stream_in *)args;
    struct pcm *pcm = NULL;
    bool close_pcm = false;

    uint8_t *buffer = NULL;
    uint8_t *out_buffer = NULL;
    int buffer_frames;
    int buffer_size;
    int out_buffer_size;

    struct generic_stream_out *out_stream;

    while (true) {
        int64_t ns = 0;
        pthread_mutex_lock(&in->dev->lock);
        ns = in->dev->sleep_ms;
        pthread_mutex_unlock(&in->dev->lock);

        pthread_mutex_lock(&in->lock);

        if (in->worker_standby || in->worker_exit) {
            close_pcm = true;
        }

        if (!close_pcm) {
            if (ns == 0) {
                pthread_cond_wait(&in->worker_wake, &in->lock);
            }

            if (in->worker_standby || in->worker_exit) {
                close_pcm = true;
            }
        }

        if (close_pcm) {
            if (pcm) {
                ALOGD("%s: closing input pcm", __func__);
                pcm_close(pcm); // Frees pcm
                pcm = NULL;
                free(buffer);
                buffer = NULL;
                if (out_buffer) {
                    free(out_buffer);
                    out_buffer = NULL;
                }
            }

            if (in->worker_exit) {
                pthread_mutex_unlock(&in->lock);
                break;
            }

            close_pcm = false;

            pthread_mutex_lock(&in->dev->lock);
            if (in->device == AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET) {
                in->dev->hfp_call.stream_flag |= HFP_IN_ACTIVE_FLAG;
            } else {
                in->dev->hfp_call.stream_flag |= MIC_IN_ACTIVE_FLAG;
            }
            pthread_mutex_unlock(&in->dev->lock);

            pthread_cond_wait(&in->worker_wake, &in->lock);
        }

        if (in->worker_exit) {
            pthread_mutex_unlock(&in->lock);
            break;
        }

        if (!pcm) {
            ALOGD("%s: opening input pcm", __func__);

            unsigned int card = PCM_CARD_DEFAULT;
            unsigned int device = PCM_DEVICE_DEFAULT;

            if (in->device == AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET) {
                card = PCM_CARD_HFP;
                device = PCM_DEVICE_HFP;
            }

            pcm = pcm_open(card, device, PCM_IN, &in->pcm_config);
            if (!pcm_is_ready(pcm)) {
                ALOGE("pcm_open(in) failed: %s: channels %d format %d rate %d period size %d",
                        pcm_get_error(pcm),
                        in->pcm_config.channels,
                        in->pcm_config.format,
                        in->pcm_config.rate,
                        in->pcm_config.period_size);
                pthread_mutex_unlock(&in->lock);
                break;
            }
            buffer_frames = in->pcm_config.period_size;
            buffer_size = pcm_frames_to_bytes(pcm, buffer_frames);
            buffer = malloc(buffer_size);
            if (!buffer) {
                ALOGE("could not allocate worker read buffer");
                pthread_mutex_unlock(&in->lock);
                break;
            }
        }

        int ret = pcm_read(pcm, buffer, pcm_frames_to_bytes(pcm, buffer_frames));
        if (ret != 0) {
            ALOGW("pcm_read failed %s", pcm_get_error(pcm));
            close_pcm = true;
            pthread_mutex_unlock(&in->lock);
            continue;
        }
        if (in->device == AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET) {
            size_t frame_size = buffer_size / buffer_frames;
            for (size_t counter = 0; counter < buffer_frames; counter++) {
                size_t offset = frame_size * counter;
                memcpy((char*)buffer + offset, (char*)buffer + offset + frame_size / 2, frame_size / 2);
            }
        }

        if (in->device == AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET) {
            out_stream = &in->dev->bus_stream_out->stream;
        } else {
            out_stream = &in->dev->hfp_call.hfp_output->stream;
        }
        if (in->pcm_config.channels > out_stream->pcm_config.channels) {
            out_buffer_size = buffer_frames * out_stream->pcm_config.channels *
                    pcm_format_to_bits(out_stream->pcm_config.format) >> 3;
            if (!out_buffer) {
                out_buffer = malloc(out_buffer_size);
            }
            if (!out_buffer) {
                ALOGE("could not allocate worker read buffer");
                close_pcm = true;
                pthread_mutex_unlock(&in->lock);
                continue;
            }
        }

        //changing number of channels in buffer
        void* adjust_buffer;
        int adjust_buffer_size;
        if (in->pcm_config.channels > out_stream->pcm_config.channels) {
            audio_buffer_adjust(out_buffer, out_stream->pcm_config.channels,
                                buffer, in->pcm_config.channels,
                                buffer_frames, pcm_format_to_bits(out_stream->pcm_config.format) >> 3);
            adjust_buffer = out_buffer;
            adjust_buffer_size = out_buffer_size;
        } else {
            adjust_buffer = buffer;
            adjust_buffer_size = buffer_size;
        }

        //resampling
        void *output_buffer;
        int output_buffer_size;
        if (in->resampler) {
            size_t in_frames_count = buffer_frames;
            size_t out_frames_count = (buffer_frames * in->req_config.sample_rate) / in->pcm_config.rate;
            in->resampler->resample_from_input(in->resampler, (int16_t*)adjust_buffer, &in_frames_count,
                                               (int16_t*)in->resampler_buffer, &out_frames_count);
            output_buffer = in->resampler_buffer;
            output_buffer_size = (adjust_buffer_size * out_frames_count) / in_frames_count;
        } else {
            output_buffer = adjust_buffer;
            output_buffer_size = adjust_buffer_size;
        }

        if (in->resampler) {
            int size_to_write_total = output_buffer_size;
            int written = 0;
            while (size_to_write_total > 0) {
                int size_to_write_current =
                        size_to_write_total < pcm_frames_to_bytes(pcm, in->pcm_config.period_size) ?
                        size_to_write_total : pcm_frames_to_bytes(pcm, in->pcm_config.period_size);
                out_write(out_stream, output_buffer + written, size_to_write_current);
                written += size_to_write_current;
                size_to_write_total -= size_to_write_current;
            }
        } else {
            out_write(out_stream, output_buffer, output_buffer_size);
        }

        pthread_mutex_unlock(&in->lock);
        if (ns > 0) {
            usleep(ns);
        }
    }
    return NULL;
}

static ssize_t in_read(struct audio_stream_in *stream, void *buffer, size_t bytes) {
    struct generic_stream_in *in = (struct generic_stream_in *)stream;
    struct generic_audio_device *adev = in->dev;
    const size_t frames = bytes / audio_stream_in_frame_size(stream);
    int ret = 0;
    bool mic_mute = false;
    size_t read_bytes = 0;

    adev_get_mic_mute(&adev->device, &mic_mute);
    pthread_mutex_lock(&in->lock);

    if (in->worker_standby) {
        in->worker_standby = false;
    }
    pthread_cond_signal(&in->worker_wake);

    int64_t current_position;
    struct timespec current_time;

    get_current_input_position(in, &current_position, &current_time);
    if (in->standby) {
        in->standby = false;
        in->standby_exit_time = current_time;
        in->standby_frames_read = 0;
    }

    const int64_t frames_available =
        current_position - in->standby_position - in->standby_frames_read;
    assert(frames_available >= 0);

    const size_t frames_wait =
        ((uint64_t)frames_available > frames) ? 0 : frames - frames_available;

    int64_t sleep_time_us  = frames_wait * 1000000LL / in_get_sample_rate(&stream->common);

    pthread_mutex_unlock(&in->lock);

    if (sleep_time_us > 0) {
        usleep(sleep_time_us);
    }

    pthread_mutex_lock(&in->lock);

    int read_frames = 0;
    if (in->standby) {
        ALOGW("Input put to sleep while read in progress");
        goto exit;
    }
    in->standby_frames_read += frames;

    memset ((uint8_t *)buffer, 0, bytes);

    const int requested_channels = popcount(in->req_config.channel_mask);

    if (in->pcm_config.channels == requested_channels) {
        read_frames = audio_vbuffer_read(&in->buffer, buffer, frames);
    } else {
        read_frames = audio_vbuffer_read_adjust(&in->buffer, buffer,
                                            frames, popcount(in->req_config.channel_mask));
    }

exit:
    read_bytes = read_frames * audio_stream_in_frame_size(stream);

    ALOGV("%s: Read %d frames out of %d (%d bytes, %d channels) from vbuffer",
         __func__,
         read_frames,
         frames,
         read_bytes,
         popcount(in->req_config.channel_mask));

    if (mic_mute) {
        read_bytes = 0;
    }

    pthread_mutex_unlock(&in->lock);

    return bytes;
}

static uint32_t in_get_input_frames_lost(struct audio_stream_in *stream) {
    return 0;
}

static int in_get_capture_position(const struct audio_stream_in *stream,
        int64_t *frames, int64_t *time) {
    struct generic_stream_in *in = (struct generic_stream_in *)stream;
    pthread_mutex_lock(&in->lock);
    struct timespec current_time;
    get_current_input_position(in, frames, &current_time);
    *time = (current_time.tv_sec * 1000000000LL + current_time.tv_nsec);
    pthread_mutex_unlock(&in->lock);
    return 0;
}

static int in_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect) {
    // in_add_audio_effect is a no op
    return 0;
}

static int in_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect) {
    // in_add_audio_effect is a no op
    return 0;
}

static int adev_open_output_stream_bus(struct audio_hw_device *dev,
        audio_io_handle_t handle, audio_devices_t devices, audio_output_flags_t flags,
        struct audio_config *config, struct audio_stream_out **stream_out, const char *address) {
    struct generic_audio_device *adev = (struct generic_audio_device *)dev;
    struct generic_stream_out *out;
    int ret = 0;
    if (refine_output_parameters(&config->sample_rate, &config->format, &config->channel_mask)) {
        ALOGE("Error opening output stream format %d, channel_mask %04x, sample_rate %u",
              config->format, config->channel_mask, config->sample_rate);
        return -EINVAL;
    }

    out = (struct generic_stream_out *)calloc(1, sizeof(struct generic_stream_out));

    if (!out) {
        ALOGE("generic_stream_out creation failed");
        return -ENOMEM;
    }

    out->stream.common.get_sample_rate = out_get_sample_rate;
    out->stream.common.set_sample_rate = out_set_sample_rate;
    out->stream.common.get_buffer_size = out_get_buffer_size;
    out->stream.common.get_channels = out_get_channels;
    out->stream.common.get_format = out_get_format;
    out->stream.common.set_format = out_set_format;
    out->stream.common.standby = out_standby;
    out->stream.common.dump = out_dump;
    out->stream.common.set_parameters = out_set_parameters;
    out->stream.common.get_parameters = out_get_parameters;
    out->stream.common.add_audio_effect = out_add_audio_effect;
    out->stream.common.remove_audio_effect = out_remove_audio_effect;
    out->stream.get_latency = out_get_latency;
    out->stream.set_volume = out_set_volume;
    out->stream.write = out_write;
    out->stream.get_render_position = out_get_render_position;
    out->stream.get_presentation_position = out_get_presentation_position;
    out->stream.get_next_write_timestamp = out_get_next_write_timestamp;

    pthread_mutex_init(&out->lock, (const pthread_mutexattr_t *) NULL);
    out->dev = adev;
    out->device = devices;
    memcpy(&out->req_config, config, sizeof(struct audio_config));
    memcpy(&out->pcm_config, &pcm_config_out_default, sizeof(struct pcm_config));
    //out->pcm_config.rate = config->sample_rate;

    out->standby = true;
    out->underrun_position = 0;
    out->underrun_time.tv_sec = 0;
    out->underrun_time.tv_nsec = 0;
    out->last_write_time_us = 0;
    out->frames_total_buffered = 0;
    out->frames_written = 0;
    out->frames_rendered = 0;

    const size_t format_bytes = pcm_format_to_bits(out->pcm_config.format) >> 3;
    const size_t pcm_frame_size = out->pcm_config.channels * format_bytes;

    ret = audio_vbuffer_init(&out->buffer,
            out->pcm_config.period_size * out->pcm_config.period_count,
            format_bytes, out->pcm_config.channels);
    if (ret != 0) {
        ALOGE("%s: audio vbuffer creation failed: %s", __func__, strerror(ret));
        return ret;
    }
    // init resampler if necessary
    if (out->pcm_config.rate != out->req_config.sample_rate) {
        const size_t resampler_buffer_frame_count =
            (out->req_config.sample_rate * out->pcm_config.period_size) / out->pcm_config.rate;
        const size_t resampler_buffer_bytes = resampler_buffer_frame_count * pcm_frame_size;

        out->resampler_buffer = malloc(resampler_buffer_bytes);
        if (!out->resampler_buffer) {
            ALOGE("%s: resampler_buffer creation failed", __func__);
            return -ENOMEM;
        }

        ret = create_resampler(out->req_config.sample_rate,
                               out->pcm_config.rate,
                               out->pcm_config.channels,
                               RESAMPLER_QUALITY_DEFAULT,
                               NULL,
                               &out->resampler);
        if (ret != 0) {
            ALOGE("%s: Resampler creation failed: %s", __func__, strerror(ret));
            return ret;
        }
    } else {
        out->resampler = NULL;
        out->resampler_buffer = NULL;
    }

    // init thread
    pthread_cond_init(&out->worker_wake, NULL);
    out->worker_standby = true;
    out->worker_exit = false;
    pthread_create(&out->worker_thread, NULL, out_write_worker, out);

    // set bus parameters if it is such
    if (address) {
        out->bus_address = calloc(strlen(address) + 1, sizeof(char));
        strncpy(out->bus_address, address, strlen(address));
        hashmapPut(adev->out_bus_stream_map, out->bus_address, out);
        /* TODO: read struct audio_gain from audio_policy_configuration */
        out->gain_stage = (struct audio_gain) {
            .min_value = -3200,
            .max_value = 600,
            .step_value = 100,
        };
        out->amplitude_ratio = 1.0;
    }

    *stream_out = &out->stream;
    adev->bus_stream_out = out;
    ALOGD("%s bus:%s", __func__, out->bus_address);
    return ret;
}

static int adev_open_output_stream_hfp(struct audio_hw_device *dev,
        audio_io_handle_t handle, audio_devices_t devices, audio_output_flags_t flags,
        struct audio_config *config, struct audio_stream_out **stream_out, const char *address) {
    struct generic_audio_device *adev = (struct generic_audio_device *)dev;
    struct generic_stream_out *out;
    int ret = 0;
    if (refine_output_parameters(&config->sample_rate, &config->format, &config->channel_mask)) {
        ALOGE("Error opening output stream format %d, channel_mask %04x, sample_rate %u",
              config->format, config->channel_mask, config->sample_rate);
        return -EINVAL;
    }

    out = (struct generic_stream_out *)calloc(1, sizeof(struct generic_stream_out));

    if (!out) {
        ALOGE("generic_stream_out creation failed");
        return -ENOMEM;
    }

    out->stream.common.get_sample_rate = out_get_sample_rate;
    out->stream.common.set_sample_rate = out_set_sample_rate;
    out->stream.common.get_buffer_size = out_get_buffer_size;
    out->stream.common.get_channels = out_get_channels;
    out->stream.common.get_format = out_get_format;
    out->stream.common.set_format = out_set_format;
    out->stream.common.standby = out_standby;
    out->stream.common.dump = out_dump;
    out->stream.common.set_parameters = out_set_parameters;
    out->stream.common.get_parameters = out_get_parameters;
    out->stream.common.add_audio_effect = out_add_audio_effect;
    out->stream.common.remove_audio_effect = out_remove_audio_effect;
    out->stream.get_latency = out_get_latency;
    out->stream.set_volume = out_set_volume;
    out->stream.write = out_write;
    out->stream.get_render_position = out_get_render_position;
    out->stream.get_presentation_position = out_get_presentation_position;
    out->stream.get_next_write_timestamp = out_get_next_write_timestamp;

    pthread_mutex_init(&out->lock, (const pthread_mutexattr_t *) NULL);
    out->dev = adev;
    out->device = devices;
    memcpy(&out->req_config, config, sizeof(struct audio_config));
    memcpy(&out->pcm_config, &pcm_config_out_hfp, sizeof(struct pcm_config));
    //out->pcm_config.rate = config->sample_rate;

    out->standby = true;
    out->underrun_position = 0;
    out->underrun_time.tv_sec = 0;
    out->underrun_time.tv_nsec = 0;
    out->last_write_time_us = 0;
    out->frames_total_buffered = 0;
    out->frames_written = 0;
    out->frames_rendered = 0;
    out->amplitude_ratio = 1.0;

    const size_t format_bytes = pcm_format_to_bits(out->pcm_config.format) >> 3;
    const size_t pcm_frame_size = out->pcm_config.channels * format_bytes;

    ret = audio_vbuffer_init(&out->buffer,
            out->pcm_config.period_size * out->pcm_config.period_count,
            format_bytes, out->pcm_config.channels);
    if (ret != 0) {
        ALOGE("%s: audio vbuffer creation failed: %s", __func__, strerror(ret));
        return ret;
    }
    // init resampler if necessary
    if (out->pcm_config.rate != out->req_config.sample_rate) {
        const size_t resampler_buffer_frame_count =
            (out->req_config.sample_rate * out->pcm_config.period_size) / out->pcm_config.rate;
        const size_t resampler_buffer_bytes = resampler_buffer_frame_count * pcm_frame_size;

        out->resampler_buffer = malloc(resampler_buffer_bytes);
        if (!out->resampler_buffer) {
            ALOGE("%s: resampler_buffer creation failed", __func__);
            return -ENOMEM;
        }

        ret = create_resampler(out->req_config.sample_rate,
                               out->pcm_config.rate,
                               out->pcm_config.channels,
                               RESAMPLER_QUALITY_DEFAULT,
                               NULL,
                               &out->resampler);
        if (ret != 0) {
            ALOGE("%s: Resampler creation failed: %s", __func__, strerror(ret));
            return ret;
        }
    } else {
        out->resampler = NULL;
        out->resampler_buffer = NULL;
    }

    // init thread
    pthread_cond_init(&out->worker_wake, NULL);
    out->worker_standby = true;
    out->worker_exit = false;
    pthread_create(&out->worker_thread, NULL, out_write_worker, out);

    *stream_out = &out->stream;
    ALOGD("%s bus:%s", __func__, out->bus_address);
    return ret;
}

static int adev_open_output_stream(struct audio_hw_device *dev,
        audio_io_handle_t handle, audio_devices_t devices, audio_output_flags_t flags,
        struct audio_config *config, struct audio_stream_out **stream_out, const char *address) {
    if (devices == AUDIO_DEVICE_OUT_BUS) {
        return adev_open_output_stream_bus(
                dev, handle, devices, flags, config, stream_out, address);
    } else if (devices == AUDIO_DEVICE_OUT_BLUETOOTH_SCO) {
        return adev_open_output_stream_hfp(
                dev, handle, devices, flags, config, stream_out, NULL);
    }
    return -EINVAL;
}

static void adev_close_output_stream(struct audio_hw_device *dev,
        struct audio_stream_out *stream) {
    struct generic_audio_device *adev = (struct generic_audio_device *)dev;
    struct generic_stream_out *out = (struct generic_stream_out *)stream;
    ALOGD("%s bus:%s", __func__, out->bus_address);
    pthread_mutex_lock(&out->lock);
    do_out_standby(out);
    if (out->device == AUDIO_DEVICE_OUT_BUS) {
        adev->bus_stream_out = NULL;
    }

    out->worker_exit = true;
    pthread_cond_signal(&out->worker_wake);
    pthread_mutex_unlock(&out->lock);

    pthread_join(out->worker_thread, NULL);
    pthread_mutex_destroy(&out->lock);
    audio_vbuffer_destroy(&out->buffer);

    if (out->bus_address) {
        hashmapRemove(adev->out_bus_stream_map, out->bus_address);
        free(out->bus_address);
    }

    if (out->resampler_buffer) {
        free(out->resampler_buffer);
    }

    if (out->resampler) {
        release_resampler(out->resampler);
    }

    free(stream);
}

static void open_hfp_handles(struct generic_audio_device *adev)
{
    adev->hfp_call.stream_flag = 0;

    if (adev->hfp_call.hfp_input == NULL) {
        struct audio_stream_in *stream_in;
        struct audio_config config = {
                DEFAULT_IN_SAMPLING_RATE, AUDIO_CHANNEL_IN_STEREO, AUDIO_FORMAT_PCM_16_BIT, {}, IN_PERIOD_SIZE};
        int res = adev->device.open_input_stream(&adev->device, 0,
                AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET, &config, &stream_in, AUDIO_INPUT_FLAG_NONE, "",
                AUDIO_SOURCE_VOICE_CALL);
        if (res == 0) {
            pthread_mutex_lock(&adev->lock);
            adev->hfp_call.hfp_input = (struct generic_stream_in*)stream_in;
            pthread_mutex_unlock(&adev->lock);
        } else {
            return;
        }
    }
    if (adev->hfp_call.mic_input == NULL) {
        struct audio_stream_in *stream_in;
        struct audio_config config = {
                DEFAULT_IN_SAMPLING_RATE, AUDIO_CHANNEL_IN_STEREO, AUDIO_FORMAT_PCM_16_BIT, {}, IN_PERIOD_SIZE};
        int res = adev->device.open_input_stream(&adev->device, 0,
                AUDIO_DEVICE_IN_BUILTIN_MIC, &config, &stream_in, AUDIO_INPUT_FLAG_NONE, "",
                AUDIO_SOURCE_VOICE_CALL);
        if (res == 0) {
            pthread_mutex_lock(&adev->lock);
            adev->hfp_call.mic_input = (struct generic_stream_in*)stream_in;
            pthread_mutex_unlock(&adev->lock);
        } else {
            return;
        }
    }
    if (adev->hfp_call.hfp_output == NULL) {
        struct audio_stream_out *stream_out;
        struct audio_config config = {
                DEFAULT_OUT_SAMPLING_RATE, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_16_BIT, {}, OUT_PERIOD_SIZE};
        int res = adev->device.open_output_stream(&adev->device, 0,
                AUDIO_DEVICE_OUT_BLUETOOTH_SCO, AUDIO_OUTPUT_FLAG_NONE, &config, &stream_out, "");
        if (res == 0) {
            pthread_mutex_lock(&adev->lock);
            adev->hfp_call.hfp_output = (struct generic_stream_out*)stream_out;
            pthread_mutex_unlock(&adev->lock);
        } else {
            return;
        }
    }
}

static void close_hfp_handles(struct generic_audio_device *adev)
{
    pthread_mutex_lock(&adev->lock);
    struct generic_stream_in *hfp_in = adev->hfp_call.hfp_input;
    struct generic_stream_in *mic = adev->hfp_call.mic_input;
    struct generic_stream_out *hfp_out = adev->hfp_call.hfp_output;
    struct generic_stream_out *stereo_out = adev->bus_stream_out;
    adev->hfp_call.stream_flag = 0;
    pthread_mutex_unlock(&adev->lock);

    if (hfp_in) {
        pthread_mutex_lock(&hfp_in->lock);
        hfp_in->worker_standby = true;
        pthread_mutex_unlock(&hfp_in->lock);
        adev->device.close_input_stream(&adev->device, &hfp_in->stream);
        pthread_mutex_lock(&adev->lock);
        adev->hfp_call.hfp_input = NULL;
        pthread_mutex_unlock(&adev->lock);
    }

    if (mic) {
        pthread_mutex_lock(&mic->lock);
        mic->worker_standby = true;
        pthread_mutex_unlock(&mic->lock);
        adev->device.close_input_stream(&adev->device, &mic->stream);
        pthread_mutex_lock(&adev->lock);
        adev->hfp_call.mic_input = NULL;
        pthread_mutex_unlock(&adev->lock);
    }

    if (hfp_out) {
        pthread_mutex_lock(&hfp_out->lock);
        hfp_out->worker_standby = true;
        pthread_mutex_unlock(&hfp_out->lock);
        adev->device.close_output_stream(&adev->device, &hfp_out->stream);
        pthread_mutex_lock(&adev->lock);
        adev->hfp_call.hfp_output = NULL;
        pthread_mutex_unlock(&adev->lock);
    }

    if (stereo_out) {
        stereo_out->stream.common.standby(&stereo_out->stream.common);
    }
}

static void start_hfp_call(struct generic_audio_device *adev) {

    pthread_mutex_lock(&adev->hfp_call.hfp_input->lock);
    adev->hfp_call.hfp_input->worker_standby = false;
    pthread_mutex_unlock(&adev->hfp_call.hfp_input->lock);

    pthread_mutex_lock(&adev->hfp_call.mic_input->lock);
    adev->hfp_call.mic_input->worker_standby = false;
    pthread_mutex_unlock(&adev->hfp_call.mic_input->lock);

    pthread_mutex_lock(&adev->lock);
    adev->sleep_ms = 1000;
    pthread_mutex_unlock(&adev->lock);

    pthread_cond_signal(&adev->hfp_call.hfp_input->worker_wake);
    pthread_cond_signal(&adev->hfp_call.mic_input->worker_wake);
}

static void stop_hfp_call(struct generic_audio_device *adev) {
    pthread_mutex_lock(&adev->lock);
    adev->sleep_ms = 0;
    pthread_mutex_unlock(&adev->lock);
}

static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs) {
    struct generic_audio_device *adev = (struct generic_audio_device *)dev;
    struct str_parms *parms;
    char value[32];
    unsigned int val;

    parms = str_parms_create_str(kvpairs);
    if (str_parms_get_str(parms, AUDIO_PARAMETER_KEY_HFP_ENABLE, value, sizeof(value)) >= 0) {
        if (strcmp(value, "true") == 0) {
            open_hfp_handles(adev);
            int sleep_time = 0;
            int stream_flag = 0;
            while (sleep_time < SLEEP_MAX) {
                pthread_mutex_lock(&adev->lock);
                stream_flag = adev->hfp_call.stream_flag;
                pthread_mutex_unlock(&adev->lock);
                if (stream_flag == ALL_ACTIVE_FLAG) {
                    break;
                }
                usleep(SLEEP_MS);
                sleep_time += SLEEP_MS;
            }
            if (stream_flag == ALL_ACTIVE_FLAG) {
                start_hfp_call(adev);
            } else {
                close_hfp_handles(adev);
            }
        } else if (strcmp(value, "false") == 0) {
            stop_hfp_call(adev);
            close_hfp_handles(adev);
        }
    } else if (str_parms_get_str(parms, AUDIO_PARAMETER_KEY_HFP_SET_SAMPLING_RATE,
            value, sizeof(value)) >= 0) {
        val = atoi(value);
        if (val > 0) {
            // todo: hack to enable 8kHz connect (actually fake on 16 kHz)
            //pcm_config_in_hfp.rate = val;
            //pcm_config_out_hfp.rate = val;
        }
    } else if (str_parms_get_str(parms, AUDIO_PARAMETER_KEY_HFP_VOLUME,
            value, sizeof(value)) >= 0) {
        val = atoi(value);
        if (val > 0) {
            pthread_mutex_lock(&adev->lock);
            adev->hfp_call.hfp_volume = val;
            pthread_mutex_unlock(&adev->lock);
        }
    }
    str_parms_destroy(parms);

    return 0;
}

static char *adev_get_parameters(const struct audio_hw_device *dev, const char *keys) {
    return NULL;
}

static int adev_init_check(const struct audio_hw_device *dev) {
    return 0;
}

static int adev_set_voice_volume(struct audio_hw_device *dev, float volume) {
    // adev_set_voice_volume is a no op (simulates phones)
    return 0;
}

static int adev_set_master_volume(struct audio_hw_device *dev, float volume) {
    return -ENOSYS;
}

static int adev_get_master_volume(struct audio_hw_device *dev, float *volume) {
    return -ENOSYS;
}

static int adev_set_master_mute(struct audio_hw_device *dev, bool muted) {
    ALOGD("%s: %s", __func__, _bool_str(muted));
    struct generic_audio_device *adev = (struct generic_audio_device *)dev;
    pthread_mutex_lock(&adev->lock);
    adev->master_mute = muted;
    pthread_mutex_unlock(&adev->lock);
    return 0;
}

static int adev_get_master_mute(struct audio_hw_device *dev, bool *muted) {
    struct generic_audio_device *adev = (struct generic_audio_device *)dev;
    pthread_mutex_lock(&adev->lock);
    *muted = adev->master_mute;
    pthread_mutex_unlock(&adev->lock);
    ALOGD("%s: %s", __func__, _bool_str(*muted));
    return 0;
}

static int adev_set_mode(struct audio_hw_device *dev, audio_mode_t mode) {
    // adev_set_mode is a no op (simulates phones)
    return 0;
}

static int adev_set_mic_mute(struct audio_hw_device *dev, bool state) {
    struct generic_audio_device *adev = (struct generic_audio_device *)dev;
    pthread_mutex_lock(&adev->lock);
    adev->mic_mute = state;
    pthread_mutex_unlock(&adev->lock);
    return 0;
}

static int adev_get_mic_mute(const struct audio_hw_device *dev, bool *state) {
    struct generic_audio_device *adev = (struct generic_audio_device *)dev;
    pthread_mutex_lock(&adev->lock);
    *state = adev->mic_mute;
    pthread_mutex_unlock(&adev->lock);
    return 0;
}

static size_t adev_get_input_buffer_size(const struct audio_hw_device *dev,
        const struct audio_config *config) {
    return get_input_buffer_size(config->sample_rate, config->format, config->channel_mask);
}

static void adev_close_input_stream(struct audio_hw_device *dev,
        struct audio_stream_in *stream) {
    struct generic_stream_in *in = (struct generic_stream_in *)stream;
    pthread_mutex_lock(&in->lock);
    do_in_standby(in);

    in->worker_exit = true;
    pthread_cond_signal(&in->worker_wake);
    pthread_mutex_unlock(&in->lock);
    pthread_join(in->worker_thread, NULL);

    if (in->bus_address) {
        free(in->bus_address);
    }

    pthread_mutex_destroy(&in->lock);
    audio_vbuffer_destroy(&in->buffer);

    if (in->resampler_buffer) {
        free(in->resampler_buffer);
    }

    if (in->resampler) {
        release_resampler(in->resampler);
    }

    free(stream);
}

static int adev_open_input_stream(struct audio_hw_device *dev,
        audio_io_handle_t handle, audio_devices_t devices, struct audio_config *config,
        struct audio_stream_in **stream_in, audio_input_flags_t flags, const char *address,
        audio_source_t source) {
    struct generic_audio_device *adev = (struct generic_audio_device *)dev;
    struct generic_stream_in *in;
    int ret = 0;

    if (refine_input_parameters(&config->sample_rate, &config->format, &config->channel_mask)) {
        ALOGE("Error opening input stream format %d, channel_mask %04x, sample_rate %u",
              config->format, config->channel_mask, config->sample_rate);
        return -EINVAL;
    }

    in = (struct generic_stream_in *)calloc(1, sizeof(struct generic_stream_in));
    if (!in) {
        return -ENOMEM;
    }

    in->stream.common.get_sample_rate = in_get_sample_rate;
    in->stream.common.set_sample_rate = in_set_sample_rate;         // no op
    in->stream.common.get_buffer_size = in_get_buffer_size;
    in->stream.common.get_channels = in_get_channels;
    in->stream.common.get_format = in_get_format;
    in->stream.common.set_format = in_set_format;                   // no op
    in->stream.common.standby = in_standby;
    in->stream.common.dump = in_dump;
    in->stream.common.set_parameters = in_set_parameters;
    in->stream.common.get_parameters = in_get_parameters;
    in->stream.common.add_audio_effect = in_add_audio_effect;       // no op
    in->stream.common.remove_audio_effect = in_remove_audio_effect; // no op
    in->stream.set_gain = in_set_gain;                              // no op
    in->stream.read = in_read;
    in->stream.get_input_frames_lost = in_get_input_frames_lost;    // no op
    in->stream.get_capture_position = in_get_capture_position;

    pthread_mutex_init(&in->lock, (const pthread_mutexattr_t *) NULL);
    in->dev = adev;
    in->device = devices;
    memcpy(&in->req_config, config, sizeof(struct audio_config));
    if (in->device == AUDIO_DEVICE_IN_FM_TUNER) {
        memcpy(&in->pcm_config, &pcm_config_in_fm, sizeof(struct pcm_config));
    } else if (in->device == AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET) {
        memcpy(&in->pcm_config, &pcm_config_in_hfp, sizeof(struct pcm_config));
    } else {
        memcpy(&in->pcm_config, &pcm_config_in_default, sizeof(struct pcm_config));
    }
    //in->pcm_config.rate = config->sample_rate;
    //in->pcm_config.period_size = in->pcm_config.rate*IN_PERIOD_MS/1000;

    in->standby = true;
    in->standby_position = 0;
    in->standby_exit_time.tv_sec = 0;
    in->standby_exit_time.tv_nsec = 0;
    in->standby_frames_read = 0;

    size_t format_bytes = pcm_format_to_bits(in->pcm_config.format) >> 3;
    const size_t pcm_frame_size = in->pcm_config.channels * format_bytes;
    size_t buffer_frame_count = 0;

    // init resampler
    if (in->pcm_config.rate != in->req_config.sample_rate) {
        buffer_frame_count = (in->pcm_config.period_size * in->req_config.sample_rate)
                                    / in->pcm_config.rate;

        size_t buffer_bytes = buffer_frame_count * pcm_frame_size;

        in->resampler_buffer = malloc(buffer_bytes);
        if (!in->resampler_buffer) {
            ALOGE("%s: resampler_buffer creation failed", __func__);
            return -ENOMEM;
        }

        ret = create_resampler(in->pcm_config.rate,
                               in->req_config.sample_rate,
                               in->pcm_config.channels,
                               RESAMPLER_QUALITY_DEFAULT,
                               NULL,
                               &in->resampler);
        if (ret != 0) {
            ALOGE("%s: Resampler creation failed", __func__);
            return ret;
        }
    } else {
        in->resampler = NULL;
        in->resampler_buffer = NULL;
        buffer_frame_count = in->pcm_config.period_size;
    }

    ret = audio_vbuffer_init(&in->buffer,
            buffer_frame_count * in->pcm_config.period_count,
            format_bytes, in->pcm_config.channels);
    if (ret != 0) {
        ALOGE("%s: audio_vbuffer creation failed: %s", __func__, strerror(ret));
        return ret;
    }

    pthread_cond_init(&in->worker_wake, NULL);
    in->worker_standby = true;
    in->worker_exit = false;
    if (source == AUDIO_SOURCE_VOICE_CALL) {
        pthread_create(&in->worker_thread, NULL, in_read_worker_bt_call, in);
    } else {
        pthread_create(&in->worker_thread, NULL, in_read_worker, in);
    }

    if (address) {
        in->bus_address = calloc(strlen(address) + 1, sizeof(char));
        strncpy(in->bus_address, address, strlen(address));
    }

    *stream_in = &in->stream;

    return ret;
}

static int adev_dump(const audio_hw_device_t *dev, int fd) {
    return 0;
}

static int adev_set_audio_port_config(struct audio_hw_device *dev,
        const struct audio_port_config *config) {
    int ret = 0;
    struct generic_audio_device *adev = (struct generic_audio_device *)dev;
    const char *bus_address = config->ext.device.address;
    struct generic_stream_out *out = hashmapGet(adev->out_bus_stream_map, bus_address);
    if (out) {
        pthread_mutex_lock(&out->lock);
        int gainIndex = (config->gain.values[0] - out->gain_stage.min_value) /
            out->gain_stage.step_value;
        int totalSteps = (out->gain_stage.max_value - out->gain_stage.min_value) /
            out->gain_stage.step_value;
        int minDb = out->gain_stage.min_value / 100;
        int maxDb = out->gain_stage.max_value / 100;
        // curve: 10^((minDb + (maxDb - minDb) * gainIndex / totalSteps) / 20)
        out->amplitude_ratio = pow(10,
                (minDb + (maxDb - minDb) * (gainIndex / (float)totalSteps)) / 20);
        pthread_mutex_unlock(&out->lock);
        ALOGD("%s: set audio gain: %f on %s",
                __func__, out->amplitude_ratio, bus_address);
    } else {
        ALOGE("%s: can not find output stream by bus_address:%s", __func__, bus_address);
        ret = -EINVAL;
    }
    return ret;
}

static int adev_create_audio_patch(struct audio_hw_device *dev,
        unsigned int num_sources,
        const struct audio_port_config *sources,
        unsigned int num_sinks,
        const struct audio_port_config *sinks,
        audio_patch_handle_t *handle) {
    static unsigned int handle_counter = 0;
    // Logging only, no real work is done here
    for (unsigned int i = 0; i < num_sources; i++) {
        ALOGD("%s: source[%d] type=%d address=%s", __func__, i, sources[i].type,
                sources[i].type == AUDIO_PORT_TYPE_DEVICE
                ? sources[i].ext.device.address
                : "");
    }
    for (unsigned int i = 0; i < num_sinks; i++) {
        ALOGD("%s: sink[%d] type=%d address=%s", __func__, i, sinks[i].type,
                sinks[i].type == AUDIO_PORT_TYPE_DEVICE ? sinks[i].ext.device.address
                : "N/A");
    }
    if (num_sources == 1 && num_sinks == 1 &&
            sources[0].type == AUDIO_PORT_TYPE_DEVICE &&
            sinks[0].type == AUDIO_PORT_TYPE_DEVICE) {
        // The same audio_patch_handle_t will be passed to release_audio_patch
        *handle = handle_counter;
        patch_list* item = (patch_list*)malloc(sizeof(patch_list));

        item->patch.id = handle_counter;
        item->patch.num_sources = num_sources;
        memcpy(item->patch.sources, sources, sizeof(struct audio_port_config) * num_sources);
        item->patch.num_sinks = num_sinks;
        memcpy(item->patch.sinks, sinks, sizeof(struct audio_port_config) * num_sinks);
        list_init(&item->list);
        if (!patch_head)
            patch_head = &item->list;
        else
            list_add_tail(patch_head, &item->list);
        ALOGD("%s: handle: %d", __func__, *handle);

        ++handle_counter;
    }

    return 0;
}

static int adev_release_audio_patch(struct audio_hw_device *dev,
        audio_patch_handle_t handle) {
    if (!patch_head)
        return 0;

    struct listnode* node;
    list_for_each(node, patch_head) {
        patch_list* item = node_to_item(node, patch_list, list);
        if (item->patch.id == handle) {
            list_remove(node);
            free(item);
        }
    }
    ALOGD("%s: handle: %d", __func__, handle);
    return 0;
}

static int adev_close(hw_device_t *dev) {
    struct generic_audio_device *adev = (struct generic_audio_device *)dev;
    int ret = 0;
    if (!adev)
        return 0;

    pthread_mutex_lock(&adev_init_lock);

    if (audio_device_ref_count == 0) {
        ALOGE("adev_close called when ref_count 0");
        ret = -EINVAL;
        goto error;
    }

    if ((--audio_device_ref_count) == 0) {
        if (adev->device_cards) {
            close_mixers_by_array(adev->device_cards);
        }
        if (adev->out_bus_stream_map) {
            hashmapFree(adev->out_bus_stream_map);
        }
        free(adev);
    }

error:
    pthread_mutex_unlock(&adev_init_lock);
    return ret;
}

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

static int adev_open(const hw_module_t *module,
        const char *name, hw_device_t **device) {
    static struct generic_audio_device *adev;

    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0)
        return -EINVAL;

    pthread_mutex_lock(&adev_init_lock);
    if (audio_device_ref_count != 0) {
        *device = &adev->device.common;
        audio_device_ref_count++;
        ALOGV("%s: returning existing instance of adev", __func__);
        ALOGV("%s: exit", __func__);
        goto unlock;
    }
    adev = calloc(1, sizeof(struct generic_audio_device));

    pthread_mutex_init(&adev->lock, (const pthread_mutexattr_t *) NULL);

    adev->device.common.tag = HARDWARE_DEVICE_TAG;
    adev->device.common.version = AUDIO_DEVICE_API_VERSION_3_0;
    adev->device.common.module = (struct hw_module_t *) module;
    adev->device.common.close = adev_close;

    adev->device.init_check = adev_init_check;               // no op
    adev->device.set_voice_volume = adev_set_voice_volume;   // no op
    adev->device.set_master_volume = adev_set_master_volume; // no op
    adev->device.get_master_volume = adev_get_master_volume; // no op
    adev->device.set_master_mute = adev_set_master_mute;
    adev->device.get_master_mute = adev_get_master_mute;
    adev->device.set_mode = adev_set_mode;                   // no op
    adev->device.set_mic_mute = adev_set_mic_mute;
    adev->device.get_mic_mute = adev_get_mic_mute;
    adev->device.set_parameters = adev_set_parameters;       // no op
    adev->device.get_parameters = adev_get_parameters;       // no op
    adev->device.get_input_buffer_size = adev_get_input_buffer_size;
    adev->device.open_output_stream = adev_open_output_stream;
    adev->device.close_output_stream = adev_close_output_stream;
    adev->device.open_input_stream = adev_open_input_stream;
    adev->device.close_input_stream = adev_close_input_stream;
    adev->device.dump = adev_dump;

    // New in AUDIO_DEVICE_API_VERSION_3_0
    adev->device.set_audio_port_config = adev_set_audio_port_config;
    adev->device.create_audio_patch = adev_create_audio_patch;
    adev->device.release_audio_patch = adev_release_audio_patch;

    *device = &adev->device.common;

    //common version
    adev->device_cards = cards;
    if (open_mixers_by_array(adev->device_cards) != 0) {
        free(adev);
        ALOGE("Unable to open and setup some mixers, aborting.");
        return -EINVAL;
    }

    adev->mode = AUDIO_MODE_NORMAL;

    // Initialize the bus address to output stream map
    adev->out_bus_stream_map = hashmapCreate(5, str_hash_fn, str_eq);

    audio_device_ref_count++;

unlock:
    pthread_mutex_unlock(&adev_init_lock);
    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = adev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = AUDIO_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = AUDIO_HARDWARE_MODULE_ID,
        .name = "Car audio HW HAL for Renesas GEN3",
        .author = "GlobalLogic",
        .methods = &hal_module_methods,
    },
};
