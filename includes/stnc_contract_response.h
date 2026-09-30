#ifndef STNC_CONTRACT_RESPONSE_H
#define STNC_CONTRACT_RESPONSE_H

#include <stddef.h>
#include <stdint.h>

#define STNC_CONTRACT_RESPONSE_VERSION 1u
#define STNC_CONTRACT_RESPONSE_MAX_TEXT 65536u
#define STNC_CONTRACT_RESPONSE_HEADER_SIZE 140u
#define STNC_CONTRACT_RESPONSE_MAX_SIZE (STNC_CONTRACT_RESPONSE_HEADER_SIZE + STNC_CONTRACT_RESPONSE_MAX_TEXT)
#define STNC_CONTRACT_RESPONSE_TRANSACTION_HEADER_SIZE 12u
#define STNC_CONTRACT_RESPONSE_TRANSACTION_MAX (STNC_CONTRACT_RESPONSE_TRANSACTION_HEADER_SIZE + STNC_CONTRACT_RESPONSE_MAX_SIZE)

/* Build the canonical signed STRP response and wrap it in an STNT type-11
 * transaction ready for stnc_core_submit_transaction(). Contract address must
 * be canonical stnc0_<64 lowercase hex>. Text is exact UTF-8 bytes. */
int stnc_contract_response_build(const char *contract_address,
    const uint8_t *text,size_t text_length,uint8_t *transaction,
    size_t capacity,size_t *written);

#endif
