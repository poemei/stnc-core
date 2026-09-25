#ifndef STNC_DIRECTORY_H
#define STNC_DIRECTORY_H

#include <stddef.h>

#include "stnc_peers.h"

#define STNC_DIRECTORY_RESPONSE_MAX 16384u

int stnc_directory_decode(
    const char *json,
    size_t length,
    stnc_peer_candidates *candidates
);

#endif
