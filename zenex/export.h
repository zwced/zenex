#pragma once

#if defined(_WIN32) || defined(_WIN64)
    #if defined(ZENEX_STATIC)
        #define ZENEX_API
    #elif defined(ZENEX_BUILDING_DLL)
        #define ZENEX_API __declspec(dllexport)
    #else
        #define ZENEX_API __declspec(dllimport)
    #endif
#else
    #define ZENEX_API
#endif
