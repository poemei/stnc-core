#include <string.h>

#include "stnc_directory.h"

int main(void)
{
    static const char valid[] =
        "{\"version\":1,\"network\":\"stn-chain\",\"protocol\":\"stnp\","
        "\"authority\":false,\"count\":2,\"peers\":["
        "{\"host\":\"peer-b.example.org\",\"port\":18474},"
        "{\"host\":\"2.24.217.197\",\"port\":18474}]}";
    static const char invalid_authority[] =
        "{\"version\":1,\"network\":\"stn-chain\",\"protocol\":\"stnp\","
        "\"authority\":true,\"count\":0,\"peers\":[]}";
    static const char invalid_count[] =
        "{\"version\":1,\"network\":\"stn-chain\",\"protocol\":\"stnp\","
        "\"authority\":false,\"count\":2,\"peers\":[]}";
    stnc_peer_candidates candidates;
    stnc_peer_candidates before;

    stnc_peers_clear(&candidates);

    if (stnc_directory_decode(valid, strlen(valid), &candidates) != 0 ||
        candidates.count != 2u ||
        strcmp(candidates.entries[0].host, "2.24.217.197") != 0 ||
        strcmp(candidates.entries[1].host, "peer-b.example.org") != 0) {
        return 1;
    }

    before = candidates;

    if (stnc_directory_decode(
            invalid_authority,
            strlen(invalid_authority),
            &candidates
        ) == 0 ||
        memcmp(&before, &candidates, sizeof(candidates)) != 0) {
        return 1;
    }

    if (stnc_directory_decode(
            invalid_count,
            strlen(invalid_count),
            &candidates
        ) == 0 ||
        memcmp(&before, &candidates, sizeof(candidates)) != 0) {
        return 1;
    }

    return 0;
}
