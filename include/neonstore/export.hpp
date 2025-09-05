// SPDX-License-Identifier: Apache-2.0
#pragma once
#if defined(_WIN32) || defined(__CYGWIN__)
  #if defined(NEONSTORE_BUILD_SHARED)
    #define NEONSTORE_API __declspec(dllexport)
  #elif defined(NEONSTORE_USE_SHARED)
    #define NEONSTORE_API __declspec(dllimport)
  #else
    #define NEONSTORE_API
  #endif
#else
  #if defined(NEONSTORE_BUILD_SHARED)
    #define NEONSTORE_API __attribute__((visibility("default")))
  #else
    #define NEONSTORE_API
  #endif
#endif
