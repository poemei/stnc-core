#include <stdio.h>

#include "stnc_config.h"
#include "stnc_core.h"
#include "stnc_log.h"
#include "stnc_platform.h"

static stnc_core_state core_state = STNC_CORE_STATE_UNINITIALIZED;

int stnc_core_init(void)
{
    const stnc_config *config;
    char message[512];

    if (core_state != STNC_CORE_STATE_UNINITIALIZED) {
        return 1;
    }

    if (stnc_platform_init() != 0) {
        return 1;
    }

    if (stnc_log_init() != 0) {
        stnc_platform_shutdown();
        return 1;
    }

    if (stnc_config_init() != 0) {
        stnc_log_error(
            "STNC Core configuration initialization failed."
        );

        stnc_log_shutdown();
        stnc_platform_shutdown();

        return 1;
    }

    config = stnc_config_get();

    if (config == NULL) {
        stnc_log_error(
            "STNC Core configuration is unavailable."
        );

        stnc_config_shutdown();
        stnc_log_shutdown();
        stnc_platform_shutdown();

        return 1;
    }

    if (snprintf(
            message,
            sizeof(message),
            "Configuration loaded: peer=%s port=%u",
            config->peer,
            (unsigned int)config->port
        ) < 0) {
        stnc_config_shutdown();
        stnc_log_shutdown();
        stnc_platform_shutdown();

        return 1;
    }

    if (stnc_platform_install_stop_handler() != 0) {
        stnc_log_error(
            "STNC Core stop handler initialization failed."
        );

        stnc_config_shutdown();
        stnc_log_shutdown();
        stnc_platform_shutdown();

        return 1;
    }

    core_state = STNC_CORE_STATE_INITIALIZED;

    stnc_log_info("STNC Core initialized.");
    stnc_log_info(message);

    return 0;
}

int stnc_core_run(void)
{
    if (core_state != STNC_CORE_STATE_INITIALIZED) {
        return 1;
    }

    core_state = STNC_CORE_STATE_RUNNING;

    stnc_log_info("STNC Core running.");

    while (core_state == STNC_CORE_STATE_RUNNING) {
        stnc_platform_wait(100);
    }

    return 0;
}

void stnc_core_request_stop(void)
{
    if (core_state != STNC_CORE_STATE_RUNNING) {
        return;
    }

    core_state = STNC_CORE_STATE_STOPPING;

    stnc_log_info("STNC Core stop requested.");
}

void stnc_core_shutdown(void)
{
    if (core_state == STNC_CORE_STATE_UNINITIALIZED ||
        core_state == STNC_CORE_STATE_STOPPED) {
        return;
    }

    stnc_log_info("STNC Core shutting down.");

    stnc_config_shutdown();
    stnc_log_shutdown();
    stnc_platform_shutdown();

    core_state = STNC_CORE_STATE_STOPPED;
}

stnc_core_state stnc_core_get_state(void)
{
    return core_state;
}