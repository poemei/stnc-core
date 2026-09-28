#include <assert.h>
#include <string.h>

#include "stnc_mutex.h"

int main(void)
{
    stnc_mutex mutex;
    memset(&mutex,0,sizeof(mutex));
    assert(stnc_mutex_init(&mutex)==0);
    assert(mutex.handle!=NULL);
    assert(stnc_mutex_lock(&mutex)==0);
    stnc_mutex_unlock(&mutex);
    stnc_mutex_destroy(&mutex);
    assert(mutex.handle==NULL);
    return 0;
}
