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

static int stnc_stnp_encode_empty(
    uint16_t type,
    uint8_t *buffer,
    size_t capacity,
    size_t *written
)
{
    if (written != NULL) {
        *written = 0;
    }

    if (buffer == NULL ||
        written == NULL ||
        capacity < STNC_STNP_HEADER_SIZE) {
        return 1;
    }

    memcpy(buffer, "STNP", 4);
    stnc_stnp_write_u16(buffer + 4, STNC_STNP_VERSION);
    stnc_stnp_write_u16(buffer + 6, type);
    stnc_stnp_write_u32(buffer + 8, 0u);
    *written = STNC_STNP_HEADER_SIZE;
    return 0;
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

int stnc_stnp_encode_get_peers(
    uint8_t *buffer,
    size_t capacity,
    size_t *written
)
{
    return stnc_stnp_encode_empty(
        STNC_STNP_GET_PEERS,
        buffer,
        capacity,
        written
    );
}

int stnc_stnp_decode_peers_header(
    const uint8_t *buffer,
    size_t length,
    size_t *payload_length
)
{
    uint32_t payload;

    if (payload_length != NULL) {
        *payload_length = 0;
    }

    if (buffer == NULL ||
        payload_length == NULL ||
        length != STNC_STNP_HEADER_SIZE ||
        memcmp(buffer, "STNP", 4) != 0 ||
        stnc_stnp_read_u16(buffer + 4) != STNC_STNP_VERSION ||
        stnc_stnp_read_u16(buffer + 6) != STNC_STNP_PEERS) {
        return 1;
    }

    payload = stnc_stnp_read_u32(buffer + 8);

    if (payload < 2u ||
        payload > STNC_STNP_PEERS_PAYLOAD_MAX ||
        ((payload - 2u) % 6u) != 0u) {
        return 1;
    }

    *payload_length = (size_t)payload;
    return 0;
}

int stnc_stnp_decode_peers(
    const uint8_t *buffer,
    size_t length,
    stnc_stnp_peers *peers
)
{
    stnc_stnp_peers value;
    size_t payload_length;
    size_t count;
    size_t index;

    if (buffer == NULL ||
        peers == NULL ||
        length < STNC_STNP_HEADER_SIZE ||
        stnc_stnp_decode_peers_header(
            buffer,
            STNC_STNP_HEADER_SIZE,
            &payload_length
        ) != 0 ||
        length != STNC_STNP_HEADER_SIZE + payload_length) {
        return 1;
    }

    count = (size_t)stnc_stnp_read_u16(buffer + STNC_STNP_HEADER_SIZE);

    if (count > STNC_STNP_PEER_MAX ||
        payload_length != 2u + (count * 6u)) {
        return 1;
    }

    memset(&value, 0, sizeof(value));
    value.count = count;

    for (index = 0; index < count; ++index) {
        size_t offset = STNC_STNP_HEADER_SIZE + 2u + (index * 6u);

        memcpy(value.entries[index].address, buffer + offset, 4);
        value.entries[index].port = stnc_stnp_read_u16(buffer + offset + 4);
    }

    *peers = value;
    return 0;
}
