#ifndef STNC_PEER_SELECT_H
#define STNC_PEER_SELECT_H

#include <stddef.h>
#include <stdint.h>

#include "stnc_peers.h"

typedef struct stnc_peer_qualified {
    stnc_peer_candidate candidate;
    uint32_t capabilities;
    uint64_t latency_ms;
} stnc_peer_qualified;

typedef struct stnc_peer_qualified_set {
    stnc_peer_qualified entries[STNC_PEERS_MAX];
    size_t count;
} stnc_peer_qualified_set;

void stnc_peer_select_clear(stnc_peer_qualified_set *set);

int stnc_peer_select_add(
    stnc_peer_qualified_set *set,
    const stnc_peer_qualified *peer
);

const stnc_peer_qualified *stnc_peer_select_best(
    const stnc_peer_qualified_set *set
);

#endif
