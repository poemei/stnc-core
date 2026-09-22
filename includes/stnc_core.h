#ifndef STNC_CORE_H
#define STNC_CORE_H

typedef enum stnc_core_state {
    STNC_CORE_STATE_UNINITIALIZED = 0,
    STNC_CORE_STATE_INITIALIZED,
    STNC_CORE_STATE_RUNNING,
    STNC_CORE_STATE_STOPPING,
    STNC_CORE_STATE_STOPPED
} stnc_core_state;

int stnc_core_init(void);
int stnc_core_run(void);
void stnc_core_request_stop(void);
void stnc_core_shutdown(void);

stnc_core_state stnc_core_get_state(void);

#endif