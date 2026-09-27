#ifndef STNC_BACKGROUND_MINING_H
#define STNC_BACKGROUND_MINING_H

#include "stnc_mining_service.h"

int stnc_background_mining_init(void);
/* Runtime tick always maintains the persisted stn0_ -> stnw0_ compensation
 * registration while Core is connected, even when background CPU mining is
 * disabled or a non-CPU mining backend is selected. Mining work itself still
 * follows the configured enable/backend policy. */
void stnc_background_mining_tick(void);
void stnc_background_mining_shutdown(void);
void stnc_background_mining_status(stnc_mining_service_status *status);
int stnc_background_mining_connected(void);

#endif
