#ifndef STNC_STRATUM_CLIENT_H
#define STNC_STRATUM_CLIENT_H

#include <stddef.h>
#include <stdint.h>
#include "stnc_stratum.h"

typedef struct stnc_stratum_client {
    void *handle;
    int connected;
} stnc_stratum_client;

void stnc_stratum_client_init(stnc_stratum_client *client);
int stnc_stratum_client_connect(stnc_stratum_client *client,const char *host,unsigned short port,const char *address);
void stnc_stratum_client_disconnect(stnc_stratum_client *client);
int stnc_stratum_client_receive_job(stnc_stratum_client *client,stnc_stratum_job *job,uint8_t *block,size_t capacity);
int stnc_stratum_client_submit(stnc_stratum_client *client,const uint8_t work_id[32],uint64_t nonce,stnc_stratum_result *result);
int stnc_stratum_client_progress(stnc_stratum_client *client,const uint8_t work_id[32],uint64_t hashes,uint64_t elapsed_ms);

#endif
