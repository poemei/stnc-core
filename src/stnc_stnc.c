#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "stnc_stnc.h"

void stnc_stnc_stnc_write_u16(uint8_t *buffer, uint16_t value)
{
    buffer[0] = (uint8_t)((value >> 8) & 0xffu);
    buffer[1] = (uint8_t)(value & 0xffu);
}

void stnc_stnc_stnc_write_u32(uint8_t *buffer, uint32_t value)
{
    buffer[0] = (uint8_t)((value >> 24) & 0xffu);
    buffer[1] = (uint8_t)((value >> 16) & 0xffu);
    buffer[2] = (uint8_t)((value >> 8) & 0xffu);
    buffer[3] = (uint8_t)(value & 0xffu);
}

static void stnc_write_u64(uint8_t *buffer, uint64_t value)
{
    unsigned int index;

    for (index = 0; index < 8; index++) {
        buffer[index] = (uint8_t)(
            (value >> ((7u - index) * 8u)) & UINT64_C(0xff)
        );
    }
}

static uint16_t stnc_read_u16(const uint8_t *buffer)
{
    return (uint16_t)(((uint16_t)buffer[0] << 8) | (uint16_t)buffer[1]);
}

static uint32_t stnc_read_u32(const uint8_t *buffer)
{
    return ((uint32_t)buffer[0] << 24) |
           ((uint32_t)buffer[1] << 16) |
           ((uint32_t)buffer[2] << 8) |
           (uint32_t)buffer[3];
}

static uint64_t stnc_read_u64(const uint8_t *buffer)
{
    uint64_t value;
    unsigned int index;

    value = 0;

    for (index = 0; index < 8; index++) {
        value <<= 8;
        value |= (uint64_t)buffer[index];
    }

    return value;
}

int stnc_stnc_encode(
    const stnc_stnc_message *message,
    uint8_t *buffer,
    size_t capacity,
    size_t *written
)
{
    if (written != NULL) {
        *written = 0;
    }

    if (message == NULL ||
        buffer == NULL ||
        written == NULL ||
        message->kind != STNC_STNC_REQUEST ||
        message->code != STNC_STNC_OK ||
        message->length != 0 ||
        message->payload != NULL ||
        capacity < STNC_STNC_HEADER_SIZE) {
        return 1;
    }

    memcpy(buffer, "STNC", 4);
    stnc_stnc_write_u16(buffer + 4, STNC_STNC_VERSION);
    stnc_stnc_write_u16(buffer + 6, message->kind);
    stnc_stnc_write_u16(buffer + 8, message->method);
    stnc_stnc_write_u16(buffer + 10, message->code);
    stnc_write_u64(buffer + 12, message->request_id);
    stnc_stnc_write_u32(buffer + 20, 0);

    *written = STNC_STNC_HEADER_SIZE;

    return 0;
}

int stnc_stnc_decode_header(
    const uint8_t *buffer,
    size_t length,
    stnc_stnc_message *message
)
{
    if (buffer == NULL ||
        message == NULL ||
        length != STNC_STNC_HEADER_SIZE) {
        return 1;
    }

    if (memcmp(buffer, "STNC", 4) != 0) {
        return 1;
    }

    if (stnc_read_u16(buffer + 4) != STNC_STNC_VERSION) {
        return 1;
    }

    message->kind = stnc_read_u16(buffer + 6);
    message->method = stnc_read_u16(buffer + 8);
    message->code = stnc_read_u16(buffer + 10);
    message->request_id = stnc_read_u64(buffer + 12);
    message->length = (size_t)stnc_read_u32(buffer + 20);
    message->payload = NULL;

    if (message->kind != STNC_STNC_RESPONSE) {
        return 1;
    }

    return 0;
}

int stnc_stnc_decode_info(
    const uint8_t *payload,
    size_t length,
    stnc_chain_info *info
)
{
    if (payload == NULL ||
        info == NULL ||
        length != STNC_STNC_INFO_SIZE) {
        return 1;
    }

    memset(info, 0, sizeof(*info));

    memcpy(info->network_id, payload, 32);
    memcpy(info->genesis_id, payload + 32, 32);
    info->height = stnc_read_u64(payload + 64);
    memcpy(info->tip_id, payload + 72, 32);
    memcpy(info->cumulative_work, payload + 104, 40);
    memcpy(info->current_target, payload + 144, 32);
    info->protocol_revision = stnc_read_u32(payload + 176);
    info->block_count = stnc_read_u32(payload + 180);

    return 0;
}
