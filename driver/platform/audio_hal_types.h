#ifndef AUDIO_HAL_TYPES_H
#define AUDIO_HAL_TYPES_H

#define ROUTE_INTARR_MAX 8

struct route_setting
{
    const char *    ctl_name;
    union {
        int          intval;
        const char * strval;
        int          intarr[ROUTE_INTARR_MAX];
    } u;
    enum {
        ROUTE_NAN,
        ROUTE_INTVAL,
        ROUTE_STRVAL,
        ROUTE_INTARR
    } type;
};

struct device_card
{
    unsigned int card;
    struct route_setting* defaults;
    struct mixer* mixer;
};

#endif // AUDIO_HAL_TYPES_H
