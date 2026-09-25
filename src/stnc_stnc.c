#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "stnc_stnc.h"

void stnc_stnc_write_u16(uint8_t *buffer, uint16_t value)
{
    buffer[0] = (uint8_t)((value >> 8) & 0xffu);
    buffer[1] = (uint8_t)(value & 0xffu);
}

void stnc_stnc_write_u32(uint8_t *buffer, uint32_t value)
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
        buffer[index] = (uint8_t)((value >> ((7u - index) * 8u)) & UINT64_C(0xff));
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
    uint64_t value = 0;
    unsigned int index;
    for (index = 0; index < 8; index++) {
        value <<= 8;
        value |= (uint64_t)buffer[index];
    }
    return value;
}

static int stnc_address_type_valid(uint16_t type)
{
    return type == STNC_STNC_ADDRESS_IDENTITY ||
           type == STNC_STNC_ADDRESS_CONTRACT ||
           type == STNC_STNC_ADDRESS_WALLET;
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
        message->length > UINT32_MAX ||
        (message->length != 0 && message->payload == NULL) ||
        capacity < STNC_STNC_HEADER_SIZE ||
        message->length > capacity - STNC_STNC_HEADER_SIZE) {
        return 1;
    }

    memcpy(buffer, "STNC", 4);
    stnc_stnc_write_u16(buffer + 4, STNC_STNC_VERSION);
    stnc_stnc_write_u16(buffer + 6, message->kind);
    stnc_stnc_write_u16(buffer + 8, message->method);
    stnc_stnc_write_u16(buffer + 10, message->code);
    stnc_write_u64(buffer + 12, message->request_id);
    stnc_stnc_write_u32(buffer + 20, (uint32_t)message->length);
    if (message->length != 0) {
        memcpy(buffer + STNC_STNC_HEADER_SIZE, message->payload, message->length);
    }
    *written = STNC_STNC_HEADER_SIZE + message->length;
    return 0;
}

int stnc_stnc_encode_derive_address(
    uint16_t type,
    const uint8_t *source,
    size_t source_length,
    uint64_t request_id,
    uint8_t *buffer,
    size_t capacity,
    size_t *written
)
{
    uint8_t payload[STNC_STNC_DERIVE_PAYLOAD_MAX];
    stnc_stnc_message message;

    if (written != NULL) {
        *written = 0;
    }

    if (!stnc_address_type_valid(type) ||
        source == NULL ||
        source_length == 0 ||
        source_length > STNC_STNC_DERIVE_SOURCE_MAX) {
        return 1;
    }

    stnc_stnc_write_u16(payload, type);
    stnc_stnc_write_u32(payload + 2, (uint32_t)source_length);
    memcpy(payload + STNC_STNC_DERIVE_PREFIX_SIZE, source, source_length);

    memset(&message, 0, sizeof(message));
    message.kind = STNC_STNC_REQUEST;
    message.method = STNC_STNC_METHOD_DERIVE_ADDRESS;
    message.code = STNC_STNC_OK;
    message.request_id = request_id;
    message.payload = payload;
    message.length = STNC_STNC_DERIVE_PREFIX_SIZE + source_length;

    return stnc_stnc_encode(&message, buffer, capacity, written);
}

int stnc_stnc_decode_header(
    const uint8_t *buffer,
    size_t length,
    stnc_stnc_message *message
)
{
    if (buffer == NULL || message == NULL || length != STNC_STNC_HEADER_SIZE) {
        return 1;
    }
    if (memcmp(buffer, "STNC", 4) != 0 ||
        stnc_read_u16(buffer + 4) != STNC_STNC_VERSION) {
        return 1;
    }

    message->kind = stnc_read_u16(buffer + 6);
    message->method = stnc_read_u16(buffer + 8);
    message->code = stnc_read_u16(buffer + 10);
    message->request_id = stnc_read_u64(buffer + 12);
    message->length = (size_t)stnc_read_u32(buffer + 20);
    message->payload = NULL;

    return message->kind == STNC_STNC_RESPONSE ? 0 : 1;
}

int stnc_stnc_decode_info(
    const uint8_t *payload,
    size_t length,
    stnc_chain_info *info
)
{
    if (payload == NULL || info == NULL || length != STNC_STNC_INFO_SIZE) {
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

int stnc_stnc_decode_address(
    uint16_t type,
    const uint8_t *payload,
    size_t length,
    char *address,
    size_t capacity
)
{
    size_t expected;
    const char *prefix;
    size_t prefix_length;

    if (!stnc_address_type_valid(type) || payload == NULL || address == NULL) {
        return 1;
    }

    if (type == STNC_STNC_ADDRESS_IDENTITY) {
        expected = STNC_STNC_ADDRESS_IDENTITY_SIZE;
        prefix = "stn0_";
        prefix_length = 5u;
    } else if (type == STNC_STNC_ADDRESS_CONTRACT) {
        expected = STNC_STNC_ADDRESS_TYPED_SIZE;
        prefix = "stnc0_";
        prefix_length = 6u;
    } else {
        expected = STNC_STNC_ADDRESS_TYPED_SIZE;
        prefix = "stnw0_";
        prefix_length = 6u;
    }

    if (length != expected ||
        capacity <= length ||
        memcmp(payload, prefix, prefix_length) != 0) {
        return 1;
    }

    memcpy(address, payload, length);
    address[length] = '\0';
    return 0;
}
