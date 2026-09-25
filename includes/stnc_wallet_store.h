#ifndef STNC_WALLET_STORE_H
#define STNC_WALLET_STORE_H
#include "stnc_wallet.h"

int stnc_wallet_store_exists(void);
int stnc_wallet_store_create(stnc_wallet_key *key);
int stnc_wallet_store_load(stnc_wallet_key *key);

/* Explicit-path variants exist for deterministic isolated qualification.
 * Production callers should use the application-path functions above. */
int stnc_wallet_store_exists_at(const char *path);
int stnc_wallet_store_create_at(const char *path,stnc_wallet_key *key);
int stnc_wallet_store_load_at(const char *path,stnc_wallet_key *key);

#endif
