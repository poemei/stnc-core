#ifndef STNC_PLATFORM_H
#define STNC_PLATFORM_H

/*
 * Platform services used by the portable STNC Core runtime.
 *
 * Platform implementations must not alter Chain, STNC,
 * serialization, validation, or other protocol-visible behavior.
 */

int stnc_platform_init(void);
void stnc_platform_shutdown(void);

#endif