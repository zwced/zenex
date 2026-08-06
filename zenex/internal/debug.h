#pragma once

#if defined(ZENEX_DEBUG) && !defined(NDEBUG)
    #define ZDEBUG_ENABLED 1
#else
    #define ZDEBUG_ENABLED 0
#endif

#define ZDEBUG_METHOD void
