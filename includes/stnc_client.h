#ifndef STNC_CLIENT_H
#define STNC_CLIENT_H
#include "stnc_wallet_status.h"
#include "stnc_identity.h"
#include "stnc_core.h"
#include "stnc_config.h"
#include "stnc_background_mining.h"
#include "stnc_contract_status.h"
#include "stnc_contract_list.h"
#include "stnc_contract_draft.h"
#include "stnc_contract_create.h"
#include "stnc_log.h"

typedef struct stnc_client_snapshot {
    stnc_wallet_status wallet;
    stnc_identity_status identity;
    stnc_core_runtime_status network;
    stnc_core_chain_state chain;
    stnc_core_peer_status peer;
    stnc_mining_service_status mining;
    stnc_config config;
    int stratum_connected;
    int contracts_available;
    stnc_contract_list contracts;
    size_t activity_count;
    char activity[5][STNC_LOG_ENTRY_MAX];
} stnc_client_snapshot;

typedef enum stnc_client_operation {
    STNC_CLIENT_REFRESH=0,STNC_CLIENT_CREATE_WALLET,STNC_CLIENT_CREATE_IDENTITY,
    STNC_CLIENT_SEND,STNC_CLIENT_MINING_ON,STNC_CLIENT_MINING_OFF,
    STNC_CLIENT_CPU_LIMIT,STNC_CLIENT_CONTRACT_LOOKUP,STNC_CLIENT_SAVE_DRAFT,
    STNC_CLIENT_CREATE_CONTRACT
} stnc_client_operation;

typedef struct stnc_client_request {
    stnc_client_operation operation;
    int confirmed;
    char address[71];
    char units[32];
    unsigned int cpu_limit;
    char path[1024];
    stnc_contract_draft_input draft;
    uint8_t terms[65536];
} stnc_client_request;

/* Owner-thread display read. Accepted balance/contract reads are polled at
 * most every ten seconds; failed reads remain unavailable. Explicit actions,
 * connection changes and identity/endpoint changes invalidate the display cache.
 * This cache must never be used for transaction authorization. */
int stnc_client_read(stnc_client_snapshot *snapshot);
int stnc_client_execute(const stnc_client_request *request,char *result,size_t capacity);
int stnc_client_parse_units(const char *text,uint64_t *units);
/* Accept pasted STNC wallet text, trim surrounding ASCII whitespace and
 * normalize hexadecimal case. Generated/displayed addresses remain canonical
 * lowercase; normalization is a client input convenience only. */
int stnc_client_normalize_wallet_address(const char *address,char normalized[71]);
int stnc_client_wallet_address_valid(const char *address);
int stnc_client_set_mining(int enabled);
#endif
