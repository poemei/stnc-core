#ifndef STNC_IDENTITY_H
#define STNC_IDENTITY_H
#include <stddef.h>
#include <stdint.h>

typedef struct stnc_identity_status {
    int present;
    int valid;
    char address[70];
    uint8_t public_key[32];
} stnc_identity_status;

/* Independent identity.key custody. No private key crosses this interface.
 * The address is SHA256(public key) in the stn0_ namespace; the signing actor
 * is the canonical Ed25519 public key, not that address digest. */
int stnc_identity_status_read(stnc_identity_status *status);
int stnc_identity_create(stnc_identity_status *status);
int stnc_identity_sign(const uint8_t *statement,size_t length,uint8_t signature[64]);
int stnc_identity_status_at(const char *path,stnc_identity_status *status);
int stnc_identity_create_at(const char *path,stnc_identity_status *status);
int stnc_identity_sign_at(const char *path,const uint8_t *statement,size_t length,uint8_t signature[64]);
#endif
