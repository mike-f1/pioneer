// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _INTRO_H
#define _INTRO_H

#include "Cutscene.h"

#include <memory>

namespace Background {
	class Container;
}

class Intro : public Cutscene {
public:
	Intro(int width, int height, float amountOfBackgroundStars);
	~Intro();
	virtual void Draw(float time);

private:
	float m_duration;

	std::unique_ptr<Background::Container> m_background;
};

#endif
