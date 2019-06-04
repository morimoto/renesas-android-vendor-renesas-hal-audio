#ifndef PLATFORM_DEPENDENCIES_H
#define PLATFORM_DEPENDENCIES_H

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
#define MIXER_PLAY_V_DAC_DEF        192

#define MIXER_CAP_V_MASTER_DEF      230
#define MIXER_CAP_V_ADC_DEF         230

#define MIXER_DVC_IN_CAP_VOL_DEF    2200000

/* ALSA cards for GEN3 */
#define CARD_GEN3                 0
#define CARD_GEN3_DEFAULT         CARD_GEN3

#define IN_CHANNELS 6
#define OUT_CHANNELS 8

struct route_setting
{
    const char *    ctl_name;
    int             intval;
    const char *    strval;
};

/* These are values that never change */
struct route_setting defaults[] = {
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

struct route_setting defaultsfm[] = {
    {
        .ctl_name = MIXER_DVC_IN_CAPTURE_VOL,
        .intval = MIXER_DVC_IN_CAP_VOL_DEF,
    },

    /* end of list */
    { .ctl_name = NULL, },
};

#endif