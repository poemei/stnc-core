#ifndef STNC_CONTRACT_CREATE_H
#define STNC_CONTRACT_CREATE_H
#include <stdint.h>
#include "stnc_contract_draft.h"
#include "stnc_stnc.h"
typedef struct stnc_contract_create_result {
    char address[71];
    uint16_t submission;
    int has_transaction_id;
    uint8_t transaction_id[32];
} stnc_contract_create_result;
int stnc_contract_create(const stnc_contract_draft_input *input,stnc_contract_create_result *result);
#endif
