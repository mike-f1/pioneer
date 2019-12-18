// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _INTRO_H
#define _INTRO_H

#include "Cutscene.h"
#include "scenegraph/ModelSkin.h"

namespace Background {
	class Container;
}

class Intro : public Cutscene {
public:
	Intro(int width, int height, float amountOfBackgroundStars);
	~Intro();
	virtual void Draw(float time);
	SceneGraph::Model *getCurrentModel() const { return m_model; }
	bool isZooming() const { return m_dist == m_zoomEnd; }

private:
	void Reset();
	bool m_needReset;

	std::vector<SceneGraph::Model *> m_models;
	SceneGraph::ModelSkin m_skin;

	float m_duration;

	unsigned int m_modelIndex;
	float m_zoomBegin, m_zoomEnd;
	float m_dist;

	std::unique_ptr<Background::Container> m_background;

	int m_spinnerLeft;
	int m_spinnerWidth;
	float m_spinnerRatio;
};

#endif
