#ifndef STNC_STRATUM_H
#define STNC_STRATUM_H

#include <stddef.h>
#include <stdint.h>

#define STNC_STNM_VERSION 1u
#define STNC_STNM_WORK_ID_SIZE 32u
#define STNC_STNM_TARGET_SIZE 32u
#define STNC_STNM_ADDRESS_LENGTH 69u
#define STNC_STNM_JOB_HEADER_SIZE 116u
#define STNC_STNM_SUBMIT_SIZE 48u
#define STNC_STNM_RESULT_SIZE 12u
#define STNC_STNM_HASH_PROGRESS_SIZE 56u
#define STNC_STNM_ADDRESS_SIZE 77u
#define STNC_STNM_BLOCK_HEADER_SIZE 168u

typedef enum stnc_stratum_result {
    STNC_STRATUM_RESULT_ACCEPTED=0,
    STNC_STRATUM_RESULT_REJECTED=1,
    STNC_STRATUM_RESULT_STALE=2,
    STNC_STRATUM_RESULT_PROVIDER=3,
    STNC_STRATUM_RESULT_MALFORMED=4
} stnc_stratum_result;

typedef struct stnc_stratum_job {
    uint8_t work_id[STNC_STNM_WORK_ID_SIZE];
    uint8_t share_target[STNC_STNM_TARGET_SIZE];
    uint8_t chain_target[STNC_STNM_TARGET_SIZE];
    uint32_t block_length;
    uint64_t initial_nonce;
} stnc_stratum_job;

int stnc_stratum_parse_job_header(const uint8_t header[STNC_STNM_JOB_HEADER_SIZE],stnc_stratum_job *job);
int stnc_stratum_validate_job_block(const stnc_stratum_job *job,const uint8_t *block,size_t length);
int stnc_stratum_build_address(const char *address,uint8_t frame[STNC_STNM_ADDRESS_SIZE]);
int stnc_stratum_build_submit(const uint8_t work_id[32],uint64_t nonce,uint8_t frame[STNC_STNM_SUBMIT_SIZE]);
int stnc_stratum_build_hash_progress(const uint8_t work_id[32],uint64_t hashes,uint64_t elapsed_ms,uint8_t frame[STNC_STNM_HASH_PROGRESS_SIZE]);
int stnc_stratum_parse_result(const uint8_t frame[STNC_STNM_RESULT_SIZE],stnc_stratum_result *result);

#endif
