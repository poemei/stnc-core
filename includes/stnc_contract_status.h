#ifndef STNC_CONTRACT_STATUS_H
#define STNC_CONTRACT_STATUS_H
#include "stnc_stnc.h"
typedef struct stnc_contract_status {
    int available;
    char address[STNC_STNC_ADDRESS_TYPED_SIZE + 1u];
    stnc_contract_state state;
} stnc_contract_status;
int stnc_contract_status_read(const char *address,stnc_contract_status *status);
const char *stnc_contract_state_name(uint16_t state);
const char *stnc_contract_type_name(uint16_t type);
#endif
