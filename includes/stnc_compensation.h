#ifndef STNC_COMPENSATION_H
#define STNC_COMPENSATION_H

#include <stddef.h>
#include <stdint.h>

#define STNC_COMPENSATION_TRANSACTION_SIZE 77u

/* Build the canonical STN_TX_COMPENSATION_DESTINATION transaction for one
 * local mining identity and payout wallet. Addresses are exact canonical text. */
int stnc_compensation_build(const char *identity,const char *wallet,
    uint8_t transaction[STNC_COMPENSATION_TRANSACTION_SIZE]);

/* Ensure the local persisted stn0_ identity -> stnw0_ wallet relationship has
 * been submitted to Chain. ADMITTED and DUPLICATE are success. */
int stnc_compensation_ensure(void);

#endif
