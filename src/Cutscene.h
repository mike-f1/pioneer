// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _CUTSCENE_H
#define _CUTSCENE_H

#include "Color.h"
#include <vector>

namespace Graphics {
	class Light;
}

namespace SceneGraph {
	class Model;
}

class Shields;

class Cutscene {
public:
	Cutscene(int width, int height) :
		m_aspectRatio(float(width) / float(height))
	{
	}
	virtual ~Cutscene() {}

	virtual void Draw(float time) = 0;

protected:
	Color m_ambientColor;
	float m_aspectRatio;
	SceneGraph::Model *m_model;
	Shields *m_shield;
	std::vector<Graphics::Light> m_lights;
};

#endif
