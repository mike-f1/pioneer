// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef SPAWNTASTYSTUFF_H_INCLUDED
#define SPAWNTASTYSTUFF_H_INCLUDED

#include "FrameId.h"
#include "libs/vector3.h"

class Body;
class SystemBody;

void MiningLaserSpawnTastyStuff(FrameId fId, Body *shooter, const SystemBody *asteroid, const vector3d &pos);

#endif // SPAWNTASTYSTUFF_H_INCLUDED
