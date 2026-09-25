#ifndef STNC_MINING_H
#define STNC_MINING_H
#include <stddef.h>
#include <stdint.h>
#include "stnc_stnc.h"
#define STNC_MINING_HASH_INPUT_SIZE (20u+STNC_STNC_BLOCK_HEADER_SIZE)
typedef enum stnc_mining_result {STNC_MINING_ERROR=0,STNC_MINING_FOUND=1,STNC_MINING_EXHAUSTED=2} stnc_mining_result;
int stnc_mining_hash(const uint8_t block[STNC_STNC_BLOCK_HEADER_SIZE],uint8_t digest[32]);
int stnc_mining_hash_meets_target(const uint8_t digest[32],const uint8_t target[32]);
stnc_mining_result stnc_mining_search(uint8_t block[STNC_STNC_BLOCK_HEADER_SIZE],
    uint64_t first_nonce,uint64_t attempts,uint64_t *found_nonce,uint8_t digest[32]);
stnc_mining_result stnc_mining_search_timed(uint8_t block[STNC_STNC_BLOCK_HEADER_SIZE],
    uint64_t first_nonce,unsigned int budget_ms,uint64_t *attempts,
    uint64_t *found_nonce,uint8_t digest[32]);
#endif
