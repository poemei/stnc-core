#include "stnc_core.h"
#include "stnc_platform.h"

static stnc_core_state core_state = STNC_CORE_STATE_UNINITIALIZED;

int stnc_core_init(void)
{
    if (core_state != STNC_CORE_STATE_UNINITIALIZED) {
        return 1;
    }

    if (stnc_platform_init() != 0) {
        return 1;
    }

    core_state = STNC_CORE_STATE_INITIALIZED;

    return 0;
}

int stnc_core_run(void)
{
    if (core_state != STNC_CORE_STATE_INITIALIZED) {
        return 1;
    }

    core_state = STNC_CORE_STATE_RUNNING;

    /*
     * Phase 1 runtime work will eventually occur here.
     *
     * For the current lifecycle build there is no active service
     * to maintain, so begin orderly termination immediately.
     */
    core_state = STNC_CORE_STATE_STOPPING;

    return 0;
}
void stnc_core_request_stop(void)
{
    if (core_state == STNC_CORE_STATE_RUNNING) {
        core_state = STNC_CORE_STATE_STOPPING;
    }
}

void stnc_core_shutdown(void)
{
    if (core_state == STNC_CORE_STATE_UNINITIALIZED ||
        core_state == STNC_CORE_STATE_STOPPED) {
        return;
    }

    stnc_platform_shutdown();

    core_state = STNC_CORE_STATE_STOPPED;
}

stnc_core_state stnc_core_get_state(void)
{
    return core_state;
}