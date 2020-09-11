#ifndef PLATFORM_DEPENDENCIES_H
#define PLATFORM_DEPENDENCIES_H

#include "../audio_hal_types.h"

/* Supported features */
#define GEN3_HFP_SUPPORT
#define GEN3_FM_SUPPORT

#define MIXER_PLAY_VOL_CTRL_TYPE    "DAC Volume Control Type"
#define MIXER_PLAY_VOL_MASTER       "Master Playback Volume"
#define MIXER_PLAY_VOL_DAC1         "DAC1 Playback Volume"
#define MIXER_PLAY_VOL_DAC2         "DAC2 Playback Volume"
#define MIXER_PLAY_VOL_DAC3         "DAC3 Playback Volume"
#define MIXER_PLAY_VOL_DAC4         "DAC4 Playback Volume"

#define MIXER_CAP_VOL_CTRL_TYPE     "ADC Volume Control Type"
#define MIXER_CAP_MASTER_VOL        "Master Capture Volume"
#define MIXER_CAP_VOL_ADC1          "ADC1 Capture Volume"
#define MIXER_CAP_VOL_ADC2          "ADC2 Capture Volume"
#define MIXER_CAP_VOL_ADC3          "ADC3 Capture Volume"

#define MIXER_DVC_IN_CAPTURE_VOL    "DVC In Capture Volume"

/* GEN3 mixer values */
#define MIXER_PLAY_V_MASTER_DEF     180
#define MIXER_PLAY_V_DAC_DEF        160

#define MIXER_CAP_V_MASTER_DEF      230
#define MIXER_CAP_V_ADC_DEF         230

#define MIXER_DVC_IN_CAP_VOL_DEF    2200000

/* ALSA cards for GEN3 */
#define PCM_CARD_GEN3             0

#ifdef ENABLE_ADSP
#define PCM_DEVICE_GEN3_OUT       1
#define PCM_DEVICE_GEN3_IN        0
#else
#define PCM_DEVICE_GEN3_OUT       0
#define PCM_DEVICE_GEN3_IN        1
#endif

#define PCM_CARD_DEFAULT          PCM_CARD_GEN3
#define PCM_DEVICE_OUT            PCM_DEVICE_GEN3_OUT
#define PCM_DEVICE_IN             PCM_DEVICE_GEN3_IN

#define PCM_CARD_GEN3_FM          2
#define PCM_DEVICE_GEN3_FM        0
#define PCM_CARD_FM               PCM_CARD_GEN3_FM
#define PCM_DEVICE_FM             PCM_DEVICE_GEN3_FM

#define PCM_CARD_GEN3_HFP         3
#define PCM_DEVICE_GEN3_HFP       0
#define PCM_CARD_HFP              PCM_CARD_GEN3_HFP
#define PCM_DEVICE_HFP            PCM_DEVICE_GEN3_HFP

#define IN_CHANNELS_DEFAULT 6
#define OUT_CHANNELS_DEFAULT 8
#define IN_CHANNELS_FM 2
#define IN_CHANNELS_HFP 2
#define OUT_CHANNELS_HFP 2

#define DEFAULT_OUT_SAMPLING_RATE   48000
#define DEFAULT_IN_SAMPLING_RATE    48000

#define DEFAULT_HFP_SAMPLING_RATE   16000

/* These are values that never change */
static struct route_setting defaults[] = {
    /* playback */
    {
        .ctl_name = MIXER_PLAY_VOL_CTRL_TYPE,
        .intval = 1, /* Master + Individual */
    },
    {
        .ctl_name = MIXER_PLAY_VOL_MASTER,
        .intval = MIXER_PLAY_V_MASTER_DEF,
    },
    {
        .ctl_name = MIXER_PLAY_VOL_DAC1,
        .intval = MIXER_PLAY_V_DAC_DEF,
    },
    {
        .ctl_name = MIXER_PLAY_VOL_DAC2,
        .intval = MIXER_PLAY_V_DAC_DEF,
    },
    {
        .ctl_name = MIXER_PLAY_VOL_DAC3,
        .intval = MIXER_PLAY_V_DAC_DEF,
    },
    {
        .ctl_name = MIXER_PLAY_VOL_DAC4,
        .intval = MIXER_PLAY_V_DAC_DEF,
    },

    /* capture */
    {
        .ctl_name = MIXER_CAP_VOL_CTRL_TYPE,
        .intval = 1, /* Master + Individual */
    },
    {
        .ctl_name = MIXER_CAP_MASTER_VOL,
        .intval = MIXER_CAP_V_MASTER_DEF,
    },
    {
        .ctl_name = MIXER_CAP_VOL_ADC1,
        .intval = MIXER_CAP_V_ADC_DEF,
    },
    {
        .ctl_name = MIXER_CAP_VOL_ADC2,
        .intval = MIXER_CAP_V_ADC_DEF,
    },
    {
        .ctl_name = MIXER_CAP_VOL_ADC3,
        .intval = MIXER_CAP_V_ADC_DEF,
    },

    /* end of list */
    { .ctl_name = NULL, },
};

static struct route_setting defaultsfm[] = {
    {
        .ctl_name = MIXER_DVC_IN_CAPTURE_VOL,
        .intval = MIXER_DVC_IN_CAP_VOL_DEF,
    },

    /* end of list */
    { .ctl_name = NULL, },
};

static struct device_card cards[] = {
    {
        .card = PCM_CARD_GEN3,
        .defaults = defaults,
        .mixer = 0,
    },
    {
        .card = PCM_CARD_GEN3_FM,
        .defaults = defaultsfm,
        .mixer = 0,
    },
    {
        .card = UINT32_MAX,
    }
};

#endif // PLATFORM_DEPENDENCIES_H
