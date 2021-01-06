#ifndef SHIELDRENDERPARAMETERS_H_INCLUDED
#define SHIELDRENDERPARAMETERS_H_INCLUDED

#include <cstdint>
#include "libs/vector3.h"

static const uint32_t MAX_SHIELD_HITS = 5;

struct ShieldRenderParameters {
	float strength;
	float coolDown;
	vector3f hitPos[MAX_SHIELD_HITS];
	float radii[MAX_SHIELD_HITS];
	uint32_t numHits;
};

#endif // SHIELDRENDERPARAMETERS_H_INCLUDED
