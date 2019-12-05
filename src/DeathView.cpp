// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "DeathView.h"

#include "Camera.h"
#include "Game.h"
#include "GameConfig.h"
#include "GameConfSingleton.h"
#include "InGameViews.h"
#include "Pi.h"
#include "Player.h"
#include "ShipCpanel.h"
#include "graphics/Graphics.h"
#include "graphics/Renderer.h"

DeathView::DeathView(Game *game, Graphics::Renderer *r) :
	View(),
	m_game(game)
{
	m_renderer = r;

	float size[2];
	GetSizeRequested(size);

	SetTransparency(true);

	float znear;
	float zfar;
	m_renderer->GetNearFarRange(znear, zfar);

	const float fovY = GameConfSingleton::getInstance().Float("FOVVertical");
	m_cameraContext.Reset(new CameraContext(Graphics::GetScreenWidth(), Graphics::GetScreenHeight(), fovY, znear, zfar));
	m_camera.reset(new Camera(m_cameraContext, m_renderer));
}

DeathView::~DeathView() {}

void DeathView::Init()
{
	m_cameraDist = m_game->GetPlayer()->GetClipRadius() * 5.0;
	m_cameraContext->SetCameraFrame(m_game->GetPlayer()->GetFrame());
	m_cameraContext->SetCameraPosition(m_game->GetPlayer()->GetInterpPosition() + vector3d(0, 0, m_cameraDist));
	m_cameraContext->SetCameraOrient(matrix3x3d::Identity());
}

void DeathView::OnSwitchTo()
{
	m_game->GetInGameViews()->GetCpan()->HideAll();
}

void DeathView::Update()
{
	assert(m_game->GetPlayer()->IsDead());

	m_cameraDist += 160.0 * Pi::GetFrameTime();
	m_cameraContext->SetCameraPosition(m_game->GetPlayer()->GetInterpPosition() + vector3d(0, 0, m_cameraDist));
	m_cameraContext->BeginFrame();
	m_camera->Update();
}

void DeathView::Draw3D()
{
	PROFILE_SCOPED()
	m_cameraContext->ApplyDrawTransforms(Pi::renderer);
	m_camera->Draw();
	m_cameraContext->EndFrame();
}
