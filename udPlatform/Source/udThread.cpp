#include "udThread.h"

#if !UDPLATFORM_WINDOWS
#include <sched.h>
#include <pthread.h>
#include <errno.h>
#include <semaphore.h>
#endif

#if !UDPLATFORM_WINDOWS
void udThread_MsToTimespec(struct timespec *pTimespec, int waitMs)
{
  if (pTimespec == nullptr)
    return;

  clock_gettime(CLOCK_REALTIME, pTimespec);
  pTimespec->tv_sec += waitMs / 1000;
  pTimespec->tv_nsec += long(waitMs % 1000) * 1000000L;

  pTimespec->tv_sec += (pTimespec->tv_nsec / 1000000000L);
  pTimespec->tv_nsec %= 1000000000L;
}
#endif

static udThreadCreateCallback *s_pThreadCreateCallback;

struct udThread
{
  // First element of structure is GUARANTEED to be the operating system thread handle
# if UDPLATFORM_WINDOWS
  HANDLE handle;
# else
  pthread_t t;
# endif
  udThreadStart *pThreadStarter;
  void *pThreadData;
  volatile int32_t refCount;
};

// ****************************************************************************
void udThread_SetCreateCallback(udThreadCreateCallback *pThreadCreateCallback)
{
  s_pThreadCreateCallback = pThreadCreateCallback;
}

// ----------------------------------------------------------------------------
static uint32_t udThread_Bootstrap(udThread *pThread)
{
  if (s_pThreadCreateCallback)
    (*s_pThreadCreateCallback)(pThread, true);

  uint32_t threadReturnValue = pThread->pThreadStarter(pThread->pThreadData);

  if (s_pThreadCreateCallback)
    (*s_pThreadCreateCallback)(pThread, false);

  // Call to destroy here will decrement reference count, and only destroy if
  // the original creator of the thread didn't take a reference themselves
  udThread_Destroy(&pThread);

  return threadReturnValue;
}


// ****************************************************************************
udResult udThread_Create(udThread **ppThread, udThreadStart *pThreadStarter, void *pThreadData, udThreadCreateFlags /*flags*/)
{
  udResult result;
  udThread *pThread = nullptr;

  pThread = udAllocType(udThread, 1, udAF_Zero);
  UD_ERROR_NULL(pThread, udR_MemoryAllocationFailure);
  pThread->pThreadStarter = pThreadStarter;
  pThread->pThreadData = pThreadData;
  udInterlockedExchange(&pThread->refCount, 1);
#if UDPLATFORM_WINDOWS
  pThread->handle = CreateThread(NULL, 4096, (LPTHREAD_START_ROUTINE)udThread_Bootstrap, pThread, 0, NULL);
#else
  typedef void *(*PTHREAD_START_ROUTINE)(void *);
  pthread_create(&pThread->t, NULL, (PTHREAD_START_ROUTINE)udThread_Bootstrap, pThread);
#endif

  if (ppThread)
  {
    // Since we're returning a handle, increment the ref count because the caller is now expected to destroy it
    *ppThread = pThread;
    udInterlockedPreIncrement(&pThread->refCount);
  }
  result = udR_Success;

epilogue:
  return result;
}

// ****************************************************************************
// Author: Dave Pevreal, November 2014
void udThread_SetPriority(udThread *pThread, udThreadPriority priority)
{
  if (pThread)
  {
#if UDPLATFORM_WINDOWS
    switch (priority)
    {
    case udTP_Lowest:   SetThreadPriority(pThread->handle, THREAD_PRIORITY_LOWEST); break;
    case udTP_Low:      SetThreadPriority(pThread->handle, THREAD_PRIORITY_BELOW_NORMAL); break;
    case udTP_Normal:   SetThreadPriority(pThread->handle, THREAD_PRIORITY_NORMAL); break;
    case udTP_High:     SetThreadPriority(pThread->handle, THREAD_PRIORITY_ABOVE_NORMAL); break;
    case udTP_Highest:  SetThreadPriority(pThread->handle, THREAD_PRIORITY_HIGHEST); break;
    }
#elif UDPLATFORM_LINUX
    int policy = sched_getscheduler(0);
    int lowest = sched_get_priority_min(policy);
    int highest = sched_get_priority_max(policy);
    int pthreadPrio = (priority * (highest - lowest) / udTP_Highest) + lowest;
    pthread_setschedprio(pThread->t, pthreadPrio);
#else
    udUnused(priority);
#endif
  }
}

// ****************************************************************************
void udThread_Destroy(udThread **ppThread)
{
  if (ppThread)
  {
    udThread *pThread = *ppThread;
    *ppThread = nullptr;
    if (pThread && udInterlockedPreDecrement(&pThread->refCount) == 0)
    {
#if UDPLATFORM_WINDOWS
      CloseHandle(pThread->handle);
#endif
      udFree(pThread);
    }
  }
}

// ****************************************************************************
udResult udThread_Join(udThread *pThread, int waitMs)
{
  if (!pThread)
    return udR_InvalidParameter_;

#if UDPLATFORM_WINDOWS
  UDCOMPILEASSERT(INFINITE == UDTHREAD_WAIT_INFINITE, "Infinite constants don't match");

  DWORD result = WaitForSingleObject(pThread->handle, (DWORD)waitMs);
  if (result)
  {
    if (result == WAIT_TIMEOUT)
      return udR_Timeout;

    return udR_Failure_;
  }
#elif UDPLATFORM_LINUX
  if (waitMs == UDTHREAD_WAIT_INFINITE)
  {
    int result = pthread_join(pThread->t, nullptr);
    if (result)
    {
      if (result == EINVAL)
        return udR_InvalidParameter_;

      return udR_Failure_;
    }
  }
  else
  {
    timespec ts;
    udThread_MsToTimespec(&ts, waitMs);

    int result = pthread_timedjoin_np(pThread->t, nullptr, &ts);
    if (result)
    {
      if (result == ETIMEDOUT)
        return udR_Timeout;

      if (result == EINVAL)
        return udR_InvalidParameter_;

      return udR_Failure_;
    }
  }
#else
  udUnused(waitMs);
  int result = pthread_join(pThread->t, nullptr);
  if (result)
  {
    if (result == EINVAL)
      return udR_InvalidParameter_;

    return udR_Failure_;
  }
#endif

  return udR_Success;
}

struct udSemaphore
{
#if UDPLATFORM_WINDOWS
  CRITICAL_SECTION criticalSection;
  CONDITION_VARIABLE condition;
#else
  pthread_mutex_t mutex;
  pthread_cond_t condition;
#endif

  volatile int count;
  volatile int refCount;
};

// ****************************************************************************
// Author: Samuel Surtees, August 2017
udSemaphore *udCreateSemaphore()
{
  udSemaphore *pSemaphore = udAllocType(udSemaphore, 1, udAF_None);

#if UDPLATFORM_WINDOWS
  InitializeCriticalSection(&pSemaphore->criticalSection);
  InitializeConditionVariable(&pSemaphore->condition);
#else
  pthread_mutex_init(&(pSemaphore->mutex), NULL);
  pthread_cond_init(&(pSemaphore->condition), NULL);
#endif

  pSemaphore->count = 0;
  pSemaphore->refCount = 1;

  return pSemaphore;
}

// ----------------------------------------------------------------------------
// Author: Samuel Surtees, August 2017
void udDestroySemaphore_Internal(udSemaphore *pSemaphore)
{
  if (pSemaphore == nullptr)
    return;

#if UDPLATFORM_WINDOWS
  DeleteCriticalSection(&pSemaphore->criticalSection);
  // CONDITION_VARIABLE doesn't have a delete/destroy function
#else
  pthread_mutex_destroy(&pSemaphore->mutex);
  pthread_cond_destroy(&pSemaphore->condition);
#endif

  udFree(pSemaphore);
}

// ----------------------------------------------------------------------------
// Author: Samuel Surtees, August 2017
void udLockSemaphore_Internal(udSemaphore *pSemaphore)
{
#if UDPLATFORM_WINDOWS
  EnterCriticalSection(&pSemaphore->criticalSection);
#else
  pthread_mutex_lock(&pSemaphore->mutex);
#endif
}

// ----------------------------------------------------------------------------
// Author: Samuel Surtees, August 2017
void udUnlockSemaphore_Internal(udSemaphore *pSemaphore)
{
#if UDPLATFORM_WINDOWS
  LeaveCriticalSection(&pSemaphore->criticalSection);
#else
  pthread_mutex_unlock(&pSemaphore->mutex);
#endif
}

// ----------------------------------------------------------------------------
// Author: Samuel Surtees, August 2017
void udWakeSemaphore_Internal(udSemaphore *pSemaphore)
{
#if UDPLATFORM_WINDOWS
  WakeConditionVariable(&pSemaphore->condition);
#else
  pthread_cond_signal(&pSemaphore->condition);
#endif
}

// ----------------------------------------------------------------------------
// Author: Samuel Surtees, August 2017
bool udSleepSemaphore_Internal(udSemaphore *pSemaphore, int waitMs)
{
#if UDPLATFORM_WINDOWS
  BOOL retVal = SleepConditionVariableCS(&pSemaphore->condition, &pSemaphore->criticalSection, (waitMs == UDTHREAD_WAIT_INFINITE ? INFINITE : waitMs));
  return (retVal == TRUE);
#else
  int retVal = 0;
  if (waitMs == UDTHREAD_WAIT_INFINITE)
  {
    retVal = pthread_cond_wait(&(pSemaphore->condition), &(pSemaphore->mutex));
  }
  else
  {
    struct timespec ts;
    udThread_MsToTimespec(&ts, waitMs);
    retVal = pthread_cond_timedwait(&(pSemaphore->condition), &(pSemaphore->mutex), &ts);
  }

  return (retVal == 0);
#endif
}

// ****************************************************************************
// Author: Samuel Surtees, August 2017
void udDestroySemaphore(udSemaphore **ppSemaphore)
{
  if (ppSemaphore == nullptr)
    return;

  udSemaphore *pSemaphore = (*(udSemaphore*volatile*)ppSemaphore);
  if (udInterlockedCompareExchangePointer(ppSemaphore, nullptr, pSemaphore) != pSemaphore)
    return;

  udLockSemaphore_Internal(pSemaphore);
  if (udInterlockedPreDecrement(&pSemaphore->refCount) == 0)
  {
    udDestroySemaphore_Internal(pSemaphore);
  }
  else
  {
    int refCount = pSemaphore->refCount;
    for (int i = 0; i < refCount; ++i)
    {
      ++(pSemaphore->count);
      udWakeSemaphore_Internal(pSemaphore);
    }
    udUnlockSemaphore_Internal(pSemaphore);
  }
}

// ****************************************************************************
// Author: Samuel Surtees, August 2017
void udIncrementSemaphore(udSemaphore *pSemaphore, int count)
{
  // Exit the function if the refCount is 0 - It's being destroyed!
  if (pSemaphore == nullptr || pSemaphore->refCount == 0)
    return;

  udInterlockedPreIncrement(&pSemaphore->refCount);
  while (count-- > 0)
  {
    udLockSemaphore_Internal(pSemaphore);
    ++(pSemaphore->count);
    udWakeSemaphore_Internal(pSemaphore);
    udUnlockSemaphore_Internal(pSemaphore);
  }
  udInterlockedPreDecrement(&pSemaphore->refCount);
}

// ****************************************************************************
// Author: Samuel Surtees, August 2017
int udWaitSemaphore(udSemaphore *pSemaphore, int waitMs)
{
  // Exit the function if the refCount is 0 - It's being destroyed!
  if (pSemaphore == nullptr || pSemaphore->refCount == 0)
    return -1;

  udInterlockedPreIncrement(&pSemaphore->refCount);
  udLockSemaphore_Internal(pSemaphore);
  bool retVal;
  if (waitMs == UDTHREAD_WAIT_INFINITE)
  {
    retVal = true;
    while (pSemaphore->count == 0)
    {
      retVal = udSleepSemaphore_Internal(pSemaphore, waitMs);

      // If something went wrong, exit the loop
      if (!retVal)
        break;
    }

    if (retVal)
      pSemaphore->count--;
  }
  else
  {
    retVal = udSleepSemaphore_Internal(pSemaphore, waitMs);

    if (retVal)
    {
      // Check for spurious wake-up
      if (pSemaphore->count > 0)
        pSemaphore->count--;
      else
        retVal = false;
    }
  }

  if (udInterlockedPreDecrement(&pSemaphore->refCount) == 0)
  {
    udDestroySemaphore_Internal(pSemaphore);
    return -1;
  }
  else
  {
    udUnlockSemaphore_Internal(pSemaphore);

    // 0 is success, not 0 is failure
    return !retVal;
  }
}

// ****************************************************************************
// Author: Samuel Surtees, September 2017
udConditionVariable *udCreateConditionVariable()
{
#if UDPLATFORM_WINDOWS
  CONDITION_VARIABLE *pCondition = udAllocType(CONDITION_VARIABLE, 1, udAF_None);
  InitializeConditionVariable(pCondition);
#else
  pthread_cond_t *pCondition = udAllocType(pthread_cond_t, 1, udAF_None);
  pthread_cond_init(pCondition, NULL);
#endif

  return (udConditionVariable*)pCondition;
}

// ****************************************************************************
// Author: Samuel Surtees, September 2017
void udDestroyConditionVariable(udConditionVariable **ppConditionVariable)
{
  udConditionVariable *pCondition = *ppConditionVariable;
  *ppConditionVariable = nullptr;

  // Windows doesn't have a clean-up function
#if !UDPLATFORM_WINDOWS
  pthread_cond_destroy((pthread_cond_t *)pCondition);
#endif

  udFree(pCondition);
}

// ****************************************************************************
// Author: Samuel Surtees, September 2017
void udSignalConditionVariable(udConditionVariable *pConditionVariable, int count)
{
  while (count-- > 0)
  {
#if UDPLATFORM_WINDOWS
    CONDITION_VARIABLE *pCondition = (CONDITION_VARIABLE*)pConditionVariable;
    WakeConditionVariable(pCondition);
#else
    pthread_cond_t *pCondition = (pthread_cond_t*)pConditionVariable;
    pthread_cond_signal(pCondition);
#endif
  }
}

// ****************************************************************************
// Author: Samuel Surtees, September 2017
int udWaitConditionVariable(udConditionVariable *pConditionVariable, udMutex *pMutex, int waitMs)
{
#if UDPLATFORM_WINDOWS
  CONDITION_VARIABLE *pCondition = (CONDITION_VARIABLE*)pConditionVariable;
  CRITICAL_SECTION *pCriticalSection = (CRITICAL_SECTION*)pMutex;
  BOOL retVal = SleepConditionVariableCS(pCondition, pCriticalSection, (waitMs == UDTHREAD_WAIT_INFINITE ? INFINITE : waitMs));
  return (retVal == TRUE ? 0 : 1); // This isn't (!retVal) for clarity.
#else
  pthread_cond_t *pCondition = (pthread_cond_t*)pConditionVariable;
  pthread_mutex_t *pMutexInternal = (pthread_mutex_t*)pMutex;
  int retVal = 0;
  if (waitMs == UDTHREAD_WAIT_INFINITE)
  {
    retVal = pthread_cond_wait(pCondition, pMutexInternal);
  }
  else
  {
    struct timespec ts;
    udThread_MsToTimespec(&ts, waitMs);
    retVal = pthread_cond_timedwait(pCondition, pMutexInternal, &ts);
  }

  return retVal;
#endif
}

// ****************************************************************************
udMutex *udCreateMutex()
{
#if UDPLATFORM_WINDOWS
  CRITICAL_SECTION *pCriticalSection = udAllocType(CRITICAL_SECTION, 1, udAF_None);
  if (pCriticalSection)
    InitializeCriticalSection(pCriticalSection);
  return (udMutex *)pCriticalSection;
#else
  pthread_mutex_t *mutex = (pthread_mutex_t *)udAlloc(sizeof(pthread_mutex_t));
  if (mutex)
    pthread_mutex_init(mutex, NULL);
  return (udMutex*)mutex;
#endif
}

// ****************************************************************************
void udDestroyMutex(udMutex **ppMutex)
{
  if (ppMutex && *ppMutex)
  {
#if UDPLATFORM_WINDOWS
    CRITICAL_SECTION *pCriticalSection = (CRITICAL_SECTION*)(*ppMutex);
    *ppMutex = NULL;
    DeleteCriticalSection(pCriticalSection);
    udFree(pCriticalSection);
#else
    pthread_mutex_t *mutex = (pthread_mutex_t *)(*ppMutex);
    pthread_mutex_destroy(mutex);
    udFree(mutex);
    *ppMutex = nullptr;
#endif
  }
}

// ****************************************************************************
void udLockMutex(udMutex *pMutex)
{
  if (pMutex)
  {
#if UDPLATFORM_WINDOWS
    EnterCriticalSection((CRITICAL_SECTION*)pMutex);
#else
    pthread_mutex_lock((pthread_mutex_t *)pMutex);
#endif
  }
}

// ****************************************************************************
void udReleaseMutex(udMutex *pMutex)
{
  if (pMutex)
  {
#if UDPLATFORM_WINDOWS
    LeaveCriticalSection((CRITICAL_SECTION*)pMutex);
#else
    pthread_mutex_unlock((pthread_mutex_t *)pMutex);
#endif
  }
}


