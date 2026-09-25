#include <stddef.h>

#include "stnc_http.h"

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
