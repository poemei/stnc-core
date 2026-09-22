#include <stdio.h>

#include "stnc_core.h"

int main(void)
{
    int result;

    printf("STNC Core\n");

    result = stnc_core_init();

    if (result != 0) {
        fprintf(stderr, "STNC Core initialization failed.\n");
        return 1;
    }

    result = stnc_core_run();

    if (result != 0) {
        fprintf(stderr, "STNC Core runtime failed.\n");
        stnc_core_shutdown();
        return 1;
    }

    stnc_core_shutdown();

    printf("STNC Core stopped.\n");

    return 0;
}