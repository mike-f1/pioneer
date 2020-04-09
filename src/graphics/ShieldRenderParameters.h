#ifndef SHIELDRENDERPARAMETERS_H_INCLUDED
#define SHIELDRENDERPARAMETERS_H_INCLUDED

#include <SDL_stdinc.h>
#include "vector3.h"

static const Uint32 MAX_SHIELD_HITS = 5;

struct ShieldRenderParameters {
	float strength;
	float coolDown;
	vector3f hitPos[MAX_SHIELD_HITS];
	float radii[MAX_SHIELD_HITS];
	Sint32 numHits;
};

#endif // SHIELDRENDERPARAMETERS_H_INCLUDED
