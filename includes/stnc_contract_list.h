#ifndef STNC_CONTRACT_LIST_H
#define STNC_CONTRACT_LIST_H

#include <stddef.h>
#include <stdint.h>

#define STNC_CONTRACT_LIST_MAX 16u
#define STNC_CONTRACT_LIST_ADDRESS_SIZE 70u
#define STNC_CONTRACT_LIST_ENTRY_SIZE 90u

typedef struct stnc_contract_list_entry {
    char address[STNC_CONTRACT_LIST_ADDRESS_SIZE + 1u];
    uint16_t state;
    uint16_t type;
    uint64_t sequence;
    uint64_t created_at;
} stnc_contract_list_entry;

typedef struct stnc_contract_list {
    size_t count;
    stnc_contract_list_entry entries[STNC_CONTRACT_LIST_MAX];
} stnc_contract_list;

int stnc_contract_list_decode(const uint8_t *payload,size_t length,stnc_contract_list *list);
int stnc_contract_list_read(const char *identity,stnc_contract_list *list);

#endif
