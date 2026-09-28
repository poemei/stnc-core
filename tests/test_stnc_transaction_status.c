#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "stnc_transaction_status.h"

int main(void)
{
    stnc_transaction_status status;
    uint8_t id[STNC_TRANSACTION_STATUS_ID_SIZE];

    memset(&status, 0, sizeof(status));
    memset(id, 0, sizeof(id));

    assert(STNC_TRANSACTION_STATUS_ID_SIZE == 32u);
    assert(STNC_TRANSACTION_STATUS_PAYLOAD_SIZE == 44u);
    assert(STNC_TRANSACTION_ACCEPTANCE_ERROR == 0);
    assert(STNC_TRANSACTION_ACCEPTANCE_PENDING == 1);
    assert(STNC_TRANSACTION_ACCEPTANCE_ACCEPTED == 2);
    assert(sizeof(status.block_id) == 32u);
    assert(sizeof(id) == 32u);

    return 0;
}
