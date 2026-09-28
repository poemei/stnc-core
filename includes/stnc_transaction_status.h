#ifndef STNC_TRANSACTION_STATUS_H
#define STNC_TRANSACTION_STATUS_H

#include <stdint.h>

#define STNC_TRANSACTION_STATUS_ID_SIZE 32u
#define STNC_TRANSACTION_STATUS_PAYLOAD_SIZE 44u

typedef enum stnc_transaction_acceptance {
    STNC_TRANSACTION_ACCEPTANCE_ERROR = 0,
    STNC_TRANSACTION_ACCEPTANCE_PENDING = 1,
    STNC_TRANSACTION_ACCEPTANCE_ACCEPTED = 2
} stnc_transaction_acceptance;

typedef struct stnc_transaction_status {
    uint64_t height;
    uint8_t block_id[32];
    uint32_t transaction_position;
} stnc_transaction_status;

stnc_transaction_acceptance stnc_transaction_status_query(
    const uint8_t transaction_id[STNC_TRANSACTION_STATUS_ID_SIZE],
    stnc_transaction_status *status
);

#endif
