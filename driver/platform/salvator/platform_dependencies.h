#ifndef PLATFORM_DEPENDENCIES_H
#define PLATFORM_DEPENDENCIES_H

#include "../audio_hal_types.h"

/* Supported features */
#ifdef GEN3_HFP_SUPPORT
  #undef GEN3_HFP_SUPPORT
#endif // GEN3_HFP_SUPPORT
#ifdef GEN3_FM_SUPPORT
  #undef GEN3_HFP_SUPPORT
#endif // GEN3_FM_SUPPORT
#define GEN3_HW_MIXER

#define MIXER_DVC0_OUT_PLAY_VOL             "DVC0 Out Playback Volume"
#define MIXER_DVC1_IN_CAPTURE_VOL           "DVC1 In Capture Volume"

#ifdef ENABLE_ADSP
  #define MIXER_ADSP_PLAY_VOL               "PlaybackVolume" /* Volume gain in ADSP */
  #define MIXER_ADSP_PLAY_VOL_DEFAULT       8                /* Max volume w/o distortion */
#endif // ENABLE_ADSP

#define MIXER_OUT_RAMP_UP_RATE_DVC0_DEF     0
#define MIXER_OUT_RAMP_DOWN_RATE_DVC0_DEF   0
#define MIXER_DVC0_OUT_PLAY_VOL_DEFAULT     {80000, 80000}  /* Max volume w/o distortion */
#define MIXER_DVC1_IN_CAPTURE_VOL_DEFAULT   {0x2DC6C0, 0x2DC6C0}

// HW Mixer defines
#define MIXER_CTU00_PASS                    "CTU00 Pass"
#define MIXER_CTU01_PASS                    "CTU01 Pass"
#define MIXER_CTU02_PASS                    "CTU02 Pass"
#define MIXER_CTU03_PASS                    "CTU03 Pass"
#define MIXER_CTU_PASS_DEFAULT              {1, 2, 3, 4, 5, 6, 7, 8}

#define MIXER_CTU00_PASS_DEF                MIXER_CTU_PASS_DEFAULT
#define MIXER_CTU01_PASS_DEF                MIXER_CTU_PASS_DEFAULT
#define MIXER_CTU02_PASS_DEF                MIXER_CTU_PASS_DEFAULT
#define MIXER_CTU03_PASS_DEF                MIXER_CTU_PASS_DEFAULT

/* ALSA cards for GEN3 */
#define PCM_CARD_GEN3                       0
#define PCM_CARD_GEN3_HDMI                  1

#define PCM_DEVICE_GEN3_IN                  0

#define PCM_DEVICE_GEN3_OUT_BUS0            0
#define PCM_DEVICE_GEN3_OUT_BUS1            1
#define PCM_DEVICE_GEN3_OUT_BUS2            2
#define PCM_DEVICE_GEN3_OUT_BUS3            3
#define PCM_DEVICE_GEN3_OUT                 PCM_DEVICE_GEN3_OUT_BUS0

#define PCM_CARD_DEFAULT                    PCM_CARD_GEN3
#define PCM_DEVICE_OUT                      PCM_DEVICE_GEN3_OUT
#define PCM_DEVICE_IN                       PCM_DEVICE_GEN3_IN

#define IN_CHANNELS_DEFAULT                 2
#define OUT_CHANNELS_DEFAULT                2

#define DEFAULT_OUT_SAMPLING_RATE           48000
#define DEFAULT_IN_SAMPLING_RATE            48000

static struct route_setting defaults[] = {
    /* general */
    {
        .ctl_name = MIXER_DVC0_OUT_PLAY_VOL,
        .u = {
            .intarr = MIXER_DVC0_OUT_PLAY_VOL_DEFAULT,
        },
        .type = ROUTE_INTARR,
    },
    {
        .ctl_name = MIXER_DVC1_IN_CAPTURE_VOL,
        .u = {
            .intarr = MIXER_DVC1_IN_CAPTURE_VOL_DEFAULT,
        },
        .type = ROUTE_INTARR,
    },
#ifdef ENABLE_ADSP
    {
        .ctl_name = MIXER_ADSP_PLAY_VOL,
        .u = {
            .intval = MIXER_ADSP_PLAY_VOL_DEFAULT,
        },
        .type = ROUTE_INTVAL,
    },
#else
#ifdef GEN3_HW_MIXER
    {
        .ctl_name = MIXER_CTU00_PASS,
        .u = {
            .intarr = MIXER_CTU00_PASS_DEF,
        },
        .type = ROUTE_INTARR,
    },
    {
        .ctl_name = MIXER_CTU01_PASS,
        .u = {
            .intarr = MIXER_CTU01_PASS_DEF,
        },
        .type = ROUTE_INTARR,
    },
    {
        .ctl_name = MIXER_CTU02_PASS,
        .u = {
            .intarr = MIXER_CTU02_PASS_DEF,
        },
        .type = ROUTE_INTARR,
    },
    {
        .ctl_name = MIXER_CTU03_PASS,
        .u = {
            .intarr = MIXER_CTU03_PASS_DEF,
        },
        .type = ROUTE_INTARR,
    },
#endif // GEN3_HW_MIXER
#endif // ENABLE_ADSP
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
