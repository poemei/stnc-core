#ifndef STNC_BACKGROUND_MINING_H
#define STNC_BACKGROUND_MINING_H

#include "stnc_mining_service.h"

int stnc_background_mining_init(void);
void stnc_background_mining_tick(void);
void stnc_background_mining_shutdown(void);
void stnc_background_mining_status(stnc_mining_service_status *status);

#endif
