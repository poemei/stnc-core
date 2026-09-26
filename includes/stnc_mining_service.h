#ifndef STNC_MINING_SERVICE_H
#define STNC_MINING_SERVICE_H

#include <stdint.h>

#define STNC_MINING_CPU_LIMIT_MAX 2u
#define STNC_MINING_CPU_WINDOW_MS 1000u

typedef enum stnc_mining_backend {
    STNC_MINING_BACKEND_AUTOMATIC = 0,
    STNC_MINING_BACKEND_CPU = 1,
    STNC_MINING_BACKEND_GPU = 2,
    STNC_MINING_BACKEND_USB_ASIC = 3
} stnc_mining_backend;

typedef struct stnc_mining_service_config {
    int enabled;
    stnc_mining_backend backend;
    unsigned int cpu_limit_percent;
} stnc_mining_service_config;

typedef struct stnc_mining_service_status {
    int enabled;
    int running;
    stnc_mining_backend configured_backend;
    stnc_mining_backend active_backend;
    unsigned int cpu_limit_percent;
    uint64_t passes;
    uint64_t attempts;
    uint64_t solutions;
    uint64_t hashrate_hps;
} stnc_mining_service_status;

int stnc_mining_service_configure(const stnc_mining_service_config *config);
void stnc_mining_service_reset(void);
void stnc_mining_service_set_running(int running, stnc_mining_backend backend);
void stnc_mining_service_record_pass(uint64_t attempts, int solution);
/* Record one active mining pass. For CPU mining, hashrate_hps is the
 * effective contribution over the configured 1000 ms duty window rather
 * than the short burst rate. Other backends retain active-pass rate. */
void stnc_mining_service_record_rate(uint64_t attempts, uint64_t elapsed_ms);
void stnc_mining_service_status_read(stnc_mining_service_status *status);
unsigned int stnc_mining_service_cpu_work_ms(void);
unsigned int stnc_mining_service_cpu_rest_ms(unsigned int elapsed_work_ms);
const char *stnc_mining_backend_name(stnc_mining_backend backend);

#endif
