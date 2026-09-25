#ifndef STNC_HTTP_H
#define STNC_HTTP_H

#include <stddef.h>

int stnc_http_get_https(
    const char *host,
    const char *path,
    char *buffer,
    size_t capacity,
    size_t *length
);

#endif
