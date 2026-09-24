#include <stdio.h>

#include "stnc_command.h"
#include "stnc_core.h"

int main(int argc, char **argv)
{
    int result;
    int command_mode;

    printf("STNC Core\n");

    command_mode = argc > 1;

    result = stnc_core_init();

    if (result != 0) {
        fprintf(
            stderr,
            "STNC Core initialization failed.\n"
        );

        return 1;
    }

    if (command_mode) {
        result = stnc_command_run(
            argc,
            argv
        );

        stnc_core_shutdown();

        return result;
    }

    result = stnc_core_run();

    if (result != 0) {
        fprintf(
            stderr,
            "STNC Core runtime failed.\n"
        );

        stnc_core_shutdown();

        return 1;
    }

    stnc_core_shutdown();

    printf("STNC Core stopped.\n");

    return 0;
}
