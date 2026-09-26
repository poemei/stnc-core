#ifndef STNC_CONTRACT_ACTION_H
#define STNC_CONTRACT_ACTION_H
#include <stddef.h>
#include <stdint.h>
#define STNC_CONTRACT_ACTION_MAX 66881u
/* Construct a signed STNT type-5 request from exact current STCT bytes and
 * externally supplied authority evidence. Uses identity.key exclusively.
 * Scope matching is an input check, NOT accepted-grant validation. This API
 * neither submits nor declares any action, grant or transition accepted. */
int stnc_contract_action_build(const uint8_t *current,size_t length,uint16_t action,
    const uint8_t *authority,size_t authority_length,uint8_t *transaction,
    size_t capacity,size_t *written);
#endif
