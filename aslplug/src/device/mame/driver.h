#pragma once

#ifndef __DRIVER_H__
#define __DRIVER_H__

#define HAS_YM3812 1
#define HAS_YM3526 0
#define HAS_Y8950  0
#define HAS_YMF262 1

#if defined(_MSC_VER)
#pragma warning(disable: 4244)
#pragma warning(disable: 4245)
#define INLINE static __inline
#elif defined(__BORLANDC__)
#define INLINE __inline
#elif defined(__GNUC__)
#define INLINE static inline // inline is not working for OS X
#else
#define INLINE static
#endif

#define logerror(x,y,z)
// typedef signed int stream_sample_t;

#endif	/* __DRIVER_H__ */
