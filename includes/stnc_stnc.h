#ifndef STNC_STNC_H
#define STNC_STNC_H

#include <stddef.h>
#include <stdint.h>

#define STNC_STNC_HEADER_SIZE 24u
#define STNC_STNC_INFO_SIZE 184u
#define STNC_STNC_ADDRESS_IDENTITY_SIZE 69u
#define STNC_STNC_ADDRESS_TYPED_SIZE 70u
#define STNC_STNC_ADDRESS_MAX_SIZE STNC_STNC_ADDRESS_TYPED_SIZE
#define STNC_STNC_DERIVE_PREFIX_SIZE 6u
#define STNC_STNC_DERIVE_SOURCE_MAX 4096u
#define STNC_STNC_DERIVE_PAYLOAD_MAX (STNC_STNC_DERIVE_PREFIX_SIZE + STNC_STNC_DERIVE_SOURCE_MAX)
#define STNC_STNC_DERIVE_FRAME_MAX (STNC_STNC_HEADER_SIZE + STNC_STNC_DERIVE_PAYLOAD_MAX)

#define STNC_STNC_VERSION 2u

#define STNC_STNC_REQUEST 1u
#define STNC_STNC_RESPONSE 2u

#define STNC_STNC_OK 0u
#define STNC_STNC_CURRENT 13u

#define STNC_STNC_METHOD_INFO 1u
#define STNC_STNC_METHOD_BLOCK_HEIGHT 2u
#define STNC_STNC_METHOD_DERIVE_ADDRESS 9u
#define STNC_STNC_METHOD_BALANCE 10u
#define STNC_STNC_METHOD_CONTRACT_STATE 11u
#define STNC_STNC_METHOD_PENDING 0x1004u
#define STNC_STNC_METHOD_SUBMIT_TRANSACTION 0x1005u
#define STNC_STNC_METHOD_SUBMIT_BLOCK_EVIDENCE 0x1006u
#define STNC_STNC_METHOD_SUBMIT_HISTORY_EVIDENCE 0x1007u
#define STNC_STNC_METHOD_SUBMIT_SUFFIX_EVIDENCE 0x1008u
#define STNC_STNC_METHOD_SUFFIX_STAGE_BEGIN 0x1009u
#define STNC_STNC_METHOD_SUFFIX_STAGE_APPEND 0x100au
#define STNC_STNC_METHOD_SUFFIX_STAGE_COMMIT 0x100bu
#define STNC_STNC_METHOD_SUFFIX_STAGE_ABORT 0x100cu
#define STNC_STNC_METHOD_MINING_CONTEXT 0x2000u
#define STNC_STNC_METHOD_CHECK_WORK_BASE 0x2001u
#define STNC_STNC_METHOD_MINING_TEMPLATE 0x2002u
#define STNC_STNC_METHOD_SUBMIT_WORK 0x2003u
#define STNC_STNC_METHOD_SUBMIT_SHARE 0x2004u
#define STNC_STNC_MINING_CONTEXT_SIZE 76u
#define STNC_STNC_MINING_TEMPLATE_PREFIX_SIZE 68u
#define STNC_STNC_MINER_IDENTITY_SIZE 69u
#define STNC_STNC_MINING_SUBMISSION_PREFIX_SIZE 137u
#define STNC_STNC_MINING_NONCE_OFFSET 152u
#define STNC_STNC_MINING_NONCE_SIZE 8u
#define STNC_STNC_BLOCK_HEADER_SIZE 168u
#define STNC_STNC_BLOCK_MAX_SIZE 1070328u
#define STNC_STNC_BLOCK_ACCEPTED_SIZE 80u
#define STNC_STNC_PENDING_SIZE 16u
#define STNC_STNC_SUBMISSION_RESPONSE_SIZE 36u
#define STNC_STNC_TRANSACTION_MAX 66881u
#define STNC_STNC_SUBMISSION_ADMITTED 0u
#define STNC_STNC_SUBMISSION_DUPLICATE 1u
#define STNC_STNC_SUBMISSION_POOL_FULL 2u
#define STNC_STNC_SUBMISSION_BAD 3u
#define STNC_STNC_SUBMISSION_UNSUPPORTED 4u
#define STNC_STNC_SUBMISSION_REPLAY 5u
#define STNC_STNC_SUBMISSION_UNAUTHORIZED 6u
#define STNC_STNC_SUBMISSION_UNAVAILABLE 7u
#define STNC_STNC_SUBMISSION_INTERNAL 8u

#define STNC_STNC_BALANCE_SIZE 8u
#define STNC_STNC_CONTRACT_STATE_SIZE 26u
#define STNC_STNC_CONTRACT_STATE_DRAFT 1u
#define STNC_STNC_CONTRACT_STATE_CLOSED 9u
#define STNC_STNC_CONTRACT_TYPE_GENERIC 1u
#define STNC_STNC_CONTRACT_TYPE_SERVICE_AGREEMENT 6u
#define STNC_STNC_CONTRACT_MAX_PARTICIPANTS 32u
#define STNC_STNC_CONTRACT_MAX_TERMS 65536u

#define STNC_STNC_ADDRESS_IDENTITY 1u
#define STNC_STNC_ADDRESS_CONTRACT 2u
#define STNC_STNC_ADDRESS_WALLET 3u

typedef struct stnc_stnc_message {
    uint16_t kind;
    uint16_t method;
    uint16_t code;
    uint64_t request_id;
    const uint8_t *payload;
    size_t length;
} stnc_stnc_message;

typedef struct stnc_contract_state {
    uint16_t state;
    uint16_t type;
    uint64_t sequence;
    uint64_t created_at;
    uint16_t participant_count;
    uint32_t terms_length;
} stnc_contract_state;

typedef struct stnc_pending_state {
    uint32_t count;
    uint32_t max_entries;
    uint32_t bytes;
    uint32_t max_bytes;
} stnc_pending_state;

typedef struct stnc_submission_result {
    uint16_t result;
    uint8_t transaction_id[32];
    int has_transaction_id;
} stnc_submission_result;

typedef struct stnc_mining_template {
    uint8_t parent_id[32];
    uint8_t work_id[32];
    const uint8_t *block;
    size_t block_length;
} stnc_mining_template;

typedef struct stnc_mining_context {
    uint8_t tip_id[32];
    uint8_t target[32];
    uint64_t height;
    uint32_t template_available;
} stnc_mining_context;

typedef struct stnc_chain_info {
    uint8_t network_id[32];
    uint8_t genesis_id[32];
    uint64_t height;
    uint8_t tip_id[32];
    uint8_t cumulative_work[40];
    uint8_t current_target[32];
    uint32_t protocol_revision;
    uint32_t block_count;
} stnc_chain_info;

int stnc_stnc_encode(
    const stnc_stnc_message *message,
    uint8_t *buffer,
    size_t capacity,
    size_t *written
);

int stnc_stnc_encode_empty_request(
    uint16_t method,uint64_t request_id,uint8_t *buffer,size_t capacity,size_t *written
);

int stnc_stnc_encode_check_work_base(
    const uint8_t tip_id[32],uint64_t request_id,uint8_t *buffer,size_t capacity,size_t *written
);

int stnc_stnc_encode_submit_work(
    const uint8_t parent_id[32],const uint8_t work_id[32],const char *miner_identity,
    const uint8_t *block,size_t block_length,uint64_t request_id,
    uint8_t *buffer,size_t capacity,size_t *written
);

int stnc_stnc_decode_mining_template(
    const uint8_t *payload,size_t length,stnc_mining_template *work
);

int stnc_stnc_decode_mining_context(
    const uint8_t *payload,size_t length,stnc_mining_context *context
);

int stnc_stnc_encode_block_height(
    uint64_t height,uint64_t request_id,uint8_t *buffer,size_t capacity,size_t *written
);

int stnc_stnc_encode_derive_address(
    uint16_t type,
    const uint8_t *source,
    size_t source_length,
    uint64_t request_id,
    uint8_t *buffer,
    size_t capacity,
    size_t *written
);

int stnc_stnc_encode_address_query(
    uint16_t method,
    const char *address,
    uint64_t request_id,
    uint8_t *buffer,
    size_t capacity,
    size_t *written
);

int stnc_stnc_decode_header(
    const uint8_t *buffer,
    size_t length,
    stnc_stnc_message *message
);

int stnc_stnc_decode_info(
    const uint8_t *payload,
    size_t length,
    stnc_chain_info *info
);

int stnc_stnc_decode_address(
    uint16_t type,
    const uint8_t *payload,
    size_t length,
    char *address,
    size_t capacity
);

int stnc_stnc_decode_balance(
    const uint8_t *payload,
    size_t length,
    uint64_t *units
);

int stnc_stnc_encode_submit_transaction(
    const uint8_t *transaction,
    size_t transaction_length,
    uint64_t request_id,
    uint8_t *buffer,
    size_t capacity,
    size_t *written
);

int stnc_stnc_encode_submit_block_evidence(
    const uint8_t *block,
    size_t block_length,
    uint64_t request_id,
    uint8_t *buffer,
    size_t capacity,
    size_t *written
);

int stnc_stnc_encode_submit_history_evidence(
    const uint8_t *const *blocks,
    const size_t *block_lengths,
    size_t block_count,
    uint64_t request_id,
    uint8_t *buffer,
    size_t capacity,
    size_t *written
);

int stnc_stnc_encode_submit_suffix_evidence(
    uint32_t prefix_count,const uint8_t *const *blocks,const size_t *block_lengths,
    size_t block_count,uint64_t request_id,uint8_t *buffer,size_t capacity,size_t *written
);

int stnc_stnc_encode_suffix_stage_begin(
    uint32_t prefix_count,uint32_t suffix_count,uint64_t request_id,
    uint8_t *buffer,size_t capacity,size_t *written
);

int stnc_stnc_encode_suffix_stage_append(
    uint32_t start,const uint8_t *const *blocks,const size_t *block_lengths,
    size_t block_count,uint64_t request_id,uint8_t *buffer,size_t capacity,size_t *written
);

int stnc_stnc_encode_suffix_stage_control(
    uint16_t method,uint64_t request_id,uint8_t *buffer,size_t capacity,size_t *written
);

int stnc_stnc_decode_block_accepted(
    const uint8_t *payload,
    size_t length,
    uint8_t tip_id[32],
    uint64_t *height,
    uint8_t cumulative_work[40]
);

int stnc_stnc_decode_pending(
    const uint8_t *payload,
    size_t length,
    stnc_pending_state *state
);

int stnc_stnc_decode_submission(
    const uint8_t *payload,
    size_t length,
    stnc_submission_result *result
);

int stnc_stnc_decode_contract_state(
    const uint8_t *payload,
    size_t length,
    stnc_contract_state *state
);

void stnc_stnc_write_u16(
    uint8_t *buffer,
    uint16_t value
);

void stnc_stnc_write_u32(
    uint8_t *buffer,
    uint32_t value
);

#endif
