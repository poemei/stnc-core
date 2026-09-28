#ifndef STNC_MUTEX_H
#define STNC_MUTEX_H

typedef struct stnc_mutex {
    void *handle;
} stnc_mutex;

int stnc_mutex_init(stnc_mutex *mutex);
void stnc_mutex_destroy(stnc_mutex *mutex);
int stnc_mutex_lock(stnc_mutex *mutex);
void stnc_mutex_unlock(stnc_mutex *mutex);

#endif
