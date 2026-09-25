#ifndef STNC_WALLET_H
#define STNC_WALLET_H

#include <stddef.h>
#include <stdint.h>

#define STNC_WALLET_PUBLIC_KEY_SIZE 32u
#define STNC_WALLET_PRIVATE_KEY_SIZE 32u
#define STNC_WALLET_NONCE_SIZE 32u
#define STNC_WALLET_ADDRESS_SIZE 70u
#define STNC_WALLET_TRANSFER_SIZE 214u
#define STNC_WALLET_TRANSFER_STATEMENT_SIZE 178u

int stnc_ed25519_publickey(const uint8_t private_key[32],uint8_t public_key[32]);

typedef struct stnc_wallet_key {
    uint8_t public_key[STNC_WALLET_PUBLIC_KEY_SIZE];
    uint8_t private_key[STNC_WALLET_PRIVATE_KEY_SIZE];
} stnc_wallet_key;

int stnc_wallet_generate(stnc_wallet_key *key);
void stnc_wallet_clear(stnc_wallet_key *key);
int stnc_wallet_address(const stnc_wallet_key *key,char address[STNC_WALLET_ADDRESS_SIZE+1u]);
int stnc_wallet_build_transfer(const stnc_wallet_key *key,const char *source,const char *destination,
    uint64_t units,const uint8_t nonce[STNC_WALLET_NONCE_SIZE],uint8_t transaction[STNC_WALLET_TRANSFER_SIZE]);

#endif
