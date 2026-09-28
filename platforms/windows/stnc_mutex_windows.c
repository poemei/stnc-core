#include <windows.h>
#include <stdlib.h>

#include "stnc_mutex.h"

int stnc_mutex_init(stnc_mutex *mutex)
{
    CRITICAL_SECTION *section;
    if(mutex==NULL)return 1;
    mutex->handle=NULL;
    section=(CRITICAL_SECTION *)malloc(sizeof(*section));
    if(section==NULL)return 1;
    InitializeCriticalSection(section);
    mutex->handle=section;
    return 0;
}

void stnc_mutex_destroy(stnc_mutex *mutex)
{
    CRITICAL_SECTION *section;
    if(mutex==NULL||mutex->handle==NULL)return;
    section=(CRITICAL_SECTION *)mutex->handle;
    DeleteCriticalSection(section);
    free(section);
    mutex->handle=NULL;
}

int stnc_mutex_lock(stnc_mutex *mutex)
{
    if(mutex==NULL||mutex->handle==NULL)return 1;
    EnterCriticalSection((CRITICAL_SECTION *)mutex->handle);
    return 0;
}

void stnc_mutex_unlock(stnc_mutex *mutex)
{
    if(mutex==NULL||mutex->handle==NULL)return;
    LeaveCriticalSection((CRITICAL_SECTION *)mutex->handle);
}
