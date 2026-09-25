#include <string.h>

#include "stnc_peers.h"

static stnc_peer_candidate candidate(const char *host, unsigned int port)
{
    stnc_peer_candidate value;

    memset(&value, 0, sizeof(value));

    if (host != NULL) {
        size_t length;

        length = strlen(host);

        if (length < sizeof(value.host)) {
            memcpy(value.host, host, length + 1u);
        }
    }

    value.port = (uint16_t)port;
    return value;
}

int main(void)
{
    stnc_peer_candidates set;
    stnc_peer_candidates before;
    stnc_peer_candidate first;
    stnc_peer_candidate second;
    stnc_peer_candidate ipv6;
    stnc_peer_candidate invalid;
    stnc_stnp_peers discovered;

    stnc_peers_clear(&set);

    first = candidate("peer-b.example.org", 18474u);
    second = candidate("2.24.217.197", 18474u);
    ipv6 = candidate("2001:db8::1", 18474u);

    if (stnc_peers_add(&set, &first) != 0 ||
        stnc_peers_add(&set, &second) != 0 ||
        stnc_peers_add(&set, &ipv6) != 0 ||
        stnc_peers_add(&set, &first) != 0 ||
        set.count != 3u ||
        strcmp(set.entries[0].host, "2.24.217.197") != 0 ||
        strcmp(set.entries[1].host, "2001:db8::1") != 0 ||
        strcmp(set.entries[2].host, "peer-b.example.org") != 0) {
        return 1;
    }

    invalid = candidate("https://bad.example.org", 18474u);
    before = set;

    if (stnc_peers_add(&set, &invalid) == 0 ||
        memcmp(&set, &before, sizeof(set)) != 0) {
        return 1;
    }

    memset(&discovered, 0, sizeof(discovered));
    discovered.count = 1u;
    discovered.entries[0].address[0] = 10u;
    discovered.entries[0].address[1] = 0u;
    discovered.entries[0].address[2] = 0u;
    discovered.entries[0].address[3] = 2u;
    discovered.entries[0].port = 18474u;

    if (stnc_peers_add_stnp(&set, &discovered) != 0 ||
        set.count != 4u ||
        strcmp(set.entries[0].host, "10.0.0.2") != 0 ||
        strcmp(set.entries[1].host, "2.24.217.197") != 0 ||
        strcmp(set.entries[2].host, "2001:db8::1") != 0 ||
        strcmp(set.entries[3].host, "peer-b.example.org") != 0) {
        return 1;
    }

    return 0;
}
