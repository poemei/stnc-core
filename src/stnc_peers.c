#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "stnc_peers.h"

static int stnc_peers_compare(
    const stnc_peer_candidate *left,
    const stnc_peer_candidate *right
)
{
    int address_order;

    address_order = memcmp(left->address, right->address, 4);

    if (address_order != 0) {
        return address_order;
    }

    if (left->port < right->port) {
        return -1;
    }

    if (left->port > right->port) {
        return 1;
    }

    return 0;
}

static int stnc_peers_valid(const stnc_peer_candidate *candidate)
{
    if (candidate == NULL ||
        candidate->port == 0 ||
        candidate->address[0] == 0 ||
        candidate->address[0] >= 224u) {
        return 0;
    }

    return 1;
}

void stnc_peers_clear(stnc_peer_candidates *set)
{
    if (set != NULL) {
        memset(set, 0, sizeof(*set));
    }
}

int stnc_peers_add(
    stnc_peer_candidates *set,
    const stnc_peer_candidate *candidate
)
{
    size_t position;
    size_t index;

    if (set == NULL ||
        !stnc_peers_valid(candidate) ||
        set->count > STNC_PEERS_MAX) {
        return 1;
    }

    for (index = 0; index < set->count; ++index) {
        if (!stnc_peers_valid(&set->entries[index]) ||
            (index != 0 &&
             stnc_peers_compare(
                 &set->entries[index - 1],
                 &set->entries[index]
             ) >= 0)) {
            return 1;
        }
    }

    for (position = 0; position < set->count; ++position) {
        int order;

        order = stnc_peers_compare(candidate, &set->entries[position]);

        if (order == 0) {
            return 0;
        }

        if (order < 0) {
            break;
        }
    }

    if (set->count == STNC_PEERS_MAX) {
        return 1;
    }

    for (index = set->count; index > position; --index) {
        set->entries[index] = set->entries[index - 1];
    }

    set->entries[position] = *candidate;
    ++set->count;
    return 0;
}

int stnc_peers_add_stnp(
    stnc_peer_candidates *set,
    const stnc_stnp_peers *peers
)
{
    stnc_peer_candidates staged;
    size_t index;

    if (set == NULL ||
        peers == NULL ||
        peers->count > STNC_STNP_PEER_MAX) {
        return 1;
    }

    staged = *set;

    for (index = 0; index < peers->count; ++index) {
        stnc_peer_candidate candidate;

        memcpy(
            candidate.address,
            peers->entries[index].address,
            sizeof(candidate.address)
        );
        candidate.port = peers->entries[index].port;

        if (stnc_peers_add(&staged, &candidate) != 0) {
            return 1;
        }
    }

    *set = staged;
    return 0;
}
