#include "udResult.h"

// A simple interface to allow function calls to be easily made optionally background calls with one additional parameter

struct udAsyncJob;

// Create an async job handle
udResult udAsyncJob_Create(udAsyncJob **ppJobHandle);

// Get the result of an async job (wait on semaphore)
udResult udAsyncJob_GetResult(udAsyncJob *pJobHandle);

// Get the result of an async job (wait on semaphore for specified time)
// Returns true if job is complete (*pResult is set), false if timed out
bool udAsyncJob_GetResultTimeout(udAsyncJob *pJobHandle, udResult *pResult, int timeoutMs);

// Set the result (increment semaphore)
void udAsyncJob_SetResult(udAsyncJob *pJobHandle, udResult returnResult);

// Destroy the async job (destroy semaphore)
void udAsyncJob_Destroy(udAsyncJob **ppJobHandle);

// Some helper macros for boiler-plate code generation, each macro corresponds to number of parameters before pAsyncJob
// For these macros to work, udAsyncJob *pAsyncJob must be the LAST PARAMETER of the function

#define UDASYNC_CALL1(func, t0, p0) if (pAsyncJob) {                                                                    \
            struct UDAJParams { t0 _p0;                                                                                 \
                                udAsyncJob *pAsyncJob; } udajParams =  { p0, pAsyncJob };                               \
            udThreadStart *udajStartFunc = [](void *pData) -> unsigned                                                  \
            { UDAJParams *p = (UDAJParams*)pData;                                                                       \
              udAsyncJob_SetResult(p->pAsyncJob, func(p->_p0, nullptr));                                                \
              udFree(p);                                                                                                \
              return 0;                                                                                                 \
            };                                                                                                          \
            return udThread_Create(nullptr, udajStartFunc, udMemDup(&udajParams, sizeof(udajParams), 0, udAF_None));    \
        }
#define UDASYNC_CALL2(func, t0, p0, t1, p1) if (pAsyncJob) {                                                            \
            struct UDAJParams { t0 _p0; t1 _p1;                                                                         \
                                udAsyncJob *pAsyncJob; } udajParams =  { p0, p1, pAsyncJob };                           \
            udThreadStart *udajStartFunc = [](void *pData) -> unsigned                                                  \
            { UDAJParams *p = (UDAJParams*)pData;                                                                       \
              udAsyncJob_SetResult(p->pAsyncJob, func(p->_p0, p->_p1, nullptr));                                        \
              udFree(p);                                                                                                \
              return 0;                                                                                                 \
            };                                                                                                          \
            return udThread_Create(nullptr, udajStartFunc, udMemDup(&udajParams, sizeof(udajParams), 0, udAF_None));    \
        }
#define UDASYNC_CALL3(func, t0, p0, t1, p1, t2, p2) if (pAsyncJob) {                                                    \
            struct UDAJParams { t0 _p0; t1 _p1; t2 _p2;                                                                 \
                                udAsyncJob *pAsyncJob; } udajParams =  { p0, p1, p2, pAsyncJob };                       \
            udThreadStart *udajStartFunc = [](void *pData) -> unsigned                                                  \
            { UDAJParams *p = (UDAJParams*)pData;                                                                       \
              udAsyncJob_SetResult(p->pAsyncJob, func(p->_p0, p->_p1, p->_p2, nullptr));                                \
              udFree(p);                                                                                                \
              return 0;                                                                                                 \
            };                                                                                                          \
            return udThread_Create(nullptr, udajStartFunc, udMemDup(&udajParams, sizeof(udajParams), 0, udAF_None));    \
        }
#define UDASYNC_CALL4(func, t0, p0, t1, p1, t2, p2, t3, p3) if (pAsyncJob) {                                            \
            struct UDAJParams { t0 _p0; t1 _p1; t2 _p2; t3 _p3;                                                         \
                                udAsyncJob *pAsyncJob; } udajParams =  { p0, p1, p2, p3, pAsyncJob };                   \
            udThreadStart *udajStartFunc = [](void *pData) -> unsigned                                                  \
            { UDAJParams *p = (UDAJParams*)pData;                                                                       \
              udAsyncJob_SetResult(p->pAsyncJob, func(p->_p0, p->_p1, p->_p2, p->_p3, nullptr));                        \
              udFree(p);                                                                                                \
              return 0;                                                                                                 \
            };                                                                                                          \
            return udThread_Create(nullptr, udajStartFunc, udMemDup(&udajParams, sizeof(udajParams), 0, udAF_None));    \
        }
#define UDASYNC_CALL5(func, t0, p0, t1, p1, t2, p2, t3, p3, t4, p4) if (pAsyncJob) {                                    \
            struct UDAJParams { t0 _p0; t1 _p1; t2 _p2; t3 _p3; t4 _p4;                                                 \
                                udAsyncJob *pAsyncJob; } udajParams =  { p0, p1, p2, p3, p4, pAsyncJob };               \
            udThreadStart *udajStartFunc = [](void *pData) -> unsigned                                                  \
            { UDAJParams *p = (UDAJParams*)pData;                                                                       \
              udAsyncJob_SetResult(p->pAsyncJob, func(p->_p0, p->_p1, p->_p2, p->_p3, p->_p4, nullptr));                \
              udFree(p);                                                                                                \
              return 0;                                                                                                 \
            };                                                                                                          \
            return udThread_Create(nullptr, udajStartFunc, udMemDup(&udajParams, sizeof(udajParams), 0, udAF_None));    \
        }
#define UDASYNC_CALL6(func, t0, p0, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5) if (pAsyncJob) {                            \
            struct UDAJParams { t0 _p0; t1 _p1; t2 _p2; t3 _p3; t4 _p4; t5 _p5;                                         \
                                udAsyncJob *pAsyncJob; } udajParams =  { p0, p1, p2, p3, p4, p5, pAsyncJob };           \
            udThreadStart *udajStartFunc = [](void *pData) -> unsigned                                                  \
            { UDAJParams *p = (UDAJParams*)pData;                                                                       \
              udAsyncJob_SetResult(p->pAsyncJob, func(p->_p0, p->_p1, p->_p2, p->_p3, p->_p4, p->_p5, nullptr));        \
              udFree(p);                                                                                                \
              return 0;                                                                                                 \
            };                                                                                                          \
            return udThread_Create(nullptr, udajStartFunc, udMemDup(&udajParams, sizeof(udajParams), 0, udAF_None));    \
        }
#define UDASYNC_CALL7(func, t0, p0, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6) if (pAsyncJob) {                    \
            struct UDAJParams { t0 _p0; t1 _p1; t2 _p2; t3 _p3; t4 _p4; t5 _p5; t6 _p6;                                 \
                                udAsyncJob *pAsyncJob; } udajParams =  { p0, p1, p2, p3, p4, p5, p6, pAsyncJob };       \
            udThreadStart *udajStartFunc = [](void *pData) -> unsigned                                                  \
            { UDAJParams *p = (UDAJParams*)pData;                                                                       \
              udAsyncJob_SetResult(p->pAsyncJob, func(p->_p0, p->_p1, p->_p2, p->_p3, p->_p4, p->_p5, p->_p6, nullptr));\
              udFree(p);                                                                                                \
              return 0;                                                                                                 \
            };                                                                                                          \
            return udThread_Create(nullptr, udajStartFunc, udMemDup(&udajParams, sizeof(udajParams), 0, udAF_None));    \
        }
