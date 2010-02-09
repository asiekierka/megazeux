/*
 * This source code is public domain.
 *
 * Authors: Rani Assaf <rani@magic.metawire.com>,
 *          Olivier Lapicque <olivierl@jps.net>,
 *          Adam Goode       <adam@evdebs.org> (endian and char fixes for PPC)
 */

#ifndef _STDAFX_H_
#define _STDAFX_H_

/* Autoconf detection of stdint/inttypes */
#if defined(HAVE_CONFIG_H) && !defined(CONFIG_H_INCLUDED)
# include "config.h"
# define CONFIG_H_INCLUDED 1
#endif
#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif


#ifdef _WIN32

#ifdef MSC_VER
#pragma warning (disable:4201)
#pragma warning (disable:4514)
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <stdio.h>
#include <malloc.h>

#define srandom(_seed)  srand(_seed)
#define random()        rand()
#define sleep(_ms)      Sleep(_ms * 1000)

static inline void ProcessPlugins(int n) {}

#else

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#ifdef __amigaos__

/* On AmigaOS platforms we work around the fact that the toolchain headers
 * pollute the namespace with many MSVC-like type declarations. Using their
 * versions directly is not possible, as some are just plain wrong. WORD, for
 * example, is implicitly signed (not unsigned) as with the BYTE type. All
 * other toolchains do this differently, which ModPlug depends on internally.
 */
#define ULONG _ULONG
#define LONG _LONG
#define WORD _WORD
#define BYTE _BYTE
#define BOOL _BOOL

#else // !__amigaos__

/* On non-AmigaOS platforms we also define VOID; on AmigaOS we can
 * trust the VOID declaration and the same _VOID trick doesn't work.
 */
typedef void VOID;

#endif // __amigaos__

typedef int8_t CHAR;
typedef uint8_t UCHAR;
typedef uint8_t* PUCHAR;
typedef uint16_t USHORT;
typedef uint32_t ULONG;
typedef uint32_t UINT;
typedef uint32_t DWORD;
typedef int32_t LONG;
typedef int64_t LONGLONG;
typedef int32_t* LPLONG;
typedef uint32_t* LPDWORD;
typedef uint16_t WORD;
typedef uint8_t BYTE;
typedef uint8_t* LPBYTE;
typedef bool BOOL;
typedef char* LPSTR;
typedef void* LPVOID;
typedef uint16_t* LPWORD;
typedef const char* LPCSTR;
typedef void* PVOID;

static inline LONG MulDiv (long a, long b, long c)
{
  // if (!c) return 0;
  return ((uint64_t) a * (uint64_t) b ) / c;
}

#define MODPLUG_NO_FILESAVE
#define NO_AGC
#define LPCTSTR LPCSTR
#define lstrcpynA strncpy
#define lstrcpyA strcpy
#define lstrcmpA strcmp
#define WAVE_FORMAT_PCM 1
//#define ENABLE_EQ

#define  GHND   0

static inline int8_t * GlobalAllocPtr(unsigned int, size_t size)
{
  int8_t * p = (int8_t *) malloc(size);

  if (p != NULL) memset(p, 0, size);
  return p;
}

static inline void ProcessPlugins(int n) {}

#define GlobalFreePtr(p) free((void *)(p))

#define strnicmp(a,b,c)		strncasecmp(a,b,c)
#define wsprintfA		sprintf

#ifndef FALSE
#define FALSE	false
#endif

#ifndef TRUE
#define TRUE	true
#endif

#endif // _WIN32

#endif



