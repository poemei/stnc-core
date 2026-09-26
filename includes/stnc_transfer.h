#ifndef STNC_TRANSFER_H
#define STNC_TRANSFER_H

#include <stdint.h>

#include "stnc_stnc.h"
#include "stnc_wallet.h"

typedef struct stnc_transfer_result {
    uint16_t submission;
    uint8_t transaction_id[32];
    int has_transaction_id;
    char source[STNC_WALLET_ADDRESS_SIZE + 1u];
    uint64_t accepted_balance;
    int balance_available;
} stnc_transfer_result;

int stnc_transfer_send(const char *destination,uint64_t units,stnc_transfer_result *result);
const char *stnc_transfer_submission_name(uint16_t submission);

#endif
