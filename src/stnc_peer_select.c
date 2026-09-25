#include <stddef.h>
#include <string.h>

#include "stnc_peer_select.h"

static int stnc_peer_select_valid(const stnc_peer_qualified *peer)
{
    return peer != NULL &&
           peer->candidate.host[0] != '\0' &&
           peer->candidate.port != 0u &&
           (peer->capabilities == 1u || peer->capabilities == 3u);
}

static int stnc_peer_select_better(
    const stnc_peer_qualified *left,
    const stnc_peer_qualified *right
)
{
    int host_order;

    if (left->latency_ms < right->latency_ms) {
        return 1;
    }

    if (left->latency_ms > right->latency_ms) {
        return 0;
    }

    host_order = strcmp(left->candidate.host, right->candidate.host);

    if (host_order < 0) {
        return 1;
    }

    if (host_order > 0) {
        return 0;
    }

    return left->candidate.port < right->candidate.port;
}

void stnc_peer_select_clear(stnc_peer_qualified_set *set)
{
    if (set != NULL) {
        memset(set, 0, sizeof(*set));
    }
}

int stnc_peer_select_add(
    stnc_peer_qualified_set *set,
    const stnc_peer_qualified *peer
)
{
    size_t index;

    if (set == NULL ||
        !stnc_peer_select_valid(peer) ||
        set->count > STNC_PEERS_MAX) {
        return 1;
    }

    for (index = 0; index < set->count; ++index) {
        if (strcmp(
                set->entries[index].candidate.host,
                peer->candidate.host
            ) == 0 &&
            set->entries[index].candidate.port == peer->candidate.port) {
            set->entries[index] = *peer;
            return 0;
        }
    }

    if (set->count == STNC_PEERS_MAX) {
        return 1;
    }

    set->entries[set->count++] = *peer;
    return 0;
}

const stnc_peer_qualified *stnc_peer_select_best(
    const stnc_peer_qualified_set *set
)
{
    const stnc_peer_qualified *best;
    size_t index;

    if (set == NULL || set->count == 0u || set->count > STNC_PEERS_MAX) {
        return NULL;
    }

    best = &set->entries[0];

    if (!stnc_peer_select_valid(best)) {
        return NULL;
    }

    for (index = 1; index < set->count; ++index) {
        if (!stnc_peer_select_valid(&set->entries[index])) {
            return NULL;
        }

        if (stnc_peer_select_better(&set->entries[index], best)) {
            best = &set->entries[index];
        }
    }

    return best;
}
