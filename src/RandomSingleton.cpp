// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "RandomSingleton.h"

#include "Random.h"

Random RandomSingleton::m_random;

void RandomSingleton::Init(const Uint32 seed)
{
	m_random.IncRefCount(); // so nothing tries to free it
	m_random.seed(seed);
}

