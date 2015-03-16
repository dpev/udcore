#include "udPlatform.h"
#include <stdlib.h>
#include <string.h>
#if UDPLATFORM_LINUX || UDPLATFORM_NACL
#include <sched.h>
#include <pthread.h>
#include <semaphore.h>
#endif

// ****************************************************************************
udThreadHandle udCreateThread(udThreadStart *threadStarter, void *threadData)
{
#if UDPLATFORM_WINDOWS
  return (udThreadHandle)CreateThread(NULL, 4096, (LPTHREAD_START_ROUTINE)threadStarter, threadData, 0, NULL);
#elif UDPLATFORM_LINUX || UDPLATFORM_NACL
  pthread_t t;
  typedef void *(*PTHREAD_START_ROUTINE)(void *);
  pthread_create(&t, NULL, (PTHREAD_START_ROUTINE)threadStarter, threadData);
  return (udThreadHandle)t;
#else
#error Unknown platform
#endif
}

// ****************************************************************************
// Author: Dave Pevreal, November 2014
void udSetThreadPriority(udThreadHandle threadHandle, udThreadPriority priority)
{
  if (threadHandle)
  {
#if UDPLATFORM_WINDOWS
    switch (priority)
    {
      case udTP_Lowest:   SetThreadPriority((HANDLE)threadHandle, THREAD_PRIORITY_LOWEST); break;
      case udTP_Low:      SetThreadPriority((HANDLE)threadHandle, THREAD_PRIORITY_BELOW_NORMAL); break;
      case udTP_Normal:   SetThreadPriority((HANDLE)threadHandle, THREAD_PRIORITY_NORMAL); break;
      case udTP_High:     SetThreadPriority((HANDLE)threadHandle, THREAD_PRIORITY_ABOVE_NORMAL); break;
      case udTP_Highest:  SetThreadPriority((HANDLE)threadHandle, THREAD_PRIORITY_HIGHEST); break;
    }
#elif UDPLATFORM_LINUX
    int policy = sched_getscheduler(0);
    int lowest = sched_get_priority_min(policy);
    int highest = sched_get_priority_max(policy);
    int pthreadPrio = (priority * (highest - lowest) / udTP_Highest) + lowest;
    pthread_setschedprio((pthread_t)threadHandle, pthreadPrio);
#elif UDPLATFORM_NACL
#else
#   error Unknown platform
#endif
  }
}

// ****************************************************************************
void udDestroyThread(udThreadHandle *pThreadHandle)
{
  if (pThreadHandle)
  {
#if UDPLATFORM_WINDOWS
    CloseHandle((HANDLE)(*pThreadHandle));
#elif UDPLATFORM_LINUX || UDPLATFORM_NACL
    // TODO: FIgure out which pthread function is *most* equivalent
#else
#   error Unknown platform
#endif
    *pThreadHandle = 0;
  }
}

// ****************************************************************************
udSemaphore *udCreateSemaphore(int maxValue, int initialValue)
{
#if UDPLATFORM_WINDOWS
  HANDLE handle = CreateSemaphore(NULL, initialValue, maxValue, NULL);
  return (udSemaphore *)handle;
#elif UDPLATFORM_LINUX || UDPLATFORM_NACL
  sem_t *sem = (sem_t *)udAlloc(sizeof(sem_t));
  (void)maxValue;
  if (sem)
    sem_init(sem, 0, initialValue);
  return (udSemaphore*)sem;
#else
# error Unknown platform
#endif
}

// ****************************************************************************
void udDestroySemaphore(udSemaphore **ppSemaphore)
{
  if (ppSemaphore && *ppSemaphore)
  {
#if UDPLATFORM_WINDOWS
    HANDLE semHandle = (HANDLE)(*ppSemaphore);
    *ppSemaphore = NULL;
    CloseHandle(semHandle);
#elif UDPLATFORM_LINUX || UDPLATFORM_NACL
    sem_t *sem = (sem_t*)(*ppSemaphore);
    sem_destroy(sem);
    udFree(sem);
    *ppSemaphore = nullptr;
#else
#  error Unknown platform
#endif
  }
}

// ****************************************************************************
void udIncrementSemaphore(udSemaphore *pSemaphore, int count)
{
  if (pSemaphore)
  {
#if UDPLATFORM_WINDOWS
    ReleaseSemaphore((HANDLE)pSemaphore, count, nullptr);
#elif UDPLATFORM_LINUX || UDPLATFORM_NACL
    while (count-- > 0)
      sem_post((sem_t*)pSemaphore);
#else
#   error Unknown platform
#endif
  }
}

// ****************************************************************************
int udWaitSemaphore(udSemaphore *pSemaphore, int waitMs)
{
  if (pSemaphore)
  {
#if UDPLATFORM_WINDOWS
    return WaitForSingleObject((HANDLE)pSemaphore, waitMs);
#elif UDPLATFORM_LINUX
    struct  timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = waitMs * 1000;
    return sem_timedwait((sem_t*)pSemaphore, &ts);
#elif UDPLATFORM_NACL
    return sem_wait((sem_t*)pSemaphore);  // TODO: Need to find out timedwait equiv for NACL
#else
#   error Unknown platform
#endif
  }
  return -1;
}

// ****************************************************************************
udMutex *udCreateMutex()
{
#if UDPLATFORM_WINDOWS
  HANDLE handle = CreateMutex(NULL, FALSE, NULL);
  return (udMutex *)handle;
#elif UDPLATFORM_LINUX || UDPLATFORM_NACL
  pthread_mutex_t *mutex = (pthread_mutex_t *)udAlloc(sizeof(pthread_mutex_t));
  if (mutex)
    pthread_mutex_init(mutex, NULL);
  return (udMutex*)mutex;
#else
#error Unknown platform
#endif
}

// ****************************************************************************
void udDestroyMutex(udMutex **ppMutex)
{
  if (ppMutex && *ppMutex)
  {
#if UDPLATFORM_WINDOWS
    HANDLE mutexHandle = (HANDLE)(*ppMutex);
    *ppMutex = NULL;
    CloseHandle(mutexHandle);
#elif UDPLATFORM_LINUX || UDPLATFORM_NACL
    pthread_mutex_t *mutex = (pthread_mutex_t *)(*ppMutex);
    pthread_mutex_destroy(mutex);
    udFree(mutex);
    *ppMutex = nullptr;
#else
#  error Unknown platform
#endif
  }
}

// ****************************************************************************
void udLockMutex(udMutex *pMutex)
{
  if (pMutex)
  {
#if UDPLATFORM_WINDOWS
    WaitForSingleObject((HANDLE)pMutex, INFINITE);
#elif UDPLATFORM_LINUX || UDPLATFORM_NACL
    pthread_mutex_lock((pthread_mutex_t *)pMutex);
#else
#   error Unknown platform
#endif
  }
}

// ****************************************************************************
void udReleaseMutex(udMutex *pMutex)
{
  if (pMutex)
  {
#if UDPLATFORM_WINDOWS
    ReleaseMutex((HANDLE)pMutex);
#elif UDPLATFORM_LINUX || UDPLATFORM_NACL
    pthread_mutex_unlock((pthread_mutex_t *)pMutex);
#else
#error Unknown platform
#endif
  }
}


#if __MEMORY_DEBUG__
# if defined(_MSC_VER)
#   pragma warning(disable:4530) //  C++ exception handler used, but unwind semantics are not enabled.
# endif
#include <map>

size_t gAddressToBreakOnAllocation = (size_t)-1;
size_t gAllocationCount = 0;
size_t gAllocationCountToBreakOn = (size_t)-1;
size_t gAddressToBreakOnFree = (size_t)-1;

struct MemTrack
{
  void *pMemory;
  size_t size;
  const char *pFile;
  int line;
  size_t allocationNumber;
};

typedef std::map<size_t, MemTrack> MemTrackMap;

static MemTrackMap *pMemoryTrackingMap;

static udMutex *pMemoryTrackingMutex;

// ----------------------------------------------------------------------------
// Author: David Ely
void udMemoryDebugTrackingInit()
{
  if (pMemoryTrackingMutex)
  {
    return;
  }
#if UDPLATFORM_LINUX || UDPLATFORM_NACL
  {
    pthread_mutex_t *mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    if (mutex)
    {
      pthread_mutex_init(mutex, NULL);
    }
    pMemoryTrackingMutex = (udMutex*)mutex;
  }
#else
  pMemoryTrackingMutex = udCreateMutex();
#endif
  if (!pMemoryTrackingMutex)
  {
    PRINT_ERROR_STRING("Failed to create memory tracking mutex\n");
  }

  if (!pMemoryTrackingMap)
  {
    pMemoryTrackingMap = new MemTrackMap;
  }
}

// ----------------------------------------------------------------------------
// Author: David Ely
void udMemoryDebugTrackingDeinit()
{
  UDASSERT(pMemoryTrackingMap, "pMemoryTrackingMap is NULL");

  udLockMutex(pMemoryTrackingMutex);

  delete pMemoryTrackingMap;
  pMemoryTrackingMap = NULL;

  udReleaseMutex(pMemoryTrackingMutex);

#if UDPLATFORM_LINUX || UDPLATFORM_NACL
  pthread_mutex_t *mutex = (pthread_mutex_t *)pMemoryTrackingMutex;
  pthread_mutex_destroy(mutex);
  free(pMemoryTrackingMutex);
  pMemoryTrackingMutex = nullptr;
#else
  udDestroyMutex(&pMemoryTrackingMutex);
#endif
}

// ----------------------------------------------------------------------------
// Author: David Ely
void udMemoryOutputLeaks()
{
  if (pMemoryTrackingMap)
  {
    udLockMutex(pMemoryTrackingMutex);

    if (pMemoryTrackingMap->size() > 0)
    {
      udDebugPrintf("%d Allocations\n", uint32_t(gAllocationCount));

      udDebugPrintf("%d Memory leaks detected\n", pMemoryTrackingMap->size());
      for (MemTrackMap::iterator memIt = pMemoryTrackingMap->begin(); memIt != pMemoryTrackingMap->end(); ++memIt)
      {
        const MemTrack &track = memIt->second;
        udDebugPrintf("%s(%d): Allocation 0x%p Address 0x%p, size %u\n", track.pFile, track.line, (void*)track.allocationNumber, track.pMemory, track.size);
      }
    }
    else
    {
      udDebugPrintf("All tracked allocations freed\n");
    }

    udReleaseMutex(pMemoryTrackingMutex);
  }
}

// ----------------------------------------------------------------------------
// Author: David Ely
void udMemoryOutputAllocInfo(void *pAlloc)
{
  udLockMutex(pMemoryTrackingMutex);

  const MemTrack &track = (*pMemoryTrackingMap)[size_t(pAlloc)];
  udDebugPrintf("%s(%d): Allocation 0x%p Address 0x%p, size %u\n", track.pFile, track.line, (void*)track.allocationNumber, track.pMemory, track.size);

  udReleaseMutex(pMemoryTrackingMutex);
}

static void DebugTrackMemoryAlloc(void *pMemory, size_t size, const char * pFile, int line)
{
  if (gAddressToBreakOnAllocation == (uint64_t)pMemory || gAllocationCount == gAllocationCountToBreakOn)
  {
    udDebugPrintf("Allocation 0x%p address 0x%p, at File %s, line %d", (void*)gAllocationCount, pMemory, pFile, line);
    __debugbreak();
  }

  if (!pMemoryTrackingMutex)
  {
    udMemoryDebugTrackingInit();
  }

  udLockMutex(pMemoryTrackingMutex);

#if UDASSERT_ON
  size_t sizeOfMap = pMemoryTrackingMap->size();
#endif
  MemTrack track = { pMemory, size, pFile, line, gAllocationCount };

  if (pMemoryTrackingMap->find(size_t(pMemory)) != pMemoryTrackingMap->end())
  {
    udDebugPrintf("Tracked allocation already exists %p at File %s, line %d", pMemory, pFile, line);
    __debugbreak();
  }

  (*pMemoryTrackingMap)[size_t(pMemory)] = track;

  ++gAllocationCount;

  UDASSERT(pMemoryTrackingMap->size() > sizeOfMap, "map didn't grow"); // I think this is incorrect as the map may not need to grow if its reusing a slot that has been freed.

  udReleaseMutex(pMemoryTrackingMutex);
}

// ----------------------------------------------------------------------------
// Author: David Ely
static void DebugTrackMemoryFree(void *pMemory, const char * pFile, int line)
{
# if UDASSERT_ON
  size_t sizeOfMap;
# endif


  if (gAddressToBreakOnFree == (uint64_t)pMemory)
  {
    udDebugPrintf("Allocation 0x%p address 0x%p, at File %s, line %d", (void*)gAllocationCount, pMemory, pFile, line);
    __debugbreak();
  }

  udLockMutex(pMemoryTrackingMutex);

  if (pMemoryTrackingMap)
  {
    MemTrackMap::iterator it = pMemoryTrackingMap->find(size_t(pMemory));
    if (it == pMemoryTrackingMap->end())
    {
      udDebugPrintf("Error freeing address %p at File %s, line %d, did not find a matching allocation", pMemory, pFile, line);
      //__debugbreak();
      goto epilogue;
    }
    UDASSERT(it->second.pMemory == (pMemory), "Pointers didn't match");

# if UDASSERT_ON
    sizeOfMap = pMemoryTrackingMap->size();
# endif
    pMemoryTrackingMap->erase(it);

    UDASSERT(pMemoryTrackingMap->size() < sizeOfMap, "map didn't shrink");
  }

epilogue:

 udReleaseMutex(pMemoryTrackingMutex);
}

void udMemoryDebugLogMemoryStats()
{
  udDebugPrintf("Memory Stats\n");

  size_t totalMemory = 0;
  for (MemTrackMap::iterator memIt = pMemoryTrackingMap->begin(); memIt != pMemoryTrackingMap->end(); ++memIt)
  {
    const MemTrack &track = memIt->second;
    totalMemory += track.size;
  }

  udDebugPrintf("Total allocated Memory %llu\n", totalMemory);
}

#else
# define DebugTrackMemoryAlloc(pMemory, size, pFile, line)
# define DebugTrackMemoryFree(pMemory, pFile, line)
#endif // __MEMORY_DEBUG__


#define UD_DEFAULT_ALIGNMENT (8)
// ----------------------------------------------------------------------------
// Author: David Ely
void *_udAlloc(size_t size, udAllocationFlags flags IF_MEMORY_DEBUG(const char * pFile, int line))
{
#if defined(_MSC_VER)
  void *pMemory = (flags & udAF_Zero) ? _aligned_recalloc(nullptr, size, 1, UD_DEFAULT_ALIGNMENT) : _aligned_malloc(size, UD_DEFAULT_ALIGNMENT);
#else
  void *pMemory = (flags & udAF_Zero) ? calloc(size, 1) : malloc(size);
#endif // defined(_MSC_VER)

  DebugTrackMemoryAlloc(pMemory, size, pFile, line);

#if __BREAK_ON_MEMORY_ALLOCATION_FAILURE
  if (!pMemory)
  {
    udDebugPrintf("udAlloc failure, %llu", size);
    __debugbreak();
  }
#endif // __BREAK_ON_MEMORY_ALLOCATION_FAILURE
  return pMemory;
}

// ----------------------------------------------------------------------------
// Author: David Ely
void *_udAllocAligned(size_t size, size_t alignment, udAllocationFlags flags IF_MEMORY_DEBUG(const char * pFile, int line))
{
#if defined(_MSC_VER)
  void *pMemory =  (flags & udAF_Zero) ? _aligned_recalloc(nullptr, size, 1, alignment) : _aligned_malloc(size, alignment);

#if __BREAK_ON_MEMORY_ALLOCATION_FAILURE
  if (!pMemory)
  {
    udDebugPrintf("udAllocAligned failure, %llu", size);
    __debugbreak();
  }
#endif // __BREAK_ON_MEMORY_ALLOCATION_FAILURE

#elif UDPLATFORM_NACL
  void *pMemory =  (flags & udAF_Zero) ? calloc(size, 1) : malloc(size);

#elif defined(__GNUC__)
  if (alignment < sizeof(size_t))
  {
    alignment = sizeof(size_t);
  }
  void *pMemory;
  int err = posix_memalign(&pMemory, alignment, size + alignment);
  if (err != 0)
  {
	  return nullptr;
  }

  if (flags & udAF_Zero)
  {
    memset(pMemory, 0, size);
  }
#endif
  DebugTrackMemoryAlloc(pMemory, size, pFile, line);

  return pMemory;
}

// ----------------------------------------------------------------------------
// Author: David Ely
void *_udRealloc(void *pMemory, size_t size IF_MEMORY_DEBUG(const char * pFile, int line))
{
#if __MEMORY_DEBUG__
  if (pMemory)
  {
    DebugTrackMemoryFree(pMemory, pFile, line);
  }
#endif
#if defined(_MSC_VER)
  pMemory =  _aligned_realloc(pMemory, size, UD_DEFAULT_ALIGNMENT);
#else
  pMemory = realloc(pMemory, size);
#endif // defined(_MSC_VER)

#if __BREAK_ON_MEMORY_ALLOCATION_FAILURE
  if (!pMemory)
  {
    udDebugPrintf("udRealloc failure, %llu", size);
    __debugbreak();
  }
#endif // __BREAK_ON_MEMORY_ALLOCATION_FAILURE
  DebugTrackMemoryAlloc(pMemory, size, pFile, line);


  return pMemory;
}

// ----------------------------------------------------------------------------
// Author: David Ely
void *_udReallocAligned(void *pMemory, size_t size, size_t alignment IF_MEMORY_DEBUG(const char * pFile, int line))
{
#if __MEMORY_DEBUG__
  if (pMemory)
  {
    DebugTrackMemoryFree(pMemory, pFile, line);
  }
#endif

#if defined(_MSC_VER)
  pMemory = _aligned_realloc(pMemory, size, alignment);
#if __BREAK_ON_MEMORY_ALLOCATION_FAILURE
  if (!pMemory)
  {
    udDebugPrintf("udReallocAligned failure, %llu", size);
    __debugbreak();
  }
#endif // __BREAK_ON_MEMORY_ALLOCATION_FAILURE
#elif UDPLATFORM_NACL
  pMemory = realloc(pMemory, size);
#elif defined(__GNUC__)
  if (!pMemory)
  {
    pMemory = udAllocAligned(size, alignment, udAF_None IF_MEMORY_DEBUG(pFile, line));
  }
  else
  {
    void *pNewMem = udAllocAligned(size, alignment, udAF_None IF_MEMORY_DEBUG(pFile, line));

    size_t *pSize = (size_t*)((uint8_t*)pMemory - sizeof(size_t));
    memcpy(pNewMem, pMemory, *pSize);
    udFree(pMemory IF_MEMORY_DEBUG(pFile, line));

    return pNewMem;
  }
#endif
  DebugTrackMemoryAlloc(pMemory, size, pFile, line);


  return pMemory;
}

// ----------------------------------------------------------------------------
// Author: David Ely
void _udFreeInternal(void * pMemory IF_MEMORY_DEBUG(const char * pFile, int line))
{
  if (pMemory)
  {
    DebugTrackMemoryFree(pMemory, pFile, line);
#if defined(_MSC_VER)
    _aligned_free(pMemory);
#else
    free(pMemory);
#endif // defined(_MSC_VER)
  }
}

// ----------------------------------------------------------------------------
// Author: David Ely
udResult udGetTotalPhysicalMemory(uint64_t *pTotalMemory)
{
#if UDPLATFORM_WINDOWS
  MEMORYSTATUSEX memorySatusEx;
  memorySatusEx.dwLength = sizeof(memorySatusEx);
  BOOL result = GlobalMemoryStatusEx(&memorySatusEx);
  if (result)
  {
    *pTotalMemory = memorySatusEx.ullTotalPhys;
    return udR_Success;
  }

  *pTotalMemory = 0;
  return udR_Failure_;

#elif UDPLATFORM_LINUX

// see http://nadeausoftware.com/articles/2012/09/c_c_tip_how_get_physical_memory_size_system for
// explanation.

#if !defined(_SC_PHYS_PAGES)
#error "_SC_PHYS_PAGES is not defined"
#endif

#if !defined(_SC_PAGESIZE)
#error "_SC_PAGESIZE is not defined"
#endif

  *pTotalMemory = (uint64_t)sysconf(_SC_PHYS_PAGES) * (uint64_t)sysconf(_SC_PAGESIZE);
  return udR_Success;

#else
  *pTotalMemory = 0;
  return udR_Success;
#endif
}
