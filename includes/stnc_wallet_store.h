#ifndef STNC_WALLET_STORE_H
#define STNC_WALLET_STORE_H
#include "stnc_wallet.h"

int stnc_wallet_store_exists(void);
int stnc_wallet_store_create(stnc_wallet_key *key);
int stnc_wallet_store_load(stnc_wallet_key *key);

#endif
