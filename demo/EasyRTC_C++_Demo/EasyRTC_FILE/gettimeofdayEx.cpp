#include "gettimeofdayEx.h"

#if (defined(__WIN32__) || defined(_WIN32)) && !defined(__MINGW32__)
// For Windoze, we need to implement our own gettimeofday()
#include <windows.h>
// used to make sure that static variables in gettimeofday() aren't initialized simultaneously by multiple threads
static LONG initializeLock_gettimeofday = 0;

#if !defined(_WIN32_WCE)
#include <sys/timeb.h>
#endif

int gettimeofdayEx(struct timeval* tp, int* /*tz*/) {
    static LARGE_INTEGER tickFrequency, epochOffset;

    static bool isInitialized = false;

    LARGE_INTEGER tickNow;

#if !defined(_WIN32_WCE)
    QueryPerformanceCounter(&tickNow);
#else
    tickNow.QuadPart = GetTickCount();
#endif

    if (!isInitialized) {
        if (1 == InterlockedIncrement(&initializeLock_gettimeofday)) {

            // For our first call, use "ftime()", so that we get a time with a proper epoch.
            // For subsequent calls, use "QueryPerformanceCount()", because it's more fine-grain.
            struct timeb tb;
            ftime(&tb);
            tp->tv_sec = (long)tb.time;
            tp->tv_usec = 1000 * tb.millitm;

            // Also get our counter frequency:
            QueryPerformanceFrequency(&tickFrequency);

            // compute an offset to add to subsequent counter times, so we get a proper epoch:
            epochOffset.QuadPart
                = tp->tv_sec * tickFrequency.QuadPart + (tp->tv_usec * tickFrequency.QuadPart) / 1000000L - tickNow.QuadPart;

            // next caller can use ticks for time calculation
            isInitialized = true;
            return 0;
        }
        else {
            InterlockedDecrement(&initializeLock_gettimeofday);
            // wait until first caller has initialized static values
            while (!isInitialized) {
                timeBeginPeriod(1);
                Sleep(1);
                timeEndPeriod(1);
            }
        }
    }

    // adjust our tick count so that we get a proper epoch:
    tickNow.QuadPart += epochOffset.QuadPart;

    tp->tv_sec = (long)(tickNow.QuadPart / tickFrequency.QuadPart);
    tp->tv_usec = (long)(((tickNow.QuadPart % tickFrequency.QuadPart) * 1000000L) / tickFrequency.QuadPart);

    return 0;
}

#elif defined ANDROID
int gettimeofdayEx(struct timeval* tp, int* /*tz*/)
{
    return gettimeofday(tp, NULL);
}

#else

int gettimeofdayEx(struct timeval* tp, int* /*tz*/)
{
    return gettimeofday(tp, NULL);

    struct timespec monotonic_time;
    memset(&monotonic_time, 0, sizeof(monotonic_time));
    clock_gettime(CLOCK_MONOTONIC, &monotonic_time);
    if (NULL != tp)
    {
        tp->tv_sec = monotonic_time.tv_sec;
        tp->tv_usec = monotonic_time.tv_nsec / 1000;
    }

    return 0;
}

#endif
