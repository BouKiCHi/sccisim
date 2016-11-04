#ifndef NESTYPES_H__
#define NESTYPES_H__

// システムのヘッダが優先される
#ifdef WIN32
#include <malloc.h>
#include <memory.h>
#else
#include <stdlib.h>
#include <string.h>
#endif

#ifndef WIN32

#include <stdlib.h>
#define XSLEEP(t)
//#define XSLEEP(t)		_sleep(t)

#else

// #include <windows.h>
#include <stdlib.h>

#undef THIS_ // for MinGW32
#define XSLEEP(t)
#endif

#define XMALLOC(s)		malloc(s)
#define XREALLOC(p,s)	realloc(p,s)
#define XFREE(p)		free(p)
#define XMEMCPY(d,s,n)	memcpy(d,s,n)
#define XMEMSET(d,c,n)	memset(d,c,n)

#if defined(_MSC_VER)
#define NEVER_REACH __assume(0);
#elif defined(__BORLANDC__)
#define __fastcall __msfastcall
#elif defined(__GNUC__)
#ifndef __inline
#define __inline		__inline__
#endif
#ifndef __fastcall
#define __fastcall
#endif
#else
#define __inline
#define __fastcall
#endif
#ifndef NEVER_REACH
#define NEVER_REACH
#endif

typedef int				Int;
typedef unsigned int	Uint;
typedef signed int		Int32;
typedef signed long		Int64;
#ifndef Uint32
#define Uint32 unsigned int
#endif
#ifndef Uint8
#define Uint8 unsigned char
#endif
typedef signed short	Int16;
typedef signed char		Int8;
typedef char			Char;

#ifndef USE_SDL
typedef unsigned long	Uint64;
typedef unsigned short	Uint16;
#endif

#endif /* NESTYPES_H__ */

