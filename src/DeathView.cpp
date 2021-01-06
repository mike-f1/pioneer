// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "DeathView.h"

#include "Camera.h"
#include "Game.h"
#include "GameConfig.h"
#include "GameConfSingleton.h"
#include "GameLocator.h"
#include "InGameViews.h"
#include "InGameViewsLocator.h"
#include "Player.h"
#include "graphics/Graphics.h"
#include "graphics/Renderer.h"
#include "graphics/RendererLocator.h"

DeathView::DeathView() :
	View(),
	m_cameraDist(0.0)
{
	float size[2];
	GetSizeRequested(size);

	SetTransparency(true);

	float znear;
	float zfar;
	RendererLocator::getRenderer()->GetNearFarRange(znear, zfar);

	const float fovY = GameConfSingleton::getInstance().Float("FOVVertical");
	m_cameraContext.Reset(new CameraContext(Graphics::GetScreenWidth(), Graphics::GetScreenHeight(), fovY, znear, zfar));
	m_camera = std::make_unique<Camera>(m_cameraContext);
}

DeathView::~DeathView()
{}

void DeathView::Init()
{
	Player *player = GameLocator::getGame()->GetPlayer();
	m_cameraDist = player->GetClipRadius() * 5.0;
	m_cameraContext->SetCameraFrame(player->GetFrame());
	m_cameraContext->SetCameraPosition(player->GetInterpPosition() + vector3d(0, 0, m_cameraDist));
	m_cameraContext->SetCameraOrient(matrix3x3d::Identity());
}

void DeathView::OnSwitchTo()
{
	InGameViewsLocator::getInGameViews()->ShouldDrawGui(false);
}

void DeathView::Update(const float frameTime)
{
	assert(GameLocator::getGame()->GetPlayer()->IsDead());

	m_cameraDist += 160.0 * frameTime;
	m_cameraContext->SetCameraPosition(GameLocator::getGame()->GetPlayer()->GetInterpPosition() + vector3d(0, 0, m_cameraDist));
	m_cameraContext->BeginFrame();
	m_camera->Update();
}

void DeathView::Draw3D()
{
	PROFILE_SCOPED()
	m_cameraContext->ApplyDrawTransforms();
	m_camera->Draw();
	m_cameraContext->EndFrame();
}
