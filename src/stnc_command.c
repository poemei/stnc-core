#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "stnc_command.h"
#include "stnc_core.h"
#include "stnc_stnc.h"
#include "stnc_wallet.h"
#include "stnc_wallet_store.h"
#include "stnc_platform.h"

static void stnc_command_print_usage(void)
{
    printf(
        "Usage:\n"
        "  stnc-core\n"
        "  stnc-core status\n"
        "  stnc-core peers\n"
        "  stnc-core pending\n"
        "  stnc-core address identity <source>\n"
        "  stnc-core address contract <source>\n"
        "  stnc-core address wallet <source>\n"
        "  stnc-core balance <stnw0_...>\n"
        "  stnc-core wallet create\n"
        "  stnc-core wallet show\n"
        "  stnc-core wallet status\n"
        "  stnc-core wallet balance\n"
        "  stnc-core transfer <stnw0_...> <units>\n"
        "  stnc-core contract state <stnc0_...>\n"
        "  stnc-core help\n"
    );
}

static int stnc_command_status(void)
{
    const stnc_core_chain_state *state=stnc_core_get_chain_state();
    const stnc_core_peer_status *peer=stnc_core_get_peer_status();
    stnc_core_runtime_status runtime;
    stnc_core_get_runtime_status(&runtime);
    printf("STNC Core status\n");
    printf("  Chain RPC: %s\n",runtime.chain_connected?"connected":"disconnected");
    printf("  Chain state: %s\n",runtime.chain_state_available?"available":"unavailable");
    if(state!=NULL&&state->available){
        printf("  Height: %" PRIu64 "\n  Blocks: %" PRIu32 "\n  Protocol: %" PRIu32 "\n",
            state->height,state->block_count,state->protocol_revision);
    }
    printf("  Root STNP discovery: %s\n",runtime.root_peer_capabilities!=0u?"qualified":"unavailable");
    printf("  P2P session: %s\n",runtime.p2p_connected?"connected":"disconnected");
    printf("  Peer candidates: %zu\n  Qualified peers: %zu\n",runtime.candidate_count,runtime.qualified_count);
    if(peer!=NULL&&peer->connected)printf("  Selected peer: %s:%u\n",peer->host,(unsigned int)peer->port);
    else printf("  Selected peer: none\n");
    return runtime.chain_connected&&runtime.chain_state_available?0:1;
}


static void stnc_print_percent(uint32_t used,uint32_t capacity)
{
    uint64_t tenths;
    if(capacity==0u){printf("unavailable");return;}
    tenths=((uint64_t)used*UINT64_C(1000))/(uint64_t)capacity;
    printf("%" PRIu64 ".%" PRIu64 "%%",tenths/10u,tenths%10u);
}

static int stnc_command_pending(void)
{
    stnc_pending_state state;
    if(stnc_core_pending(&state)!=0){fprintf(stderr,"Pending pool query failed.\n");return 1;}
    printf("Pending pool\n  Transactions: %" PRIu32 " / %" PRIu32 " (",state.count,state.max_entries);
    stnc_print_percent(state.count,state.max_entries);
    printf(")\n  Bytes: %" PRIu32 " / %" PRIu32 " (",state.bytes,state.max_bytes);
    stnc_print_percent(state.bytes,state.max_bytes);
    printf(")\n");
    return 0;
}

static int stnc_command_peers(void)
{
    const stnc_core_peer_status *peer=stnc_core_get_peer_status();
    if(peer==NULL||!peer->connected){printf("Selected P2P peer: none\n");return 0;}
    printf("Selected P2P peer\n  Endpoint: %s:%u\n  Capabilities: %" PRIu32 "\n  Qualification latency: %" PRIu64 " ms\n",
        peer->host,(unsigned int)peer->port,peer->capabilities,peer->latency_ms);
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
    if(strcmp(argv[2],"show")==0||strcmp(argv[2],"status")==0||strcmp(argv[2],"balance")==0){
        if(stnc_wallet_store_load(&key)!=0||stnc_wallet_address(&key,address)!=0){stnc_wallet_clear(&key);fprintf(stderr,"Wallet is unavailable.\n");return 1;}
        if(strcmp(argv[2],"show")==0){printf("%s\n",address);stnc_wallet_clear(&key);return 0;}
        if(strcmp(argv[2],"status")==0){
            printf("Wallet status\n  Present: yes\n  Key integrity: valid\n  Address: %s\n",address);
            if(stnc_core_balance(address,&units)==0)printf("  Accepted balance: %" PRIu64 "\n",units);
            else printf("  Accepted balance: unavailable\n");
            stnc_wallet_clear(&key);return 0;
        }
        if(stnc_core_balance(address,&units)!=0){stnc_wallet_clear(&key);fprintf(stderr,"Wallet balance query failed.\n");return 1;}
        printf("%" PRIu64 "\n",units);stnc_wallet_clear(&key);return 0;
    }
    stnc_wallet_clear(&key);stnc_command_print_usage();return 1;
}


static const char *stnc_submission_name(uint16_t result)
{
    switch(result){
    case STNC_STNC_SUBMISSION_ADMITTED:return "admitted";
    case STNC_STNC_SUBMISSION_DUPLICATE:return "duplicate";
    case STNC_STNC_SUBMISSION_POOL_FULL:return "pool-full";
    case STNC_STNC_SUBMISSION_BAD:return "bad-submission";
    case STNC_STNC_SUBMISSION_UNSUPPORTED:return "unsupported";
    case STNC_STNC_SUBMISSION_REPLAY:return "replay";
    case STNC_STNC_SUBMISSION_UNAUTHORIZED:return "unauthorized";
    case STNC_STNC_SUBMISSION_UNAVAILABLE:return "unavailable";
    case STNC_STNC_SUBMISSION_INTERNAL:return "internal";
    default:return "unknown";
    }
}

static void stnc_print_id(const uint8_t id[32])
{
    size_t i;for(i=0;i<32u;i++)printf("%02x",(unsigned int)id[i]);printf("\n");
}

static int stnc_command_transfer(int argc,char **argv)
{
    stnc_wallet_key key;char source[STNC_WALLET_ADDRESS_SIZE+1u];uint8_t nonce[STNC_WALLET_NONCE_SIZE];
    uint8_t transaction[STNC_WALLET_TRANSFER_SIZE];stnc_submission_result result;uint64_t units=0,balance=0;size_t i;
    if(argc!=4){stnc_command_print_usage();return 1;}
    if(argv[3][0]=='\0')return 1;
    for(i=0;argv[3][i]!='\0';i++){unsigned int digit;if(argv[3][i]<'0'||argv[3][i]>'9')return 1;digit=(unsigned int)(argv[3][i]-'0');if(units>(UINT64_MAX-digit)/10u)return 1;units=units*10u+digit;}
    if(units==0){fprintf(stderr,"Transfer units must be greater than zero.\n");return 1;}
    memset(&key,0,sizeof(key));memset(nonce,0,sizeof(nonce));memset(&result,0,sizeof(result));
    if(stnc_wallet_store_load(&key)!=0||stnc_wallet_address(&key,source)!=0||
       stnc_platform_random(nonce,sizeof(nonce))!=0||
       stnc_wallet_build_transfer(&key,source,argv[2],units,nonce,transaction)!=0){
        stnc_wallet_clear(&key);stnc_platform_secure_clear(nonce,sizeof(nonce));fprintf(stderr,"Transfer construction failed.\n");return 1;
    }
    stnc_wallet_clear(&key);stnc_platform_secure_clear(nonce,sizeof(nonce));
    if(stnc_core_submit_transaction(transaction,sizeof(transaction),&result)!=0){stnc_platform_secure_clear(transaction,sizeof(transaction));fprintf(stderr,"Transfer submission failed.\n");return 1;}
    stnc_platform_secure_clear(transaction,sizeof(transaction));
    if(result.result==STNC_STNC_SUBMISSION_ADMITTED||result.result==STNC_STNC_SUBMISSION_DUPLICATE){
        printf(result.result==STNC_STNC_SUBMISSION_ADMITTED?"Transfer admitted\n":"Transfer already pending\n");
        printf("Transaction: ");stnc_print_id(result.transaction_id);
        if(stnc_core_balance(source,&balance)==0)printf("Accepted balance: %" PRIu64 "\n",balance);
        else printf("Accepted balance: unavailable\n");
        return 0;
    }
    fprintf(stderr,"Transfer rejected: %s (%u)\n",stnc_submission_name(result.result),(unsigned int)result.result);return 1;
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

    if (strcmp(argv[1], "pending") == 0) { if(argc!=2){stnc_command_print_usage();return 1;} return stnc_command_pending(); }

    if (strcmp(argv[1], "peers") == 0) { if(argc!=2){stnc_command_print_usage();return 1;} return stnc_command_peers(); }

    if (strcmp(argv[1], "address") == 0) {
        return stnc_command_address(argc, argv);
    }

    if (strcmp(argv[1], "balance") == 0) return stnc_command_balance(argc, argv);
    if (strcmp(argv[1], "wallet") == 0) return stnc_command_wallet(argc, argv);
    if (strcmp(argv[1], "contract") == 0) return stnc_command_contract(argc, argv);
    if (strcmp(argv[1], "transfer") == 0) return stnc_command_transfer(argc, argv);

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
