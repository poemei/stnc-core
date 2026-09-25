#ifndef STNC_PEERS_H
#define STNC_PEERS_H

#include <stddef.h>
#include <stdint.h>

#include "stnc_stnp.h"

#define STNC_PEERS_MAX STNC_STNP_PEER_MAX
#define STNC_PEER_HOST_MAX 253u

typedef struct stnc_peer_candidate {
    char host[STNC_PEER_HOST_MAX + 1u];
    uint16_t port;
} stnc_peer_candidate;

typedef struct stnc_peer_candidates {
    stnc_peer_candidate entries[STNC_PEERS_MAX];
    size_t count;
} stnc_peer_candidates;

void stnc_peers_clear(stnc_peer_candidates *set);

int stnc_peers_add(
    stnc_peer_candidates *set,
    const stnc_peer_candidate *candidate
);

int stnc_peers_add_stnp(
    stnc_peer_candidates *set,
    const stnc_stnp_peers *peers
);

#endif
