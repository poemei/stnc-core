#include "stnc_contract_response.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do{if(!(x)){fprintf(stderr,"stnc contract response test failed: %d\n",__LINE__);return 1;}}while(0)

int main(void)
{
    uint8_t transaction[STNC_CONTRACT_RESPONSE_TRANSACTION_MAX];size_t written=0u;
    static const uint8_t text[]="Accepted.";
    /* Builder must reject malformed addresses and empty responses before it
     * attempts to read/sign with a local identity. */
    CHECK(stnc_contract_response_build("stnc0_bad",text,sizeof(text)-1u,
        transaction,sizeof(transaction),&written)!=0);
    CHECK(written==0u);
    CHECK(stnc_contract_response_build(
        "stnc0_0000000000000000000000000000000000000000000000000000000000000000",
        text,0u,transaction,sizeof(transaction),&written)!=0);
    CHECK(written==0u);
    puts("stnc contract response tests passed");
    return 0;
}
