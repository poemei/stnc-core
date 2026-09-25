#include <string.h>

#include "stnc_peer_select.h"

static stnc_peer_qualified peer(
    const char *host,
    unsigned int port,
    unsigned int capabilities,
    uint64_t latency
)
{
    stnc_peer_qualified value;
    size_t length;

    memset(&value, 0, sizeof(value));
    length = strlen(host);

    if (length < sizeof(value.candidate.host)) {
        memcpy(value.candidate.host, host, length + 1u);
    }

    value.candidate.port = (uint16_t)port;
    value.capabilities = (uint32_t)capabilities;
    value.latency_ms = latency;
    return value;
}

int main(void)
{
    stnc_peer_qualified_set set;
    stnc_peer_qualified a;
    stnc_peer_qualified b;
    stnc_peer_qualified c;
    stnc_peer_qualified bad;
    const stnc_peer_qualified *best;

    stnc_peer_select_clear(&set);

    a = peer("peer-b.example.org", 18474u, 3u, 20u);
    b = peer("peer-a.example.org", 18474u, 1u, 20u);
    c = peer("peer-c.example.org", 18474u, 3u, 10u);
    bad = peer("bad.example.org", 18474u, 2u, 1u);

    if (stnc_peer_select_add(&set, &a) != 0 ||
        stnc_peer_select_add(&set, &b) != 0 ||
        stnc_peer_select_add(&set, &c) != 0 ||
        stnc_peer_select_add(&set, &bad) == 0) {
        return 1;
    }

    best = stnc_peer_select_best(&set);

    if (best == NULL ||
        strcmp(best->candidate.host, "peer-c.example.org") != 0) {
        return 1;
    }

    c.latency_ms = 20u;

    if (stnc_peer_select_add(&set, &c) != 0) {
        return 1;
    }

    best = stnc_peer_select_best(&set);

    if (best == NULL ||
        strcmp(best->candidate.host, "peer-a.example.org") != 0) {
        return 1;
    }

    return 0;
}
