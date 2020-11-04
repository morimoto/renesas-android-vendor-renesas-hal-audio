#ifndef PLATFORM_DEPENDENCIES_H
#define PLATFORM_DEPENDENCIES_H

#include "../audio_hal_types.h"

/* Supported features */
#define GEN3_HFP_SUPPORT
#define GEN3_FM_SUPPORT
#define GEN3_HW_MIXER
#ifdef ENABLE_ADSP
  #undef GEN3_HW_MIXER
#endif // ENABLE_ADSP

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

#define MIXER_DVC1_IN_CAPTURE_VOL    "DVC1 In Capture Volume"

/* GEN3 mixer values */
#define MIXER_PLAY_V_MASTER_DEF     180
#define MIXER_PLAY_V_DAC_DEF        {180, 180}

#define MIXER_CAP_V_MASTER_DEF      230
#define MIXER_CAP_V_ADC_DEF         {230 , 230}

#define MIXER_DVC1_IN_CAP_VOL_DEF    2200000

#define MIXER_OUT_RAMP_UP_RATE_DVC0  "DVC0 Out Ramp Up Rate"
#define MIXER_OUT_RAMP_DOWN_RATE_DVC0 "DVC0 Out Ramp Down Rate"
#define MIXER_DVC0_OUT_PLAY_VOL "DVC0 Out Playback Volume"

// HW Mixer defines
#define MIXER_CTU00_PASS "CTU00 Pass"
#define MIXER_CTU01_PASS "CTU01 Pass"
#define MIXER_CTU02_PASS "CTU02 Pass"
#define MIXER_CTU03_PASS "CTU03 Pass"

#define MIXER_OUT_RAMP_UP_RATE_DVC0_DEF 0
#define MIXER_OUT_RAMP_DOWN_RATE_DVC0_DEF 0
#define MIXER_DVC0_OUT_PLAY_VOL_DEF {1088607, 1088607, 1088607, 1088607, 1088607, 1088607, 1088607, 1088607}
#define MIXER_CTU_PASS_DEFAULT {1, 2, 3, 4, 5, 6, 7, 8}

#define MIXER_CTU00_PASS_DEF MIXER_CTU_PASS_DEFAULT
#define MIXER_CTU01_PASS_DEF MIXER_CTU_PASS_DEFAULT
#define MIXER_CTU02_PASS_DEF MIXER_CTU_PASS_DEFAULT
#define MIXER_CTU03_PASS_DEF MIXER_CTU_PASS_DEFAULT

/* ALSA cards for GEN3 */
#define PCM_CARD_GEN3             0

#ifdef ENABLE_ADSP
  #define PCM_DEVICE_GEN3_OUT       1
  #define PCM_DEVICE_GEN3_IN        0
#else
  #define PCM_DEVICE_GEN3_OUT_BUS0  0
  #define PCM_DEVICE_GEN3_OUT_BUS1  1
  #define PCM_DEVICE_GEN3_OUT_BUS2  2
  #define PCM_DEVICE_GEN3_IN        3
  #define PCM_DEVICE_GEN3_OUT       PCM_DEVICE_GEN3_OUT_BUS0
#endif // ENABLE_ADSP

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
        .u = {
            .intval = 1, /* Master + Individual */
        },
        .type = ROUTE_INTVAL,
    },
    {
        .ctl_name = MIXER_PLAY_VOL_MASTER,
        .u = {
            .intval = MIXER_PLAY_V_MASTER_DEF,
        },
        .type = ROUTE_INTVAL,
    },
    {
        .ctl_name = MIXER_PLAY_VOL_DAC1,
        .u = {
            .intarr = MIXER_PLAY_V_DAC_DEF,
        },
        .type = ROUTE_INTARR,
    },
    {
        .ctl_name = MIXER_PLAY_VOL_DAC2,
        .u = {
            .intarr = MIXER_PLAY_V_DAC_DEF,
        },
        .type = ROUTE_INTARR,
    },
    {
        .ctl_name = MIXER_PLAY_VOL_DAC3,
        .u = {
            .intarr = MIXER_PLAY_V_DAC_DEF,
        },
        .type = ROUTE_INTARR,
    },
    {
        .ctl_name = MIXER_PLAY_VOL_DAC4,
        .u = {
            .intarr = MIXER_PLAY_V_DAC_DEF,
        },
        .type = ROUTE_INTARR,
    },
    {
        .ctl_name = MIXER_OUT_RAMP_UP_RATE_DVC0,
        .u = {
            .intval = MIXER_OUT_RAMP_UP_RATE_DVC0_DEF,
        },
        .type = ROUTE_INTVAL,
    },
        {
        .ctl_name = MIXER_OUT_RAMP_DOWN_RATE_DVC0,
        .u = {
            .intval = MIXER_OUT_RAMP_DOWN_RATE_DVC0_DEF,
        },
        .type = ROUTE_INTVAL,
    },
    {
        .ctl_name = MIXER_DVC0_OUT_PLAY_VOL,
        .u = {
            .intarr = MIXER_DVC0_OUT_PLAY_VOL_DEF,
        },
        .type = ROUTE_INTARR,
    },
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

    /* capture */
    {
        .ctl_name = MIXER_CAP_VOL_CTRL_TYPE,
        .u = {
            .intval = 1, /* Master + Individual */
        },
        .type = ROUTE_INTVAL,
    },
    {
        .ctl_name = MIXER_CAP_MASTER_VOL,
        .u = {
            .intval = MIXER_CAP_V_MASTER_DEF,
        },
        .type = ROUTE_INTVAL,
    },
    {
        .ctl_name = MIXER_CAP_VOL_ADC1,
        .u = {
            .intarr = MIXER_CAP_V_ADC_DEF,
        },
        .type = ROUTE_INTARR,
    },
    {
        .ctl_name = MIXER_CAP_VOL_ADC2,
        .u = {
            .intarr = MIXER_CAP_V_ADC_DEF,
        },
        .type = ROUTE_INTARR,
    },
    {
        .ctl_name = MIXER_CAP_VOL_ADC3,
        .u = {
            .intarr = MIXER_CAP_V_ADC_DEF,
        },
        .type = ROUTE_INTARR,
    },

    /* end of list */
    { .ctl_name = NULL, },
};

static struct route_setting defaultsfm[] = {
    {
        .ctl_name = MIXER_DVC1_IN_CAPTURE_VOL,
        .u = {
            .intval = MIXER_DVC1_IN_CAP_VOL_DEF,
        },
        .type = ROUTE_INTVAL,
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
#ifdef GEN3_FM_SUPPORT
    {
        .card = PCM_CARD_GEN3_FM,
        .defaults = defaultsfm,
        .mixer = 0,
    },
#endif //GEN3_FM_SUPPORT
    {
        .card = UINT32_MAX,
    }
};

#endif // PLATFORM_DEPENDENCIES_H
