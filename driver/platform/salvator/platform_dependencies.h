#ifndef PLATFORM_DEPENDENCIES_H
#define PLATFORM_DEPENDENCIES_H

#include "../audio_hal_types.h"

/* Supported features */
#ifdef GEN3_HFP_SUPPORT
#undef GEN3_HFP_SUPPORT
#endif
#ifdef GEN3_FM_SUPPORT
#undef GEN3_HFP_SUPPORT
#endif

#define MIXER_PLAY_VOL              "DVC Out Playback Volume"
#define MIXER_CAPTURE_VOL           "DVC In Capture Volume"

#define MIXER_PLAY_V_DEFAULT        260000      /* Max volume w/o distortion */
#define MIXER_PLAY_V_MAX            0x7fffff

#define MIXER_CAPTURE_V_DEFAULT     0x2DC6C0
#define MIXER_CAPTURE_V_MAX         0x7fffff

#define MIXER_MAXIMUM_LPCM_CHANNELS "Maximum LPCM channels"

/* ALSA cards for GEN3 */
#define PCM_CARD_GEN3             0
#define PCM_DEVICE_GEN3_OUT       0
#define PCM_DEVICE_GEN3_IN        0
#define PCM_CARD_GEN3_HDMI        1

#define PCM_CARD_DEFAULT          PCM_CARD_GEN3
#define PCM_DEVICE_OUT            PCM_DEVICE_GEN3_OUT
#define PCM_DEVICE_IN             PCM_DEVICE_GEN3_IN

#define IN_CHANNELS_DEFAULT 2
#define OUT_CHANNELS_DEFAULT 2

#define DEFAULT_OUT_SAMPLING_RATE   48000
#define DEFAULT_IN_SAMPLING_RATE    48000

static struct route_setting defaults[] = {
    /* general */
    {
        .ctl_name = MIXER_PLAY_VOL,
        .intval = MIXER_PLAY_V_DEFAULT,
    },
    {
        .ctl_name = MIXER_CAPTURE_VOL,
        .intval = MIXER_CAPTURE_V_DEFAULT,
    },
    {
        .ctl_name = NULL,
    },
};

static struct device_card cards[] = {
    {
        .card = PCM_CARD_GEN3,
        .defaults = defaults,
        .mixer = 0,
    },
    {
        .card = UINT32_MAX,
    }
};

#endif // PLATFORM_DEPENDENCIES_H
