#ifndef STNC_PROFILE_H
#define STNC_PROFILE_H

#include <stddef.h>

#define STNC_PROFILE_NAME_MAX 48u
#define STNC_PROFILE_PATH_MAX 1024u

typedef struct stnc_profile_info {
    char name[STNC_PROFILE_NAME_MAX + 1u];
    char wallet_path[STNC_PROFILE_PATH_MAX];
    char identity_path[STNC_PROFILE_PATH_MAX];
} stnc_profile_info;

/* Profiles are local Core metadata only. Wallet and identity private keys
 * remain in separate files and retain their existing independent formats. */
int stnc_profile_active(stnc_profile_info *profile);
int stnc_profile_create(const char *name,stnc_profile_info *profile);
int stnc_profile_select(const char *name,stnc_profile_info *profile);
int stnc_profile_name_valid(const char *name);
int stnc_profile_paths(const char *name,stnc_profile_info *profile);

#endif
