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

#define STNC_STNC_METHOD_INFO 1u
#define STNC_STNC_METHOD_DERIVE_ADDRESS 9u

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

int stnc_stnc_encode_derive_address(
    uint16_t type,
    const uint8_t *source,
    size_t source_length,
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

void stnc_stnc_write_u16(
    uint8_t *buffer,
    uint16_t value
);

void stnc_stnc_write_u32(
    uint8_t *buffer,
    uint32_t value
);

#endif
