#include <stdio.h>
#include <string.h>

#include "stnc_stnc.h"

static int check_type(uint16_t type, const char *prefix, size_t address_length)
{
    uint8_t frame[STNC_STNC_DERIVE_FRAME_MAX];
    uint8_t address[STNC_STNC_ADDRESS_MAX_SIZE];
    char decoded[STNC_STNC_ADDRESS_MAX_SIZE + 1u];
    const char source[] = "canonical-source";
    size_t written;
    size_t index;

    if (stnc_stnc_encode_derive_address(
            type,
            (const uint8_t *)source,
            strlen(source),
            UINT64_C(7),
            frame,
            sizeof(frame),
            &written
        ) != 0 ||
        written != STNC_STNC_HEADER_SIZE + STNC_STNC_DERIVE_PREFIX_SIZE + strlen(source) ||
        memcmp(frame, "STNC", 4) != 0 ||
        frame[8] != 0u ||
        frame[9] != STNC_STNC_METHOD_DERIVE_ADDRESS ||
        frame[24] != 0u ||
        frame[25] != (uint8_t)type) {
        return 1;
    }

    memset(address, 'a', address_length);
    memcpy(address, prefix, strlen(prefix));
    for (index = strlen(prefix); index < address_length; index++) {
        address[index] = (uint8_t)"0123456789abcdef"[index & 15u];
    }

    if (stnc_stnc_decode_address(
            type,
            address,
            address_length,
            decoded,
            sizeof(decoded)
        ) != 0 ||
        strlen(decoded) != address_length ||
        memcmp(decoded, prefix, strlen(prefix)) != 0) {
        return 1;
    }

    return 0;
}

int main(void)
{
    uint8_t frame[STNC_STNC_DERIVE_FRAME_MAX];
    size_t written = 99u;

    if (check_type(STNC_STNC_ADDRESS_IDENTITY, "stn0_", STNC_STNC_ADDRESS_IDENTITY_SIZE) != 0 ||
        check_type(STNC_STNC_ADDRESS_CONTRACT, "stnc0_", STNC_STNC_ADDRESS_TYPED_SIZE) != 0 ||
        check_type(STNC_STNC_ADDRESS_WALLET, "stnw0_", STNC_STNC_ADDRESS_TYPED_SIZE) != 0) {
        return 1;
    }

    if (stnc_stnc_encode_derive_address(
            99u,
            (const uint8_t *)"x",
            1u,
            UINT64_C(1),
            frame,
            sizeof(frame),
            &written
        ) == 0 ||
        written != 0u) {
        return 1;
    }

    printf("STNC codec tests passed.\n");
    return 0;
}
