// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _UTILS_H
#define _UTILS_H

#if defined(_MSC_VER) && !defined(NOMINMAX)
#define NOMINMAX
#endif

#include "libs.h"
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <vector>

#ifndef __GNUC__
#define __attribute(x)
#endif /* __GNUC__ */

// GCC warns when a function marked __attribute((noreturn)) actually returns a value
// but other compilers which don't see the noreturn attribute of course require that
// a function with a non-void return type should return something.
#ifndef __GNUC__
#define RETURN_ZERO_NONGNU_ONLY return 0;
#else
#define RETURN_ZERO_NONGNU_ONLY
#endif

// align x to a. taken from the Linux kernel
#define ALIGN(x, a) __ALIGN_MASK(x, (a - 1))
#define __ALIGN_MASK(x, mask) (((x) + (mask)) & ~(mask))

void Error(const char *format, ...) __attribute((format(printf, 1, 2))) __attribute((noreturn));
void Warning(const char *format, ...) __attribute((format(printf, 1, 2)));
void Output(const char *format, ...) __attribute((format(printf, 1, 2)));
void OpenGLDebugMsg(const char *format, ...) __attribute((format(printf, 1, 2)));

template <class T>
inline const T &Clamp(const T &x, const T &min, const T &max) { return x > max ? max : (x < min ? min : x); }

constexpr double _deg2radFactor = M_PI / 180.;
constexpr double _rad2degFactor = 180 / M_PI;

constexpr double DEG2RAD(double x) { return x * _deg2radFactor; }
constexpr float DEG2RAD(float x) { return x * float(_deg2radFactor); }
constexpr double RAD2DEG(double x) { return x * _rad2degFactor; }
constexpr float RAD2DEG(float x) { return x * float(_rad2degFactor); }

// from StackOverflow: http://stackoverflow.com/a/1500517/52251
// Q: "Compile time sizeof_array without using a macro"
template <typename T, size_t N>
char (&COUNTOF_Helper(T (&array)[N]))[N];
#define COUNTOF(array) (sizeof(COUNTOF_Helper(array)))

// Helper for timing functions with multiple stages
// Used on a branch to help time loading.
struct MsgTimer {
	MsgTimer() { mTimer.Start(); }
	~MsgTimer() {}

	void Mark(const char *identifier)
	{
		mTimer.SoftStop();
		const double lastTiming = mTimer.avgms();
		mTimer.SoftReset();
		Output("(%lf) avgms in %s\n", lastTiming, identifier);
	}

protected:
	Profiler::Timer mTimer;
};

static inline int64_t isqrt(int64_t a)
{
	// replace with cast from sqrt below which is between x7.3 (win32, Debug) & x15 (x64, Release) times faster
	return static_cast<int64_t>(sqrt(static_cast<double>(a)));
}

// add a few things that MSVC is missing
#if defined(_MSC_VER) && (_MSC_VER < 1800)

// round & roundf. taken from http://cgit.freedesktop.org/mesa/mesa/tree/src/gallium/auxiliary/util/u_math.h
static inline double round(double x)
{
	return x >= 0.0 ? floor(x + 0.5) : ceil(x - 0.5);
}

static inline float roundf(float x)
{
	return x >= 0.0f ? floorf(x + 0.5f) : ceilf(x - 0.5f);
}
#endif /* _MSC_VER < 1800 */

static inline uint32_t ceil_pow2(uint32_t v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v;
}

void hexdump(const unsigned char *buf, int len);

#endif /* _UTILS_H */
