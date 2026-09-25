#include <stddef.h>

#include "stnc_http.h"

int stnc_platform_https_get(
    const char *host,
    const char *path,
    char *buffer,
    size_t capacity,
    size_t *length
)
{
    (void)host;
    (void)path;
    (void)buffer;
    (void)capacity;

    if (length != NULL) {
        *length = 0;
    }

    return 1;
}

int main(void)
{
    size_t length;
    char buffer[8];

    length = 99u;

    if (stnc_http_get_https(NULL, "/", buffer, sizeof(buffer), &length) == 0 ||
        length != 0u) {
        return 1;
    }

    length = 99u;

    if (stnc_http_get_https("example.org", "bad", buffer, sizeof(buffer), &length) == 0 ||
        length != 0u) {
        return 1;
    }

    return 0;
}
