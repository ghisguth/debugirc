/* common.hpp
 * This file is a part of debugirc library
 * Copyright (c) debugirc authors (see file `COPYRIGHT` for the license)
 */

#pragma once

#if defined(__WIN32__) || defined(WIN32) || defined(_WIN32)
#  define PLATFORM_WIN32
#else
#  define PLATFORM_LINUX
#  if defined(__APPLE__)
#    define PLATFORM_OSX
#  endif
#endif

#if defined(PLATFORM_WIN32)
#include "common_win32.hpp"
#endif

#if defined(PLATFORM_LINUX)
#include "common_linux.hpp"
#endif

#if defined(PLATFORM_WIN32)
#  define PLATFORM_NAME "win32"
#else
#  if defined(PLATFORM_OSX)
#    define PLATFORM_NAME "osx"
#  else
#    define PLATFORM_NAME "linux"
#  endif
#endif

#include "config.hpp"
