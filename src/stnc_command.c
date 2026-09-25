#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "stnc_command.h"
#include "stnc_core.h"
#include "stnc_stnc.h"
#include "stnc_wallet.h"
#include "stnc_wallet_store.h"

static void stnc_command_print_usage(void)
{
    printf(
        "Usage:\n"
        "  stnc-core\n"
        "  stnc-core status\n"
        "  stnc-core address identity <source>\n"
        "  stnc-core address contract <source>\n"
        "  stnc-core address wallet <source>\n"
        "  stnc-core balance <stnw0_...>\n"
        "  stnc-core wallet create\n"
        "  stnc-core wallet show\n"
        "  stnc-core wallet balance\n"
        "  stnc-core contract state <stnc0_...>\n"
        "  stnc-core help\n"
    );
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

static int stnc_command_address(int argc, char **argv)
{
    uint16_t type;
    char address[STNC_STNC_ADDRESS_MAX_SIZE + 1u];

    if (argc != 4) {
        stnc_command_print_usage();
        return 1;
    }

    if (strcmp(argv[2], "identity") == 0) {
        type = STNC_STNC_ADDRESS_IDENTITY;
    } else if (strcmp(argv[2], "contract") == 0) {
        type = STNC_STNC_ADDRESS_CONTRACT;
    } else if (strcmp(argv[2], "wallet") == 0) {
        type = STNC_STNC_ADDRESS_WALLET;
    } else {
        fprintf(stderr, "Unknown address type: %s\n", argv[2]);
        return 1;
    }

    if (argv[3][0] == '\0' ||
        stnc_core_derive_address(
            type,
            (const uint8_t *)argv[3],
            strlen(argv[3]),
            address,
            sizeof(address)
        ) != 0) {
        fprintf(stderr, "Address derivation failed.\n");
        return 1;
    }

    printf("%s\n", address);
    return 0;
}


static int stnc_command_balance(int argc, char **argv)
{
    uint64_t units;
    if (argc != 3) { stnc_command_print_usage(); return 1; }
    if (stnc_core_balance(argv[2], &units) != 0) {
        fprintf(stderr, "Balance query failed.\n"); return 1;
    }
    printf("%" PRIu64 "\n", units);
    return 0;
}

static int stnc_command_contract(int argc, char **argv)
{
    stnc_contract_state state;
    if (argc != 4 || strcmp(argv[2], "state") != 0) { stnc_command_print_usage(); return 1; }
    if (stnc_core_contract_state(argv[3], &state) != 0) {
        fprintf(stderr, "Contract state query failed.\n"); return 1;
    }
    printf("Contract state\n  State: %u\n  Type: %u\n  Sequence: %" PRIu64
           "\n  Created: %" PRIu64 "\n  Participants: %u\n  Terms bytes: %" PRIu32 "\n",
        (unsigned int)state.state,(unsigned int)state.type,state.sequence,state.created_at,
        (unsigned int)state.participant_count,state.terms_length);
    return 0;
}


static int stnc_command_wallet(int argc,char **argv)
{
    stnc_wallet_key key;char address[STNC_WALLET_ADDRESS_SIZE+1u];uint64_t units;
    memset(&key,0,sizeof(key));
    if(argc!=3){stnc_command_print_usage();return 1;}
    if(strcmp(argv[2],"create")==0){
        if(stnc_wallet_store_exists()){fprintf(stderr,"Wallet already exists.\n");return 1;}
        if(stnc_wallet_store_create(&key)!=0||stnc_wallet_address(&key,address)!=0){stnc_wallet_clear(&key);fprintf(stderr,"Wallet creation failed.\n");return 1;}
        printf("%s\n",address);stnc_wallet_clear(&key);return 0;
    }
    if(strcmp(argv[2],"show")==0||strcmp(argv[2],"balance")==0){
        if(stnc_wallet_store_load(&key)!=0||stnc_wallet_address(&key,address)!=0){stnc_wallet_clear(&key);fprintf(stderr,"Wallet is unavailable.\n");return 1;}
        if(strcmp(argv[2],"show")==0){printf("%s\n",address);stnc_wallet_clear(&key);return 0;}
        if(stnc_core_balance(address,&units)!=0){stnc_wallet_clear(&key);fprintf(stderr,"Wallet balance query failed.\n");return 1;}
        printf("%" PRIu64 "\n",units);stnc_wallet_clear(&key);return 0;
    }
    stnc_wallet_clear(&key);stnc_command_print_usage();return 1;
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

    if (strcmp(argv[1], "address") == 0) {
        return stnc_command_address(argc, argv);
    }

    if (strcmp(argv[1], "balance") == 0) return stnc_command_balance(argc, argv);
    if (strcmp(argv[1], "wallet") == 0) return stnc_command_wallet(argc, argv);
    if (strcmp(argv[1], "contract") == 0) return stnc_command_contract(argc, argv);

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
