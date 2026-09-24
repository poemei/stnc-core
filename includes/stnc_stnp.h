#ifndef STNC_STNP_H
#define STNC_STNP_H

#include <stddef.h>
#include <stdint.h>

#define STNC_STNP_HEADER_SIZE 12u
#define STNC_STNP_HELLO_SIZE 68u
#define STNC_STNP_VERSION 2u
#define STNC_STNP_HELLO 1u

typedef struct stnc_stnp_hello {
    uint8_t network_id[32];
    uint8_t genesis_id[32];
    uint32_t capabilities;
} stnc_stnp_hello;

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

#endif
