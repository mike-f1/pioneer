#ifndef BASESPHEREDEBUGFLAGS_H_INCLUDED
#define BASESPHEREDEBUGFLAGS_H_INCLUDED

#include "libs/bitmask_op.h"

enum class GSDebugFlags {
	NONE = 0x0,
	SINGLE_PATCH_BBOX = 0x1,
	NEAR_PATCHES_BBOX = 0x2,
	ALL_PATCHES_BBOX = 0x3,
	BOUNDING_SPHERE = 0x4,
};

template<>
struct enable_bitmask_operators<GSDebugFlags> {
    static constexpr bool enable = true;
};

#endif // BASESPHEREDEBUGFLAGS_H_INCLUDED
