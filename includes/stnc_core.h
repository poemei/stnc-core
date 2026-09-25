#ifndef STNC_CORE_H
#define STNC_CORE_H

#include <stddef.h>
#include <stdint.h>

typedef enum stnc_core_state {
    STNC_CORE_STATE_UNINITIALIZED = 0,
    STNC_CORE_STATE_INITIALIZED,
    STNC_CORE_STATE_RUNNING,
    STNC_CORE_STATE_STOPPING,
    STNC_CORE_STATE_STOPPED
} stnc_core_state;

typedef struct stnc_core_chain_state {
    int available;
    uint8_t network_id[32];
    uint8_t genesis_id[32];
    uint64_t height;
    uint8_t tip_id[32];
    uint8_t cumulative_work[40];
    uint8_t current_target[32];
    uint32_t protocol_revision;
    uint32_t block_count;
} stnc_core_chain_state;

int stnc_core_init(void);
int stnc_core_run(void);
void stnc_core_request_stop(void);
void stnc_core_shutdown(void);

stnc_core_state stnc_core_get_state(void);

const stnc_core_chain_state *
stnc_core_get_chain_state(void);

int stnc_core_derive_address(
    uint16_t type,
    const uint8_t *source,
    size_t source_length,
    char *address,
    size_t capacity
);

#endif
