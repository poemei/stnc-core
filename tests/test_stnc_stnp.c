#include <stdio.h>
#include <string.h>

#include "stnc_stnp.h"

static void write_u16(unsigned char *buffer, unsigned int value)
{
    buffer[0] = (unsigned char)((value >> 8) & 0xffu);
    buffer[1] = (unsigned char)(value & 0xffu);
}

static void write_u32(unsigned char *buffer, unsigned long value)
{
    buffer[0] = (unsigned char)((value >> 24) & 0xffu);
    buffer[1] = (unsigned char)((value >> 16) & 0xffu);
    buffer[2] = (unsigned char)((value >> 8) & 0xffu);
    buffer[3] = (unsigned char)(value & 0xffu);
}

static void write_u64(unsigned char *buffer, unsigned long long value)
{
    buffer[0] = (unsigned char)((value >> 56) & 0xffu);
    buffer[1] = (unsigned char)((value >> 48) & 0xffu);
    buffer[2] = (unsigned char)((value >> 40) & 0xffu);
    buffer[3] = (unsigned char)((value >> 32) & 0xffu);
    buffer[4] = (unsigned char)((value >> 24) & 0xffu);
    buffer[5] = (unsigned char)((value >> 16) & 0xffu);
    buffer[6] = (unsigned char)((value >> 8) & 0xffu);
    buffer[7] = (unsigned char)(value & 0xffu);
}

int main(void)
{
    unsigned char network_id[32];
    unsigned char genesis_id[32];
    unsigned char hello_frame[STNC_STNP_HEADER_SIZE + STNC_STNP_HELLO_SIZE];
    unsigned char state_request[STNC_STNP_HEADER_SIZE];
    unsigned char state_frame[STNC_STNP_HEADER_SIZE + STNC_STNP_STATE_SIZE];
    unsigned char get_headers[STNC_STNP_HEADER_SIZE + 8u];
    unsigned char headers_frame[STNC_STNP_HEADER_SIZE + 8u + STNC_STNP_HEADER_WIRE_SIZE];
    unsigned char get_block[STNC_STNP_HEADER_SIZE + STNC_STNP_BLOCK_INDEX_SIZE];
    unsigned char block_frame[STNC_STNP_HEADER_SIZE + STNC_STNP_BLOCK_INDEX_SIZE + STNC_STNP_HEADER_WIRE_SIZE];
    unsigned char get_peers[STNC_STNP_HEADER_SIZE];
    unsigned char peers_frame[STNC_STNP_HEADER_SIZE + 14u];
    stnc_stnp_hello hello;
    stnc_stnp_state state;
    stnc_stnp_peers peers;
    size_t written;
    size_t payload_length;
    uint32_t header_start;
    uint32_t header_count;
    uint32_t block_index;
    const uint8_t *block_bytes;
    size_t block_length;
    size_t index;

    for (index = 0; index < sizeof(network_id); ++index) {
        network_id[index] = (unsigned char)index;
        genesis_id[index] = (unsigned char)(0xffu - (unsigned int)index);
    }

    if (stnc_stnp_encode_hello(
            network_id,
            genesis_id,
            3u,
            hello_frame,
            sizeof(hello_frame),
            &written
        ) != 0 ||
        written != sizeof(hello_frame) ||
        stnc_stnp_decode_hello(hello_frame, sizeof(hello_frame), &hello) != 0 ||
        memcmp(hello.network_id, network_id, sizeof(network_id)) != 0 ||
        memcmp(hello.genesis_id, genesis_id, sizeof(genesis_id)) != 0 ||
        hello.capabilities != 3u) {
        return 1;
    }

    hello_frame[0] = 'X';
    if (stnc_stnp_decode_hello(hello_frame, sizeof(hello_frame), &hello) == 0) {
        return 1;
    }

    if (stnc_stnp_encode_state(
            state_request,
            sizeof(state_request),
            &written
        ) != 0 ||
        written != sizeof(state_request) ||
        memcmp(state_request, "STNP", 4) != 0 ||
        state_request[6] != 0u ||
        state_request[7] != STNC_STNP_STATE) {
        return 1;
    }

    memset(state_frame, 0, sizeof(state_frame));
    memcpy(state_frame, "STNP", 4);
    write_u16(state_frame + 4, STNC_STNP_VERSION);
    write_u16(state_frame + 6, STNC_STNP_STATE);
    write_u32(state_frame + 8, STNC_STNP_STATE_SIZE);
    write_u64(state_frame + 12, 251u);

    for (index = 0; index < 32u; ++index) {
        state_frame[20u + index] = (unsigned char)(0x80u + index);
    }
    for (index = 0; index < 40u; ++index) {
        state_frame[52u + index] = (unsigned char)(0x40u + index);
    }
    write_u32(state_frame + 92, 252u);

    if (stnc_stnp_decode_state(state_frame, sizeof(state_frame), &state) != 0 ||
        state.height != 251u ||
        state.block_count != 252u ||
        memcmp(state.tip_id, state_frame + 20, 32) != 0 ||
        memcmp(state.cumulative_work, state_frame + 52, 40) != 0) {
        return 1;
    }

    write_u32(state_frame + 92, 251u);
    if (stnc_stnp_decode_state(state_frame, sizeof(state_frame), &state) == 0) {
        return 1;
    }
    write_u32(state_frame + 92, 252u);

    if (stnc_stnp_encode_get_headers(
            252u,
            1u,
            get_headers,
            sizeof(get_headers),
            &written
        ) != 0 ||
        written != sizeof(get_headers) ||
        memcmp(get_headers, "STNP", 4) != 0 ||
        get_headers[6] != 0u ||
        get_headers[7] != STNC_STNP_GET_HEADERS) {
        return 1;
    }

    if (stnc_stnp_encode_get_headers(
            252u,
            0u,
            get_headers,
            sizeof(get_headers),
            &written
        ) == 0) {
        return 1;
    }

    memset(headers_frame, 0, sizeof(headers_frame));
    memcpy(headers_frame, "STNP", 4);
    write_u16(headers_frame + 4, STNC_STNP_VERSION);
    write_u16(headers_frame + 6, STNC_STNP_HEADERS);
    write_u32(headers_frame + 8, 8u + STNC_STNP_HEADER_WIRE_SIZE);
    write_u32(headers_frame + 12, 252u);
    write_u32(headers_frame + 16, 1u);

    if (stnc_stnp_decode_headers_header(
            headers_frame,
            STNC_STNP_HEADER_SIZE,
            &payload_length
        ) != 0 ||
        payload_length != 8u + STNC_STNP_HEADER_WIRE_SIZE ||
        stnc_stnp_decode_headers(
            headers_frame,
            sizeof(headers_frame),
            &header_start,
            &header_count
        ) != 0 ||
        header_start != 252u ||
        header_count != 1u) {
        return 1;
    }

    write_u32(headers_frame + 16, 2u);
    if (stnc_stnp_decode_headers(
            headers_frame,
            sizeof(headers_frame),
            &header_start,
            &header_count
        ) == 0) {
        return 1;
    }

    if (stnc_stnp_encode_get_block(
            252u,
            get_block,
            sizeof(get_block),
            &written
        ) != 0 ||
        written != sizeof(get_block) ||
        memcmp(get_block, "STNP", 4) != 0 ||
        get_block[6] != 0u ||
        get_block[7] != STNC_STNP_GET_BLOCK) {
        return 1;
    }

    memset(block_frame, 0, sizeof(block_frame));
    memcpy(block_frame, "STNP", 4);
    write_u16(block_frame + 4, STNC_STNP_VERSION);
    write_u16(block_frame + 6, STNC_STNP_BLOCK);
    write_u32(block_frame + 8, STNC_STNP_BLOCK_INDEX_SIZE + STNC_STNP_HEADER_WIRE_SIZE);
    write_u32(block_frame + 12, 252u);
    for (index = 0; index < STNC_STNP_HEADER_WIRE_SIZE; ++index) {
        block_frame[16u + index] = (unsigned char)(index & 0xffu);
    }

    if (stnc_stnp_decode_block_header(
            block_frame,
            STNC_STNP_HEADER_SIZE,
            &payload_length
        ) != 0 ||
        payload_length != STNC_STNP_BLOCK_INDEX_SIZE + STNC_STNP_HEADER_WIRE_SIZE ||
        stnc_stnp_decode_block(
            block_frame,
            sizeof(block_frame),
            &block_index,
            &block_bytes,
            &block_length
        ) != 0 ||
        block_index != 252u ||
        block_length != STNC_STNP_HEADER_WIRE_SIZE ||
        block_bytes[0] != 0u ||
        block_bytes[167] != 167u) {
        return 1;
    }

    if (stnc_stnp_encode_get_peers(
            get_peers,
            sizeof(get_peers),
            &written
        ) != 0 ||
        written != sizeof(get_peers) ||
        memcmp(get_peers, "STNP", 4) != 0 ||
        get_peers[6] != 0u ||
        get_peers[7] != STNC_STNP_GET_PEERS) {
        return 1;
    }

    memset(peers_frame, 0, sizeof(peers_frame));
    memcpy(peers_frame, "STNP", 4);
    write_u16(peers_frame + 4, STNC_STNP_VERSION);
    write_u16(peers_frame + 6, STNC_STNP_PEERS);
    write_u32(peers_frame + 8, 14u);
    write_u16(peers_frame + 12, 2u);

    peers_frame[14] = 2u;
    peers_frame[15] = 24u;
    peers_frame[16] = 217u;
    peers_frame[17] = 197u;
    write_u16(peers_frame + 18, 18474u);

    peers_frame[20] = 10u;
    peers_frame[21] = 0u;
    peers_frame[22] = 0u;
    peers_frame[23] = 2u;
    write_u16(peers_frame + 24, 18474u);

    if (stnc_stnp_decode_peers_header(
            peers_frame,
            STNC_STNP_HEADER_SIZE,
            &payload_length
        ) != 0 ||
        payload_length != 14u ||
        stnc_stnp_decode_peers(peers_frame, sizeof(peers_frame), &peers) != 0 ||
        peers.count != 2u ||
        peers.entries[0].address[0] != 2u ||
        peers.entries[0].address[1] != 24u ||
        peers.entries[0].address[2] != 217u ||
        peers.entries[0].address[3] != 197u ||
        peers.entries[0].port != 18474u ||
        peers.entries[1].address[0] != 10u ||
        peers.entries[1].address[1] != 0u ||
        peers.entries[1].address[2] != 0u ||
        peers.entries[1].address[3] != 2u ||
        peers.entries[1].port != 18474u) {
        return 1;
    }

    peers_frame[13] = 3u;
    if (stnc_stnp_decode_peers(peers_frame, sizeof(peers_frame), &peers) == 0) {
        return 1;
    }

    printf("STNP codec tests passed.\n");
    return 0;
}
