#ifndef STNC_CORE_H
#define STNC_CORE_H

#include <stddef.h>
#include <stdint.h>

#include "stnc_stnc.h"
#include "stnc_peers.h"

typedef enum stnc_core_state {
    STNC_CORE_STATE_UNINITIALIZED = 0,
    STNC_CORE_STATE_INITIALIZED,
    STNC_CORE_STATE_RUNNING,
    STNC_CORE_STATE_STOPPING,
    STNC_CORE_STATE_STOPPED
} stnc_core_state;

typedef struct stnc_core_runtime_status {
    int chain_connected;
    int chain_state_available;
    int p2p_connected;
    size_t candidate_count;
    size_t qualified_count;
    uint32_t root_peer_capabilities;
} stnc_core_runtime_status;

typedef struct stnc_core_peer_status {
    int connected;
    char host[STNC_PEER_HOST_MAX + 1u];
    uint16_t port;
    uint32_t capabilities;
    uint64_t latency_ms;
} stnc_core_peer_status;

typedef enum stnc_core_evidence_result {
    STNC_CORE_EVIDENCE_ERROR = 0,
    STNC_CORE_EVIDENCE_ADOPTED = 1,
    STNC_CORE_EVIDENCE_CURRENT = 2
} stnc_core_evidence_result;

typedef struct stnc_core_chain_state {
    int available;
    uint8_t network_id[32];
    uint8_t genesis_id[32];
    uint64_t height;
    uint8_t tip_id[32];
    uint8_t cumulative_work[40];
    uint8_t current_target[32];
    uint32_t protocol_revision;
    uint32_t block_count;
} stnc_core_chain_state;

int stnc_core_init(void);
int stnc_core_run(void);
void stnc_core_request_stop(void);
void stnc_core_shutdown(void);

stnc_core_state stnc_core_get_state(void);

const stnc_core_chain_state *
stnc_core_get_chain_state(void);
const stnc_core_peer_status *stnc_core_get_peer_status(void);
void stnc_core_get_runtime_status(stnc_core_runtime_status *status);

int stnc_core_derive_address(
    uint16_t type,
    const uint8_t *source,
    size_t source_length,
    char *address,
    size_t capacity
);

int stnc_core_balance(const char *wallet, uint64_t *units);
int stnc_core_contract_state(const char *contract, stnc_contract_state *state);
int stnc_core_pending(stnc_pending_state *state);
int stnc_core_mining_context(stnc_mining_context *context);
typedef enum stnc_core_work_base_result {
    STNC_CORE_WORK_BASE_ERROR=0,
    STNC_CORE_WORK_BASE_CURRENT=1,
    STNC_CORE_WORK_BASE_STALE=2
} stnc_core_work_base_result;
stnc_core_work_base_result stnc_core_check_work_base(const uint8_t tip_id[32],stnc_mining_context *context);
int stnc_core_mining_template(uint8_t **payload,size_t *payload_length,stnc_mining_template *work);
void stnc_core_mining_template_release(uint8_t *payload);
int stnc_core_submit_work(
    const uint8_t parent_id[32],const uint8_t work_id[32],const char *miner_identity,
    const uint8_t *block,size_t block_length,uint8_t block_id[32],uint64_t *height,
    uint8_t cumulative_work[40]
);
int stnc_core_submit_block_evidence(const uint8_t *block,size_t block_length);
int stnc_core_submit_history_evidence(
    const uint8_t *const *blocks,const size_t *block_lengths,size_t block_count);
stnc_core_evidence_result stnc_core_submit_suffix_evidence(
    uint32_t prefix_count,const uint8_t *const *blocks,const size_t *block_lengths,size_t block_count);
int stnc_core_submit_transaction(const uint8_t *transaction,size_t transaction_length,stnc_submission_result *result);

#endif
