#include "stnc_platform.h"

static int platform_initialized = 0;

int stnc_platform_init(void)
{
    if (platform_initialized) {
        return 1;
    }

    platform_initialized = 1;

    return 0;
}

void stnc_platform_shutdown(void)
{
    if (!platform_initialized) {
        return;
    }

    platform_initialized = 0;
}