#ifndef STNC_WALLET_STATUS_H
#define STNC_WALLET_STATUS_H
#include <stdint.h>
#include "stnc_wallet.h"
typedef struct stnc_wallet_status {
    int present;
    int key_valid;
    int balance_available;
    char address[STNC_WALLET_ADDRESS_SIZE + 1u];
    uint64_t accepted_balance;
} stnc_wallet_status;
int stnc_wallet_status_read(stnc_wallet_status *status);
#endif
