#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "stnc_command.h"
#include "stnc_core.h"
#include "stnc_mining.h"
#include "stnc_contract_status.h"
#include "stnc_stnc.h"
#include "stnc_wallet.h"
#include "stnc_wallet_store.h"
#include "stnc_wallet_status.h"
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
        "  stnc-core mining status\n"
        "  stnc-core mining template\n"
        "  stnc-core mining check\n"
        "  stnc-core mining scan <attempts>\n"
        "  stnc-core mining mine-once <attempts>\n"
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
    stnc_contract_status status;
    if(argc!=4||strcmp(argv[2],"state")!=0){stnc_command_print_usage();return 1;}
    if(stnc_contract_status_read(argv[3],&status)!=0){fprintf(stderr,"Invalid contract address.\n");return 1;}
    if(!status.available){fprintf(stderr,"Contract state unavailable.\n");return 1;}
    printf("Contract state\n  Address: %s\n  State: %s (%u)\n  Type: %s (%u)\n  Sequence: %" PRIu64
           "\n  Created: %" PRIu64 "\n  Participants: %u\n  Terms bytes: %" PRIu32 "\n",
        status.address,stnc_contract_state_name(status.state.state),(unsigned int)status.state.state,
        stnc_contract_type_name(status.state.type),(unsigned int)status.state.type,status.state.sequence,
        status.state.created_at,(unsigned int)status.state.participant_count,status.state.terms_length);
    return 0;
}


static int stnc_command_mining(int argc,char **argv)
{
    size_t i;
    if(argc!=3&&argc!=4){stnc_command_print_usage();return 1;}
    if(strcmp(argv[2],"mine-once")==0||strcmp(argv[2],"scan")==0){
        int submit_solution=strcmp(argv[2],"mine-once")==0;
        uint8_t *payload=NULL,block[STNC_STNC_BLOCK_HEADER_SIZE],digest[32],accepted_id[32],accepted_work[40];
        size_t payload_length=0u;stnc_mining_template work;stnc_mining_context checked;
        uint64_t attempts=0u,nonce=0u,height=0u;size_t j;stnc_wallet_key key;char address[STNC_STNC_ADDRESS_IDENTITY_SIZE+1u];
        memset(&key,0,sizeof(key));
        if(argc!=4||argv[3][0]=='\0'){stnc_command_print_usage();return 1;}
        for(j=0u;argv[3][j]!='\0';j++){unsigned int d;if(argv[3][j]<'0'||argv[3][j]>'9')return 1;d=(unsigned int)(argv[3][j]-'0');if(attempts>(UINT64_MAX-d)/10u)return 1;attempts=attempts*10u+d;}
        if(attempts==0u){fprintf(stderr,"Mining attempts must be greater than zero.\n");return 1;}
        if(stnc_wallet_store_load(&key)!=0){stnc_wallet_clear(&key);fprintf(stderr,"Mining requires a valid Core wallet.\n");return 1;}
        if(stnc_core_derive_address(STNC_STNC_ADDRESS_IDENTITY,key.public_key,sizeof(key.public_key),address,sizeof(address))!=0){
            stnc_wallet_clear(&key);fprintf(stderr,"Mining identity derivation failed.\n");return 1;
        }
        stnc_wallet_clear(&key);
        if(stnc_core_mining_template(&payload,&payload_length,&work)!=0||work.block_length!=sizeof(block)){
            stnc_core_mining_template_release(payload);fprintf(stderr,"Mining template unavailable or unsupported.\n");return 1;
        }
        memcpy(block,work.block,sizeof(block));
        if(memcmp(block,work.parent_id,32u)!=0){
            stnc_core_mining_template_release(payload);fprintf(stderr,"Mining template parent mismatch.\n");return 1;
        }
        {stnc_mining_context context;
        if(stnc_core_mining_context(&context)!=0||!context.template_available||
           memcmp(context.tip_id,work.parent_id,32u)!=0||memcmp(context.target,block+120u,32u)!=0){
            stnc_core_mining_template_release(payload);fprintf(stderr,"Mining template does not match current Chain context.\n");return 1;
        }}
        {stnc_core_work_base_result base=stnc_core_check_work_base(work.parent_id,&checked);
        if(base!=STNC_CORE_WORK_BASE_CURRENT){stnc_core_mining_template_release(payload);
            fprintf(stderr,base==STNC_CORE_WORK_BASE_STALE?"Mining work base is stale.\n":"Mining work base check failed.\n");return 1;}
        if(!checked.template_available||memcmp(checked.tip_id,work.parent_id,32u)!=0||
           memcmp(checked.target,block+120u,32u)!=0){
            stnc_core_mining_template_release(payload);fprintf(stderr,"Checked mining context does not match template.\n");return 1;}}
        {uint64_t first_nonce=0u;
        size_t k;
        for(k=0u;k<STNC_STNC_MINING_NONCE_SIZE;k++)first_nonce=(first_nonce<<8)|block[STNC_STNC_MINING_NONCE_OFFSET+k];
        if(attempts-1u>UINT64_MAX-first_nonce){
            stnc_core_mining_template_release(payload);
            fprintf(stderr,"Mining pass exceeds the canonical nonce range.\n");return 1;
        }
        switch(stnc_mining_search(block,first_nonce,attempts,&nonce,digest)){
        case STNC_MINING_FOUND:
            if(!stnc_mining_hash_meets_target(digest,block+120u)){
                stnc_core_mining_template_release(payload);fprintf(stderr,"Mining worker returned an invalid solution.\n");return 1;
            }
            {uint8_t verified_digest[32];
            if(stnc_mining_hash(block,verified_digest)!=0||memcmp(verified_digest,digest,32u)!=0||
               !stnc_mining_hash_meets_target(verified_digest,block+120u)){
                stnc_core_mining_template_release(payload);fprintf(stderr,"Mining solution verification failed.\n");return 1;
            }}
            if(!submit_solution){
                stnc_core_work_base_result base=stnc_core_check_work_base(work.parent_id,&checked);
                if(base!=STNC_CORE_WORK_BASE_CURRENT){
                    stnc_core_mining_template_release(payload);
                    fprintf(stderr,base==STNC_CORE_WORK_BASE_STALE?
                        "Mining scan found valid work after its base became stale.\n":
                        "Mining scan final work-base check failed.\n");
                    return 1;
                }
                if(!checked.template_available||memcmp(checked.tip_id,work.parent_id,32u)!=0||
                   memcmp(checked.target,block+120u,32u)!=0){
                    stnc_core_mining_template_release(payload);
                    fprintf(stderr,"Mining scan found valid work against changed Chain context.\n");return 1;
                }
                printf("Mining scan found valid current work\n  Nonce: %" PRIu64 "\n  Hash: ",nonce);
                for(j=0u;j<32u;j++)printf("%02x",(unsigned int)digest[j]);
                printf("\n");
                stnc_core_mining_template_release(payload);return 0;
            }
            {stnc_core_work_base_result base=stnc_core_check_work_base(work.parent_id,&checked);
            if(base!=STNC_CORE_WORK_BASE_CURRENT){stnc_core_mining_template_release(payload);
                fprintf(stderr,base==STNC_CORE_WORK_BASE_STALE?"Solved mining work became stale.\n":"Solved mining work recheck failed.\n");return 1;}
            if(!checked.template_available||memcmp(checked.tip_id,work.parent_id,32u)!=0||
               memcmp(checked.target,block+120u,32u)!=0){
                stnc_core_mining_template_release(payload);fprintf(stderr,"Solved mining work no longer matches checked Chain context.\n");return 1;}}
            if(stnc_core_submit_work(work.parent_id,work.work_id,address,block,sizeof(block),accepted_id,&height,accepted_work)!=0){
                stnc_core_mining_template_release(payload);fprintf(stderr,"Solved mining work was not accepted by Chain.\n");return 1;
            }
            printf("Mining solution accepted\n  Nonce: %" PRIu64 "\n  Height: %" PRIu64 "\n  Block: ",nonce,height);
            for(j=0u;j<32u;j++)printf("%02x",(unsigned int)accepted_id[j]);printf("\n");
            stnc_core_mining_template_release(payload);return 0;
        case STNC_MINING_EXHAUSTED:
            {stnc_core_work_base_result base=stnc_core_check_work_base(work.parent_id,&checked);
            if(base!=STNC_CORE_WORK_BASE_CURRENT){
                stnc_core_mining_template_release(payload);
                fprintf(stderr,base==STNC_CORE_WORK_BASE_STALE?
                    "Mining pass exhausted after its work became stale.\n":
                    "Mining pass final work-base check failed.\n");
                return 1;
            }
            if(!checked.template_available||memcmp(checked.tip_id,work.parent_id,32u)!=0||
               memcmp(checked.target,block+120u,32u)!=0){
                stnc_core_mining_template_release(payload);
                fprintf(stderr,"Mining pass exhausted against changed Chain context.\n");return 1;
            }}
            printf("Mining pass complete\n  Attempts: %" PRIu64 "\n  Solution: none\n",attempts);
            stnc_core_mining_template_release(payload);return 0;
        default:
            stnc_core_mining_template_release(payload);fprintf(stderr,"Mining worker failed.\n");return 1;
        }}
    }
    if(strcmp(argv[2],"status")==0){
        stnc_mining_context context;
        if(stnc_core_mining_context(&context)!=0){fprintf(stderr,"Mining context unavailable.\n");return 1;}
        printf("Mining status\n  Height: %" PRIu64 "\n  Template: %s\n  Tip: ",
            context.height,context.template_available?"available":"unavailable");
        for(i=0u;i<sizeof(context.tip_id);i++)printf("%02x",(unsigned int)context.tip_id[i]);
        printf("\n  Target: ");
        for(i=0u;i<sizeof(context.target);i++)printf("%02x",(unsigned int)context.target[i]);
        printf("\n");return 0;
    }
    if(strcmp(argv[2],"check")==0){
        stnc_mining_context current,checked;
        stnc_core_work_base_result base;
        if(stnc_core_mining_context(&current)!=0){fprintf(stderr,"Mining context unavailable.\n");return 1;}
        base=stnc_core_check_work_base(current.tip_id,&checked);
        if(base==STNC_CORE_WORK_BASE_STALE){fprintf(stderr,"Mining work base is stale.\n");return 1;}
        if(base!=STNC_CORE_WORK_BASE_CURRENT){fprintf(stderr,"Mining work base check failed.\n");return 1;}
        printf("Mining work base\n  Status: current\n  Height: %" PRIu64 "\n  Template: %s\n",
            checked.height,checked.template_available?"available":"unavailable");
        return 0;
    }
    if(strcmp(argv[2],"template")==0){
        uint8_t *payload=NULL;size_t payload_length=0u;stnc_mining_template work;
        if(stnc_core_mining_template(&payload,&payload_length,&work)!=0){
            fprintf(stderr,"Mining template unavailable.\n");return 1;
        }
        printf("Mining template\n  Parent: ");
        for(i=0u;i<sizeof(work.parent_id);i++)printf("%02x",(unsigned int)work.parent_id[i]);
        printf("\n  Work ID: ");
        for(i=0u;i<sizeof(work.work_id);i++)printf("%02x",(unsigned int)work.work_id[i]);
        printf("\n  Block bytes: %zu\n  Envelope bytes: %zu\n",work.block_length,payload_length);
        stnc_core_mining_template_release(payload);return 0;
    }
    stnc_command_print_usage();return 1;
}

static int stnc_command_wallet(int argc,char **argv)
{
    stnc_wallet_key key;stnc_wallet_status status;char address[STNC_WALLET_ADDRESS_SIZE+1u];uint64_t units;
    memset(&key,0,sizeof(key));memset(&status,0,sizeof(status));
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
            stnc_wallet_clear(&key);
            if(stnc_wallet_status_read(&status)!=0){fprintf(stderr,"Wallet status failed.\n");return 1;}
            printf("Wallet status\n  Present: %s\n  Key integrity: %s\n",
                status.present?"yes":"no",status.key_valid?"valid":"invalid");
            if(status.key_valid)printf("  Address: %s\n",status.address);
            if(status.balance_available)printf("  Accepted balance: %" PRIu64 "\n",status.accepted_balance);
            else printf("  Accepted balance: unavailable\n");
            return status.present&&status.key_valid?0:1;
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
    if (strcmp(argv[1], "mining") == 0) return stnc_command_mining(argc, argv);
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
