#ifndef STNC_CONTRACT_DRAFT_H
#define STNC_CONTRACT_DRAFT_H
#include <stddef.h>
#include <stdint.h>
#define STNC_CONTRACT_DRAFT_MAX 66656u
typedef struct stnc_contract_participant_input {
    char public_key[65];
    uint16_t role;
} stnc_contract_participant_input;
typedef struct stnc_contract_draft_input {
    uint16_t type;
    uint64_t created_at;
    size_t participant_count;
    stnc_contract_participant_input participants[32];
    const uint8_t *terms;
    size_t terms_length;
} stnc_contract_draft_input;
/* Exact STCT v1, Draft/0. Input order and terms bytes are preserved.
 * Structural construction never establishes accepted state or authority. */
int stnc_contract_draft_build(const stnc_contract_draft_input *input,
    uint8_t *bytes,size_t capacity,size_t *written,char address[71]);
int stnc_contract_draft_save(const stnc_contract_draft_input *input,const char *path,char address[71]);
int stnc_contract_actor_decode(const char *address,uint8_t identifier[32]);
#endif
