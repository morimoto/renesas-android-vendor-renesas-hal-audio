#ifndef PLATFORM_DEPENDENCIES_H
#define PLATFORM_DEPENDENCIES_H

#define MIXER_PLAY_VOL              "DVC Out Playback Volume"
#define MIXER_CAPTURE_VOL           "DVC In Capture Volume"

#define MIXER_PLAY_V_DEFAULT        260000      /* Max volume w/o distortion */
#define MIXER_PLAY_V_MAX            0x7fffff

#define MIXER_CAPTURE_V_DEFAULT     0x2DC6C0
#define MIXER_CAPTURE_V_MAX         0x7fffff

#define MIXER_MAXIMUM_LPCM_CHANNELS "Maximum LPCM channels"

/* ALSA cards for GEN3 */
#define CARD_GEN3                 0
#define CARD_GEN3_HDMI            1
#define CARD_GEN3_DEFAULT         CARD_GEN3

#define IN_CHANNELS 2
#define OUT_CHANNELS 2

struct route_setting
{
    const char *    ctl_name;
    int             intval;
    const char *    strval;
};

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
    {
        .ctl_name = NULL,
    },
};

#endif