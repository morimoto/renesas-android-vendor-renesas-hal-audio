#ifndef PLATFORM_DEPENDENCIES_H
#define PLATFORM_DEPENDENCIES_H

#include "../audio_hal_types.h"

#define MIXER_PLAY_VOL              "DVC Out Playback Volume"
#define MIXER_CAPTURE_VOL           "DVC In Capture Volume"

#ifdef ENABLE_ADSP
#define MIXER_ADSP_PLAY_VOL         "PlaybackVolume" /* Volume gain in ADSP */
#endif

#define MIXER_PLAY_V_DEFAULT        260000      /* Max volume w/o distortion */
#define MIXER_PLAY_V_MAX            0x7fffff

#define MIXER_CAPTURE_V_DEFAULT     0x2DC6C0
#define MIXER_CAPTURE_V_MAX         0x7fffff

#ifdef ENABLE_ADSP
#define MIXER_ADSP_PLAY_VOL_DEFAULT 25 /* This value was get experimentally */
#endif

#define MIXER_MAXIMUM_LPCM_CHANNELS "Maximum LPCM channels"

/* ALSA cards for GEN3 */
#define PCM_CARD_GEN3             0
#define PCM_DEVICE_GEN3           0
#define PCM_CARD_GEN3_HDMI        1

#define PCM_CARD_DEFAULT          PCM_CARD_GEN3
#define PCM_DEVICE_DEFAULT        PCM_DEVICE_GEN3

#define IN_CHANNELS_DEFAULT 2
#define OUT_CHANNELS_DEFAULT 2

#define DEFAULT_OUT_SAMPLING_RATE   48000
#define DEFAULT_IN_SAMPLING_RATE    48000

struct route_setting defaults[] = {
    /* general */
    {
        .ctl_name = MIXER_PLAY_VOL,
        .intval = MIXER_PLAY_V_DEFAULT,
    },
    {
        .ctl_name = MIXER_CAPTURE_VOL,
        .intval = MIXER_CAPTURE_V_DEFAULT,
    },
#ifdef ENABLE_ADSP
    {
        .ctl_name = MIXER_ADSP_PLAY_VOL,
        .intval = MIXER_ADSP_PLAY_VOL_DEFAULT,
    },
#endif
    {
        .ctl_name = NULL,
    },
};

struct device_card cards[] = {
    {
        .card = PCM_CARD_GEN3,
        .defaults = defaults,
        .mixer = 0,
    },
    {
        .card = UINT32_MAX,
    }
};

#endif
