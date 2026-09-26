#ifndef STNC_LOG_H
#define STNC_LOG_H

#include <stddef.h>

#define STNC_LOG_RECENT_CAPACITY 5u
#define STNC_LOG_ENTRY_MAX 512u

/*
 * STNC Core Logging
 *
 * Full runtime history is written to stnc-core.log. The logger also keeps
 * the five most recent entries in memory for the operator console/UI.
 */

int stnc_log_init(void);

void stnc_log_info(const char *message);
void stnc_log_warning(const char *message);
void stnc_log_error(const char *message);

size_t stnc_log_recent_count(void);
int stnc_log_recent_get(size_t index,char *entry,size_t capacity);
void stnc_log_console_enable(int enabled);
void stnc_log_console_refresh(void);

void stnc_log_shutdown(void);

#endif
