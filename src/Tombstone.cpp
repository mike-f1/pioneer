// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Tombstone.h"

#include "Lang.h"
#include "ModelCache.h"
#include "graphics/Renderer.h"
#include "graphics/RendererLocator.h"
#include "scenegraph/SceneGraph.h"

Tombstone::Tombstone(int width, int height) :
	Cutscene(width, height)
{
	m_ambientColor = Color(13, 13, 26, 255);

	const Color lc(255, 255, 255, 0);
	m_lights.push_back(Graphics::Light(Graphics::Light::LIGHT_DIRECTIONAL, vector3f(0.f, 0.8f, 1.0f), lc, lc));

	m_model = ModelCache::FindModel("tombstone");
	m_model->SetLabel(Lang::TOMBSTONE_EPITAPH);
	const Uint32 numMats = m_model->GetNumMaterials();
	for (Uint32 m = 0; m < numMats; m++) {
		RefCountedPtr<Graphics::Material> mat = m_model->GetMaterialByIndex(m);
		mat->specialParameter0 = nullptr;
	}
	m_total_time = 0.0;
}

void Tombstone::Draw(float _time)
{
	m_total_time += _time;

	RendererLocator::getRenderer()->SetClearColor(Color::BLACK);
	RendererLocator::getRenderer()->ClearScreen();

	RendererLocator::getRenderer()->SetPerspectiveProjection(75, m_aspectRatio, 1.f, 10000.f);
	RendererLocator::getRenderer()->SetTransform(matrix4x4f::Identity());

	RendererLocator::getRenderer()->SetAmbientColor(m_ambientColor);
	RendererLocator::getRenderer()->SetLights(m_lights.size(), &m_lights[0]);

	matrix4x4f rot = matrix4x4f::RotateYMatrix(m_total_time * 2);
	rot[14] = -std::max(150.0f - 30.0f * m_total_time, 30.0f);
	m_model->Render(rot);
}
