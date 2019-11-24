// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef RANDOMSINGLETON_H
#define RANDOMSINGLETON_H

#include "RefCounted.h"
#include "SDL_stdinc.h"

class Random;

class RandomSingleton
{
public:
	RandomSingleton() = delete;

	static void Init(const Uint32 seed);

	static Random &getInstance() { return m_random; };

private:
	static Random m_random;
};

#endif // RANDOMSINGLETON_H
