/* common_windows.hpp
 * This file is a part of debugirc library
 * Copyright (c) debugirc authors (see file `COPYRIGHT` for the license)
 */

#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef STRICT
#define STRICT
#endif  //  STRICT

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef WINVER
#define WINVER 0x0400
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400
#endif

#include <windows.h>

#define snprintf _snprintf

typedef __int8 int8_t;
typedef __int16 int16_t;
typedef __int32 int32_t;
typedef __int64 int64_t;
typedef unsigned __int8 uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;

inline time_t FileTimeToTimeT (FILETIME uft)
{
	static int init = 0;
	static LARGE_INTEGER utc_base_li;
	static FILETIME utc_base_ft;
	static long double utc_base;

	time_t ret;
#ifndef MAXLONGLONG
	SYSTEMTIME st;
	struct tm t;
	FILETIME ft;
	TIME_ZONE_INFORMATION tzi;
	DWORD tzid;
#else
	LARGE_INTEGER lft;
#endif

	if (!init)
	{
		/* Determine the delta between 1-Jan-1601 and 1-Jan-1970. */
		SYSTEMTIME st;

		st.wYear = 1970;
		st.wMonth = 1;
		st.wDay = 1;
		st.wHour = 0;
		st.wMinute = 0;
		st.wSecond = 0;
		st.wMilliseconds = 0;

		SystemTimeToFileTime (&st, &utc_base_ft);

		utc_base_li.LowPart = utc_base_ft.dwLowDateTime;
		utc_base_li.HighPart = utc_base_ft.dwHighDateTime;

		init = 1;
	}

#ifdef MAXLONGLONG

	/* On a compiler that supports long integers, do it the easy way */
	lft.LowPart = uft.dwLowDateTime;
	lft.HighPart = uft.dwHighDateTime;
	ret = (time_t) ((lft.QuadPart - utc_base_li.QuadPart) / 10000000);

#else

	/* Do it the hard way using mktime. */
	FileTimeToLocalFileTime(&uft, &ft);
	FileTimeToSystemTime (&ft, &st);
	tzid = GetTimeZoneInformation (&tzi);
	t.tm_year = st.wYear - 1900;
	t.tm_mon = st.wMonth - 1;
	t.tm_mday = st.wDay;
	t.tm_hour = st.wHour;
	t.tm_min = st.wMinute;
	t.tm_sec = st.wSecond;
	t.tm_isdst = (tzid == TIME_ZONE_ID_DAYLIGHT);
	/* st.wMilliseconds not applicable */
	ret = mktime(&t);
	if (ret == -1)
	{
		ret = 0;
	}

#endif

	return ret;
}
