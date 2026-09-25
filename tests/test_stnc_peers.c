#include <string.h>

#include "stnc_peers.h"

static stnc_peer_candidate candidate(
    unsigned int a,
    unsigned int b,
    unsigned int c,
    unsigned int d,
    unsigned int port
)
{
    stnc_peer_candidate value;

    memset(&value, 0, sizeof(value));
    value.address[0] = (uint8_t)a;
    value.address[1] = (uint8_t)b;
    value.address[2] = (uint8_t)c;
    value.address[3] = (uint8_t)d;
    value.port = (uint16_t)port;
    return value;
}

int main(void)
{
    stnc_peer_candidates set;
    stnc_peer_candidates before;
    stnc_peer_candidate first;
    stnc_peer_candidate second;
    stnc_peer_candidate invalid;
    stnc_stnp_peers discovered;

    stnc_peers_clear(&set);

    first = candidate(10u, 0u, 0u, 2u, 18474u);
    second = candidate(2u, 24u, 217u, 197u, 18474u);

    if (stnc_peers_add(&set, &first) != 0 ||
        stnc_peers_add(&set, &second) != 0 ||
        stnc_peers_add(&set, &first) != 0 ||
        set.count != 2u ||
        set.entries[0].address[0] != 2u ||
        set.entries[1].address[0] != 10u) {
        return 1;
    }

    invalid = candidate(0u, 0u, 0u, 0u, 18474u);
    before = set;

    if (stnc_peers_add(&set, &invalid) == 0 ||
        memcmp(&set, &before, sizeof(set)) != 0) {
        return 1;
    }

    memset(&discovered, 0, sizeof(discovered));
    discovered.count = 2u;
    discovered.entries[0].address[0] = 192u;
    discovered.entries[0].address[1] = 168u;
    discovered.entries[0].address[2] = 1u;
    discovered.entries[0].address[3] = 20u;
    discovered.entries[0].port = 18474u;
    discovered.entries[1].address[0] = 224u;
    discovered.entries[1].address[1] = 0u;
    discovered.entries[1].address[2] = 0u;
    discovered.entries[1].address[3] = 1u;
    discovered.entries[1].port = 18474u;

    before = set;

    if (stnc_peers_add_stnp(&set, &discovered) == 0 ||
        memcmp(&set, &before, sizeof(set)) != 0) {
        return 1;
    }

    discovered.count = 1u;

    if (stnc_peers_add_stnp(&set, &discovered) != 0 ||
        set.count != 3u) {
        return 1;
    }

    return 0;
}
