#pragma once

// nDPI 5.0 only needs the pthread mutex subset on Windows.  Mapping that
// subset to SRWLOCK keeps the vendored library self-contained and avoids a
// runtime dependency on the legacy pthreads-win32 NuGet package.

#include <windows.h>

typedef SRWLOCK pthread_mutex_t;

#define PTHREAD_MUTEX_INITIALIZER SRWLOCK_INIT

static __inline int pthread_mutex_init(pthread_mutex_t* mutex, const void* attributes)
{
    (void)attributes;
    InitializeSRWLock(mutex);
    return 0;
}

static __inline int pthread_mutex_destroy(pthread_mutex_t* mutex)
{
    (void)mutex;
    return 0;
}

static __inline int pthread_mutex_lock(pthread_mutex_t* mutex)
{
    AcquireSRWLockExclusive(mutex);
    return 0;
}

static __inline int pthread_mutex_unlock(pthread_mutex_t* mutex)
{
    ReleaseSRWLockExclusive(mutex);
    return 0;
}
