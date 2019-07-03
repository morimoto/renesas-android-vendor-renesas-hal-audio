#ifndef AUDIO_HAL_TYPES_H
#define AUDIO_HAL_TYPES_H

struct route_setting
{
    const char *    ctl_name;
    int             intval;
    const char *    strval;
};

struct device_card
{
    unsigned int card;
    struct route_setting* defaults;
    struct mixer* mixer;
};

#endif // AUDIO_HAL_TYPES_H
