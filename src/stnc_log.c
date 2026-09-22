#include <stdio.h>

#include "stnc_log.h"

static int log_initialized = 0;

int stnc_log_init(void)
{
    if (log_initialized) {
        return 1;
    }

    log_initialized = 1;

    return 0;
}

void stnc_log_info(const char *message)
{
    if (!log_initialized || message == NULL) {
        return;
    }

    fprintf(stdout, "[INFO] %s\n", message);
}

void stnc_log_warning(const char *message)
{
    if (!log_initialized || message == NULL) {
        return;
    }

    fprintf(stderr, "[WARNING] %s\n", message);
}

void stnc_log_error(const char *message)
{
    if (!log_initialized || message == NULL) {
        return;
    }

    fprintf(stderr, "[ERROR] %s\n", message);
}

void stnc_log_shutdown(void)
{
    if (!log_initialized) {
        return;
    }

    fflush(stdout);
    fflush(stderr);

    log_initialized = 0;
}