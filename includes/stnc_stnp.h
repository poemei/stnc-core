#ifndef STNC_STNP_H
#define STNC_STNP_H

#include <stddef.h>
#include <stdint.h>

#define STNC_STNP_HEADER_SIZE 12u
#define STNC_STNP_HELLO_SIZE 68u
#define STNC_STNP_STATE_SIZE 84u
#define STNC_STNP_VERSION 2u
#define STNC_STNP_HELLO 1u
#define STNC_STNP_STATE 2u
#define STNC_STNP_GET_PEERS 7u
#define STNC_STNP_PEERS 8u
#define STNC_STNP_PEER_MAX 64u
#define STNC_STNP_PEERS_PAYLOAD_MAX (2u + (6u * STNC_STNP_PEER_MAX))
#define STNC_STNP_PEERS_FRAME_MAX (STNC_STNP_HEADER_SIZE + STNC_STNP_PEERS_PAYLOAD_MAX)

typedef struct stnc_stnp_hello {
    uint8_t network_id[32];
    uint8_t genesis_id[32];
    uint32_t capabilities;
} stnc_stnp_hello;

typedef struct stnc_stnp_state {
    uint64_t height;
    uint8_t tip_id[32];
    uint8_t cumulative_work[40];
    uint32_t block_count;
} stnc_stnp_state;

typedef struct stnc_stnp_peer {
    uint8_t address[4];
    uint16_t port;
} stnc_stnp_peer;

typedef struct stnc_stnp_peers {
    stnc_stnp_peer entries[STNC_STNP_PEER_MAX];
    size_t count;
} stnc_stnp_peers;

int stnc_stnp_encode_hello(
    const uint8_t network_id[32],
    const uint8_t genesis_id[32],
    uint32_t capabilities,
    uint8_t *buffer,
    size_t capacity,
    size_t *written
);

int stnc_stnp_decode_hello(
    const uint8_t *buffer,
    size_t length,
    stnc_stnp_hello *hello
);

int stnc_stnp_encode_state(
    uint8_t *buffer,
    size_t capacity,
    size_t *written
);

int stnc_stnp_decode_state(
    const uint8_t *buffer,
    size_t length,
    stnc_stnp_state *state
);

int stnc_stnp_encode_get_peers(
    uint8_t *buffer,
    size_t capacity,
    size_t *written
);

int stnc_stnp_decode_peers_header(
    const uint8_t *buffer,
    size_t length,
    size_t *payload_length
);

int stnc_stnp_decode_peers(
    const uint8_t *buffer,
    size_t length,
    stnc_stnp_peers *peers
);

#endif
