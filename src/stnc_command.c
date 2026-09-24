#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "stnc_command.h"
#include "stnc_core.h"

static void stnc_command_print_usage(void)
{
    printf("Usage:\n  stnc-core\n  stnc-core status\n  stnc-core help\n");
}

static int stnc_command_status(void)
{
    const stnc_core_chain_state *state = stnc_core_get_chain_state();

    if (state == NULL || !state->available) {
        fprintf(stderr, "Chain state is unavailable.\n");
        return 1;
    }

    printf(
        "Chain status\n"
        "  Connected: yes\n"
        "  Height: %" PRIu64 "\n"
        "  Blocks: %" PRIu32 "\n"
        "  Protocol: %" PRIu32 "\n",
        state->height,
        state->block_count,
        state->protocol_revision
    );

    return 0;
}

int stnc_command_run(int argc, char **argv)
{
    if (argc < 2 || argv == NULL) {
        return 2;
    }

    if (strcmp(argv[1], "status") == 0) {
        if (argc != 2) {
            stnc_command_print_usage();
            return 1;
        }
        return stnc_command_status();
    }

    if (strcmp(argv[1], "help") == 0 ||
        strcmp(argv[1], "--help") == 0 ||
        strcmp(argv[1], "-h") == 0) {
        stnc_command_print_usage();
        return 0;
    }

    fprintf(stderr, "Unknown command: %s\n", argv[1]);
    stnc_command_print_usage();
    return 1;
}
