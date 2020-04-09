// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _LIBS_H
#define _LIBS_H

#include <SDL.h>
#include <SDL_image.h>
#include <sigc++/sigc++.h>
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cfloat>
#include <cinttypes>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
#include <malloc.h>

#ifdef _MSC_VER
#pragma warning(disable : 4244) // "conversion from x to x: possible loss of data"
#pragma warning(disable : 4800) // int-to-bool "performance warning"
#pragma warning(disable : 4355) // 'this' used in base member initializer list
#pragma warning(disable : 4351) // new behavior [after vs2003!]: elements of array 'array' will be default initialized
#endif

#ifndef __MINGW32__
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif
#endif

#ifdef _WIN32 // MSVC doesn't support the %z specifier, but has its own %I specifier
#define SIZET_FMT "%Iu"
#else
#define SIZET_FMT "%zu"
#endif

#include "fixed.h"
#include "matrix3x3.h"
#include "matrix4x4.h"
#include "vector2.h"
#include "vector3.h"

#include "Aabb.h"
#include "Color.h"
#include "Random.h"

#include "FloatComparison.h"
#include "RefCounted.h"
#include "SmartPtr.h"

#include "profiler/Profiler.h"

#ifdef NDEBUG
#define PiVerify(x) ((void)(x))
#else
#define PiVerify(x) assert(x)
#endif

#endif /* _LIBS_H */
