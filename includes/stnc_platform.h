#ifndef STNC_PLATFORM_H
#define STNC_PLATFORM_H

#include <stddef.h>

int stnc_platform_init(void);
void stnc_platform_shutdown(void);

int stnc_platform_get_app_directory(
    char *buffer,
    size_t buffer_size
);

/*
 * Install the platform-specific operator stop handler.
 *
 * The handler requests an orderly Core stop.
 *
 * Returns 0 on success.
 * Returns non-zero on failure.
 */
int stnc_platform_install_stop_handler(void);

/*
 * Suspend runtime execution for the requested number
 * of milliseconds.
 */
void stnc_platform_wait(unsigned int milliseconds);

#endif