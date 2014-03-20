#ifndef UDPLATFORM_H
#define UDPLATFORM_H

#include "udResult.h"

// This must be set on all platforms for large files to work
#if (_FILE_OFFSET_BITS != 64)
#error "_FILE_OFFSET_BITS not defined to 64"
#endif

#if defined(_MSC_VER)
# define _CRT_SECURE_NO_WARNINGS
# define fseeko _fseeki64
# define ftello _ftelli64
# if !defined(_OFF_T_DEFINED)
    typedef __int64 _off_t;
    typedef _off_t off_t;
#   define _OFF_T_DEFINED
# endif //_OFF_T_DEFINED
#elif defined(__linux__)
# if !defined(_LARGEFILE_SOURCE )
  // This must be set for linux to expose fseeko and ftello 
# error "_LARGEFILE_SOURCE  not defined"
#endif

#endif



// An abstraction layer for common functions that differ on various platforms
#include <stdint.h>

#if defined(_WIN64) || defined(__amd64__)
  //64-bit code
# define UD_64BIT
# define UD_WORD_SHIFT  6   // 6 bits for 64 bit pointer
# define UD_WORD_BITS   64
# define UD_WORD_BYTES  8
# define UD_WORD_MAX    0x7fffffffffffffffLL
  typedef signed long long udIWord;
  typedef unsigned long long udUWord;
#elif  defined(_WIN32) || defined(__i386__)  || defined(__arm__)
   //32-bit code
# define UD_32BIT
# define UD_WORD_SHIFT  5   // 5 bits for 32 bit pointer  
# define UD_WORD_BITS   32
# define UD_WORD_BYTES  4
# define UD_WORD_MAX    0x7fffffffL
  typedef signed long udIWord;
  typedef unsigned long udUWord;
#else
# error "Unknown architecture (32/64 bit)"
#endif

#if defined(_MSC_VER) || defined(__MINGW32__)
# define UDPLATFORM_WINDOWS (1)
# define UDPLATFORM_LINUX   (0)
#elif defined(__linux__) // TODO: Work out best tag to detect linux here
# define UDPLATFORM_WINDOWS (0)
# define UDPLATFORM_LINUX   (1)
#else
# define UDPLATFORM_WINDOWS (0)
# define UDPLATFORM_LINUX   (0)
# error "Unknown platform"
#endif

#if defined(_DEBUG)
# define UD_DEBUG   (1)
# define UD_RELEASE (0)
#else
# define UD_DEBUG   (0)
# define UD_RELEASE (1)
#endif


#if UDPLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif
#include <Intrin.h>
inline int32_t udInterlockedPreIncrement(int32_t *p)  { return _InterlockedIncrement((long*)p); }
inline int32_t udInterlockedPostIncrement(int32_t *p) { return _InterlockedIncrement((long*)p) - 1; }
inline int32_t udInterlockedPreDecrement(int32_t *p) { return _InterlockedDecrement((long*)p); }
inline int32_t udInterlockedPostDecrement(int32_t *p) { return _InterlockedDecrement((long*)p) + 1; }
inline int32_t udInterlockedCompareExchange(volatile int32_t *dest, int32_t exchange, int32_t comparand) { return _InterlockedCompareExchange((volatile long*)dest, exchange, comparand); }
# if defined(UD_32BIT)
inline void *udInterlockedCompareExchangePointer(void ** volatile dest, void *exchange, void *comparand) { return (void*)_InterlockedCompareExchange((volatile long *)dest, (long)exchange, (long)comparand); }
# else // defined(UD_32BIT)
inline void *udInterlockedCompareExchangePointer(void ** volatile dest, void *exchange, void *comparand) { return _InterlockedCompareExchangePointer(dest, exchange, comparand); }
# endif // defined(UD_32BIT)
# define udSleep(x) Sleep(x)

#elif UDPLATFORM_LINUX
inline long udInterlockedPreIncrement(int32_t *p)  { return __sync_add_and_fetch(p, 1); }
inline long udInterlockedPostIncrement(int32_t *p) { return __sync_fetch_and_add(p, 1); }
inline long udInterlockedPreDecrement(int32_t *p)  { return __sync_add_and_fetch(p, -1); }
inline long udInterlockedPostDecrement(int32_t *p) { return __sync_fetch_and_add(p, -1); }
inline long udInterlockedCompareExchange(volatile int32_t *dest, int32_t exchange, int32_t comparand) { return __sync_val_compare_and_swap(dest, comparand, exchange); }
inline void *udInterlockedCompareExchangePointer(void ** volatile dest, void *exchange, void *comparand) { return __sync_val_compare_and_swap(dest, comparand, exchange); }
# define udSleep(x) usleep((x)*1000)

#else
#error Unknown platform
#endif

// TODO: Consider wrapping instead of implementing psuedo-c++11 interfaces
// Using c++11 ATOMIC library, so for MSVC versions not supporting this provide a minimal implementation
#if (defined(_MSC_VER) && (_MSC_VER <= 1600)) || defined(__MINGW32__)// Visual studio 2010 (VC110) and below
// Define a subset of std::atomic specifically to meet exactly the needs of udRender's use
#include <windows.h>

namespace std
{
  class atomic_long
  {
  public:
    operator long() { return member; }
    long operator--() { return InterlockedDecrement(&member); }
    long operator--(int) { return InterlockedDecrement(&member) + 1; }
    long operator++() { return InterlockedIncrement(&member); }
    long operator++(int) { return InterlockedIncrement(&member) - 1; }
    void operator=(long v) { member = v; }
  protected:
    volatile long member;
  };

  template <typename T>
  class atomic
  {
  public:
    operator T() { return member; }
    void operator =(const T &value) { member = value; } // TODO: Check there's no better Interlocked way to do this
    bool compare_exchange_weak(T &expected, T desired) { T actual = _InterlockedCompareExchangePointer((void*volatile*)&member, desired, expected); if (actual == expected) return true; expected = actual; return false; }
  protected:
    T member;
  };


  class mutex
  {
  public:
    mutex() { handle = CreateMutex(NULL, FALSE, NULL); }
    ~mutex() { CloseHandle(handle); }
    void lock() { WaitForSingleObject(handle, INFINITE); }
    bool try_lock() { return WaitForSingleObject(handle, 0) == WAIT_OBJECT_0; }
    void unlock() { ReleaseMutex(handle); }
  protected:
    HANDLE handle;
  };
};

#else

#include <thread>
#include <atomic>
#include <mutex>

#endif

// Minimalist MOST BASIC cross-platform thread support
struct udSemaphore;
struct udMutex;
typedef uint64_t udThreadHandle;

typedef uint32_t (udThreadStart)(void *data);
udThreadHandle udCreateThread(udThreadStart *threadStarter, void *threadData); // Returns thread handle
void udDestroyThread(udThreadHandle threadHandle);

udSemaphore *udCreateSemaphore(int maxValue, int initialValue);
void udDestroySemaphore(udSemaphore **ppSemaphore);
void udIncrementSemaphore(udSemaphore *pSemaphore);
void udWaitSemaphore(udSemaphore *pSemaphore);

udMutex *udCreateMutex();
void udDestroyMutex(udMutex **ppMutex);
void udLockMutex(udMutex *pMutex);
void udReleaseMutex(udMutex *pMutex);

// A convenience class to lock and unlock based on scope of the variable
class udScopeLock
{
public:
  udScopeLock(udMutex *mutex) { m_mutex = mutex; udLockMutex(m_mutex); }
  ~udScopeLock() { udReleaseMutex(m_mutex); }
protected:
  udMutex *m_mutex;
};

#define UDALIGN_POWEROF2(x,b) (((x)+(b)-1) & -(b))

#define __MEMORY_DEBUG__ (0)

#if __MEMORY_DEBUG__
# define IF_MEMORY_DEBUG(x,y) x,y
#else 
# define IF_MEMORY_DEBUG(x,y) 
#endif //  __MEMORY_DEBUG__

void *_udAlloc(size_t size IF_MEMORY_DEBUG(,const char * pFile ) IF_MEMORY_DEBUG(,int  line));
void *_udAllocAligned(size_t size, size_t alignment IF_MEMORY_DEBUG(,const char * pFile ) IF_MEMORY_DEBUG(,int  line));

void *_udRealloc(void *pMemory, size_t size IF_MEMORY_DEBUG(,const char * pFile ) IF_MEMORY_DEBUG(,int  line));
void *_udReallocAligned(void *pMemory, size_t size, size_t alignment IF_MEMORY_DEBUG(,const char * pFile ) IF_MEMORY_DEBUG(,int  line));

void _udFree(void **pMemory IF_MEMORY_DEBUG(,const char * pFile ) IF_MEMORY_DEBUG(,int  line));


enum udMemoryOverload
{
  UDMO_Memory
};

void *operator new (size_t size, udMemoryOverload memoryOverload IF_MEMORY_DEBUG(,const char * pFile ) IF_MEMORY_DEBUG(,int  line));
void *operator new[] (size_t size, udMemoryOverload memoryOverload IF_MEMORY_DEBUG(,const char * pFile ) IF_MEMORY_DEBUG(,int  line));

void operator delete (void *p, udMemoryOverload memoryOverload IF_MEMORY_DEBUG(,const char * pFile ) IF_MEMORY_DEBUG(,int  line));
void operator delete [](void *p, udMemoryOverload memoryOverload IF_MEMORY_DEBUG(,const char * pFile ) IF_MEMORY_DEBUG(,int  line));

template <typename T> void _udDelete(T *&pMemory, udMemoryOverload memoryOverload IF_MEMORY_DEBUG(,const char * pFile ) IF_MEMORY_DEBUG(,int  line)) { if (pMemory) { pMemory->~T(); operator delete (pMemory, memoryOverload IF_MEMORY_DEBUG(, pFile) IF_MEMORY_DEBUG(, line)); pMemory = NULL; } }
template <typename T> void _udDeleteArray(T *&pMemory, udMemoryOverload memoryOverload IF_MEMORY_DEBUG(,const char * pFile ) IF_MEMORY_DEBUG(,int  line)) { if (pMemory) { /*pMemory->~T(); */operator delete [] (pMemory, memoryOverload IF_MEMORY_DEBUG(,pFile) IF_MEMORY_DEBUG(, line)); pMemory = NULL; }}


#if __MEMORY_DEBUG__
#  define udAlloc(size) _udAlloc(size, __FILE__, __LINE__)
#  define udAllocType(type) (type*)_udAlloc(sizeof(type), __FILE__, __LINE__)
#  define udAllocAligned(size, alignment) _udAllocAligned(size, alignment, __FILE__, __LINE__)

#  define udRealloc(pMemory, size) _udRealloc(pMemory, size, __FILE__, __LINE__)
#  define udReallocAligned(size, alignment) _udReallocAligned(size, alignment, __FILE__, __LINE__)

#  define udFree(pMemory) _udFree((void**)&pMemory, __FILE__, __LINE__)

#  define udNew new (UDMO_Memory, __FILE__, __LINE__)
#  define udNewArray new [] (UDMO_Memory, __FILE__, __LINE__)

#  define udDelete(pMemory) _udDelete(pMemory, UDMO_Memory, __FILE__, __LINE__)
#  define udDeleteArray(pMemory) _udDeleteArray(pMemory, UDMO_Memory, __FILE__, __LINE__)

#else //  __MEMORY_DEBUG__
#  define udAlloc(size) _udAlloc(size)
#  define udAllocType(type) (type*)_udAlloc(sizeof(type))
#  define udAllocAligned(size, alignment) _udAllocAligned(size, alignment)

#  define udRealloc(pMemory, size) _udRealloc(pMemory, size)
#  define udReallocAligned(size, alignment) _udReallocAligned(size, alignment)

#  define udFree(pMemory) _udFree((void**)&pMemory)

#  define udNew new (UDMO_Memory)
#  define udNewArray new [] (UDMO_Memory)

#  define udDelete(pMemory) _udDelete(pMemory, UDMO_Memory)
#  define udDeleteArray(pMemory) _udDeleteArray(pMemory, UDMO_Memory)

#endif  //  __MEMORY_DEBUG__

#if __MEMORY_DEBUG__
void udMemoryDebugTrackingInit();
void udMemoryOutputLeaks();
void udMemoryOutputAllocInfo(void *pAlloc);
void udMemoryDebugTrackingDeinit();
#else
# define udMemoryDebugTrackingInit()
# define udMemoryOutputLeaks()
#define udMemoryOutputAllocInfo(pAlloc)
#define udMemoryDebugTrackingDeinit()
#endif // __MEMORY_DEBUG__

#if UDPLATFORM_WINDOWS
# define udMemoryBarrier() MemoryBarrier()
#else
# define udMemoryBarrier() __sync_synchronize()
#endif

#if defined(__GNUC__)
# define udUnusedVar(x) __attribute__((__unused__))x
#elif defined(_WIN32)
# define udUnusedVar(x) (void)x
#else 
# define udUnusedVar(x) x
#endif

#if defined(__GNUC__)
# define udUnusedParam(x) __attribute__((__unused__))x
#elif defined(_WIN32)
# define udUnusedParam(x) 
#else 
# define udUnusedParam(x) 
#endif


#if defined(_MSC_VER)
# define __FUNC_NAME__ __FUNCTION__
#elif defined(__GNUC__)
# define __FUNC_NAME__ __PRETTY_FUNCTION__
#else
#pragma message ("This platform hasn't setup up __FUNC_NAME__")
# define __FUNC_NAME__ "unknown"
#endif


// Disabled Warnings
#if defined(_MSC_VER)
#pragma warning(disable:4127) // conditional expression is constant
#endif //defined(_MSC_VER)



#include "udDebug.h"

#endif // UDPLATFORM_H
