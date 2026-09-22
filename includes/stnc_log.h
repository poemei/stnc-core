#ifndef STNC_LOG_H
#define STNC_LOG_H

/*
 * STNC Core Logging
 *
 * Provides the portable logging interface used by the
 * STNC Core runtime.
 */

int stnc_log_init(void);

void stnc_log_info(const char *message);
void stnc_log_warning(const char *message);
void stnc_log_error(const char *message);

void stnc_log_shutdown(void);

#endif