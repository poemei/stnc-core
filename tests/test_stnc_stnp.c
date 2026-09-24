#include <stdio.h>
#include <string.h>

#include "stnc_stnp.h"

int main(void)
{
    uint8_t network_id[32];
    uint8_t genesis_id[32];
    uint8_t frame[STNC_STNP_HEADER_SIZE + STNC_STNP_HELLO_SIZE];
    stnc_stnp_hello hello;
    size_t written;
    size_t index;

    for (index = 0; index < sizeof(network_id); ++index) {
        network_id[index] = (uint8_t)index;
        genesis_id[index] = (uint8_t)(0xffu - (unsigned int)index);
    }

    if (stnc_stnp_encode_hello(
            network_id,
            genesis_id,
            3u,
            frame,
            sizeof(frame),
            &written
        ) != 0 ||
        written != sizeof(frame)) {
        return 1;
    }

    memset(&hello, 0, sizeof(hello));

    if (stnc_stnp_decode_hello(frame, sizeof(frame), &hello) != 0 ||
        memcmp(hello.network_id, network_id, sizeof(network_id)) != 0 ||
        memcmp(hello.genesis_id, genesis_id, sizeof(genesis_id)) != 0 ||
        hello.capabilities != 3u) {
        return 1;
    }

    frame[0] = 'X';

    if (stnc_stnp_decode_hello(frame, sizeof(frame), &hello) == 0) {
        return 1;
    }

    printf("STNP codec tests passed.\n");
    return 0;
}
