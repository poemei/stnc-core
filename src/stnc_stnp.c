#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "stnc_stnp.h"

static void stnc_stnp_write_u16(uint8_t *buffer, uint16_t value)
{
    buffer[0] = (uint8_t)((value >> 8) & 0xffu);
    buffer[1] = (uint8_t)(value & 0xffu);
}

static void stnc_stnp_write_u32(uint8_t *buffer, uint32_t value)
{
    buffer[0] = (uint8_t)((value >> 24) & 0xffu);
    buffer[1] = (uint8_t)((value >> 16) & 0xffu);
    buffer[2] = (uint8_t)((value >> 8) & 0xffu);
    buffer[3] = (uint8_t)(value & 0xffu);
}

static uint16_t stnc_stnp_read_u16(const uint8_t *buffer)
{
    return (uint16_t)(((uint16_t)buffer[0] << 8) | (uint16_t)buffer[1]);
}

static uint32_t stnc_stnp_read_u32(const uint8_t *buffer)
{
    return ((uint32_t)buffer[0] << 24) |
           ((uint32_t)buffer[1] << 16) |
           ((uint32_t)buffer[2] << 8) |
           (uint32_t)buffer[3];
}

int stnc_stnp_encode_hello(
    const uint8_t network_id[32],
    const uint8_t genesis_id[32],
    uint32_t capabilities,
    uint8_t *buffer,
    size_t capacity,
    size_t *written
)
{
    if (written != NULL) {
        *written = 0;
    }

    if (network_id == NULL ||
        genesis_id == NULL ||
        buffer == NULL ||
        written == NULL ||
        capacity < STNC_STNP_HEADER_SIZE + STNC_STNP_HELLO_SIZE) {
        return 1;
    }

    memcpy(buffer, "STNP", 4);
    stnc_stnp_write_u16(buffer + 4, STNC_STNP_VERSION);
    stnc_stnp_write_u16(buffer + 6, STNC_STNP_HELLO);
    stnc_stnp_write_u32(buffer + 8, STNC_STNP_HELLO_SIZE);
    memcpy(buffer + STNC_STNP_HEADER_SIZE, network_id, 32);
    memcpy(buffer + STNC_STNP_HEADER_SIZE + 32, genesis_id, 32);
    stnc_stnp_write_u32(buffer + STNC_STNP_HEADER_SIZE + 64, capabilities);

    *written = STNC_STNP_HEADER_SIZE + STNC_STNP_HELLO_SIZE;
    return 0;
}

int stnc_stnp_decode_hello(
    const uint8_t *buffer,
    size_t length,
    stnc_stnp_hello *hello
)
{
    stnc_stnp_hello value;

    if (buffer == NULL ||
        hello == NULL ||
        length != STNC_STNP_HEADER_SIZE + STNC_STNP_HELLO_SIZE ||
        memcmp(buffer, "STNP", 4) != 0 ||
        stnc_stnp_read_u16(buffer + 4) != STNC_STNP_VERSION ||
        stnc_stnp_read_u16(buffer + 6) != STNC_STNP_HELLO ||
        stnc_stnp_read_u32(buffer + 8) != STNC_STNP_HELLO_SIZE) {
        return 1;
    }

    memcpy(value.network_id, buffer + STNC_STNP_HEADER_SIZE, 32);
    memcpy(value.genesis_id, buffer + STNC_STNP_HEADER_SIZE + 32, 32);
    value.capabilities = stnc_stnp_read_u32(buffer + STNC_STNP_HEADER_SIZE + 64);

    *hello = value;
    return 0;
}
