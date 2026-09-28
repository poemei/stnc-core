#include <string.h>

#include "stnc_core.h"
#include "stnc_stnc.h"
#include "stnc_transaction_status.h"

stnc_transaction_acceptance stnc_transaction_status_query(
    const uint8_t transaction_id[STNC_TRANSACTION_STATUS_ID_SIZE],
    stnc_transaction_status *status)
{
    return stnc_core_transaction_status(transaction_id, status);
}
