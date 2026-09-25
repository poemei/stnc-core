#include <stddef.h>

#include "stnc_http.h"
#include "stnc_platform.h"

int stnc_http_get_https(
    const char *host,
    const char *path,
    char *buffer,
    size_t capacity,
    size_t *length
)
{
    if (length != NULL) {
        *length = 0;
    }

    if (host == NULL || host[0] == '\0' ||
        path == NULL || path[0] != '/' ||
        buffer == NULL || capacity == 0 ||
        length == NULL) {
        return 1;
    }

    return stnc_platform_https_get(
        host,
        path,
        buffer,
        capacity,
        length
    );
}
