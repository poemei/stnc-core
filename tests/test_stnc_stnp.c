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

int main(void)
{
    unsigned char network_id[32];
    unsigned char genesis_id[32];
    unsigned char hello_frame[STNC_STNP_HEADER_SIZE + STNC_STNP_HELLO_SIZE];
    unsigned char get_peers[STNC_STNP_HEADER_SIZE];
    unsigned char peers_frame[STNC_STNP_HEADER_SIZE + 14u];
    stnc_stnp_hello hello;
    stnc_stnp_peers peers;
    size_t written;
    size_t payload_length;
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
