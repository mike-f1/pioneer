// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Intro.h"

#include "Background.h"
#include "RandomSingleton.h"
#include "graphics/Drawables.h"
#include "graphics/Renderer.h"
#include "graphics/RendererLocator.h"

Intro::Intro(int width, int height, float amountOfBackgroundStars) :
	Cutscene(width, height)
{
	m_background.reset(new Background::Container(RandomSingleton::getInstance(), amountOfBackgroundStars));

	m_duration = 0.;
}

Intro::~Intro()
{
}

void Intro::Draw(float _time)
{
	m_duration += _time;

	Graphics::Renderer::StateTicket ticket(RendererLocator::getRenderer());

	RendererLocator::getRenderer()->SetPerspectiveProjection(75, m_aspectRatio, 1.f, 10000.f);
	RendererLocator::getRenderer()->SetTransform(matrix4x4f::Identity());

	// XXX all this stuff will be gone when intro uses a Camera
	// rotate background by time, and a bit extra Z so it's not so flat
	matrix4x4d brot = matrix4x4d::RotateXMatrix(-0.25 * m_duration) * matrix4x4d::RotateZMatrix(0.6);
	RendererLocator::getRenderer()->ClearDepthBuffer();
	m_background->Draw(brot);
}
