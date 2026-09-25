#ifndef STNC_CONFIG_H
#define STNC_CONFIG_H

#define STNC_CONFIG_PEER_MAX 256

typedef struct stnc_config {
    char peer[STNC_CONFIG_PEER_MAX];
    unsigned short port;
    char root_peer[STNC_CONFIG_PEER_MAX];
    unsigned short root_peer_port;
    int mining_enabled;
    char mining_backend[16];
    unsigned int mining_cpu_limit_percent;
    char stratum_host[STNC_CONFIG_PEER_MAX];
    unsigned short stratum_port;
} stnc_config;

/*
 * Initialize configuration.
 *
 * config.json is loaded from the directory containing
 * the STNC Core executable.
 *
 * If config.json does not exist, the default configuration
 * is created and stored there.
 *
 * Returns 0 on success.
 * Returns non-zero on failure.
 */
int stnc_config_init(void);

/*
 * Return the active configuration.
 *
 * Returns NULL if configuration is not initialized.
 */
const stnc_config *stnc_config_get(void);

/*
 * Release configuration state.
 */
void stnc_config_shutdown(void);

#endif
