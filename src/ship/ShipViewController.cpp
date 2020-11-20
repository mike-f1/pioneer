// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "ShipViewController.h"

#include "CameraController.h"
#include "WorldView.h"

#include "Game.h"
#include "GameLocator.h"
#include "input/InputFrame.h"
#include "input/InputFwd.h"
#include "input/KeyBindings.h"
#include "Pi.h"
#include "Player.h"
#include "PlayerShipController.h"

namespace {
	static const float MOUSELOOK_SPEED = 0.01;
	static const float ZOOM_SPEED = 1.f;
	static const float WHEEL_SENSITIVITY = .05f; // Should be a variable in user settings.
} // namespace

ShipViewController::ShipViewController(WorldView *v) :
	InteractionController(v),
	m_camType(CAM_INTERNAL),
	headtracker_input_priority(false)
{
	RegisterInputBindings();
}

ShipViewController::~ShipViewController()
{
}

void ShipViewController::RegisterInputBindings()
{
	using namespace KeyBindings;
	using namespace std::placeholders;

	m_inputFrame = std::make_unique<InputFrame>("ShipView");

	BindingPage &page = m_inputFrame->GetBindingPage("ShipView");

	BindingGroup &group = page.GetBindingGroup("GeneralViewControls");

	m_inputBindings.cameraRoll = m_inputFrame->AddAxisBinding("BindCameraRoll", group, AxisBinding(SDLK_KP_1, SDLK_KP_3));
	m_inputBindings.cameraPitch = m_inputFrame->AddAxisBinding("BindCameraPitch", group, AxisBinding(SDLK_KP_2, SDLK_KP_8));
	m_inputBindings.cameraYaw = m_inputFrame->AddAxisBinding("BindCameraYaw", group, AxisBinding(SDLK_KP_4, SDLK_KP_6));
	m_inputBindings.cameraZoom = m_inputFrame->AddAxisBinding("BindViewZoom", group, AxisBinding(SDLK_EQUALS, SDLK_MINUS));
	m_inputBindings.lookYaw = m_inputFrame->AddAxisBinding("BindLookYaw", group, AxisBinding(0, 0));
	m_inputBindings.lookPitch = m_inputFrame->AddAxisBinding("BindLookPitch", group, AxisBinding(0, 0));

	m_inputBindings.frontCamera = m_inputFrame->AddActionBinding("BindFrontCamera", group, ActionBinding(SDLK_KP_8, SDLK_UP));
	m_inputBindings.rearCamera = m_inputFrame->AddActionBinding("BindRearCamera", group, ActionBinding(SDLK_KP_2, SDLK_DOWN));
	m_inputBindings.leftCamera = m_inputFrame->AddActionBinding("BindLeftCamera", group, ActionBinding(SDLK_KP_4, SDLK_LEFT));
	m_inputBindings.rightCamera = m_inputFrame->AddActionBinding("BindRightCamera", group, ActionBinding(SDLK_KP_6, SDLK_RIGHT));
	m_inputBindings.topCamera = m_inputFrame->AddActionBinding("BindTopCamera", group, ActionBinding(SDLK_KP_9));
	m_inputBindings.bottomCamera = m_inputFrame->AddActionBinding("BindBottomCamera", group, ActionBinding(SDLK_KP_3));

	m_inputBindings.resetCamera = m_inputFrame->AddActionBinding("BindResetCamera", group, ActionBinding(SDLK_HOME));
	m_inputBindings.resetCamera->StoreOnActionCallback(std::bind(&ShipViewController::OnCamReset, this, _1));
}

void ShipViewController::OnCamReset(bool down)
{
	if (down) return;
	auto *cam = static_cast<MoveableCameraController *>(m_activeCameraController);
	if (cam) cam->Reset();
}

void ShipViewController::LoadFromJson(const Json &jsonObj)
{
	m_internalCameraController->LoadFromJson(jsonObj);
	m_externalCameraController->LoadFromJson(jsonObj);
	m_siderealCameraController->LoadFromJson(jsonObj);
	m_flybyCameraController->LoadFromJson(jsonObj);
}

void ShipViewController::SaveToJson(Json &jsonObj)
{
	jsonObj["cam_type"] = int(m_camType);
	m_internalCameraController->SaveToJson(jsonObj);
	m_externalCameraController->SaveToJson(jsonObj);
	m_siderealCameraController->SaveToJson(jsonObj);
	m_flybyCameraController->SaveToJson(jsonObj);
}

void ShipViewController::Init(Ship *ship)
{
	RefCountedPtr<CameraContext> m_cameraContext = parentView->GetCameraContext();
	m_internalCameraController.reset(new InternalCameraController(m_cameraContext, ship));
	m_externalCameraController.reset(new ExternalCameraController(m_cameraContext, ship));
	m_siderealCameraController.reset(new SiderealCameraController(m_cameraContext, ship));
	m_flybyCameraController.reset(new FlyByCameraController(m_cameraContext,ship));
	SetCamType(ship, m_camType); //set the active camera
}

void ShipViewController::Activated()
{
	m_inputFrame->SetActive(true);

	GameLocator::getGame()->GetPlayer()->GetPlayerController()->SetMouseForRearView(GetCamType() == CAM_INTERNAL && m_internalCameraController->GetMode() == InternalCameraController::MODE_REAR);
}

void ShipViewController::Deactivated()
{
	m_inputFrame->SetActive(false);
}

void ShipViewController::SetCamType(Ship *ship, enum CamType c)
{
	m_camType = c;

	switch (m_camType) {
	case CAM_INTERNAL: {
			m_activeCameraController = m_internalCameraController.get();
			Player *p = static_cast<Player *>(ship);
			if (p) p->OnCockpitActivated();
		}
		break;
	case CAM_EXTERNAL:
		m_activeCameraController = m_externalCameraController.get();
		break;
	case CAM_SIDEREAL:
		m_activeCameraController = m_siderealCameraController.get();
		break;
	case CAM_FLYBY:
		m_activeCameraController = m_flybyCameraController.get();
		break;
	}

	if (m_camType != CAM_INTERNAL) {
		headtracker_input_priority = false;
	}

	PlayerShipController *psc = static_cast<PlayerShipController *>(ship->GetController());
	if (psc) psc->SetMouseForRearView(m_camType == CAM_INTERNAL && m_internalCameraController->GetMode() == InternalCameraController::MODE_REAR);
	else Output("WARNING: Cannot set mouse for rear view\n");

	m_activeCameraController->Reset();

	onChangeCamType.emit();
}

void ShipViewController::SetCamType(enum CamType c)
{
	// TODO: add collision testing for external cameras to avoid clipping through
	// stations / spaceports the ship is docked to.

	// XXX During initialization, GameLocator is not reliable
	SetCamType(GameLocator::getGame()->GetPlayer(), c);
}

void ShipViewController::ChangeInternalCameraMode(InternalCameraController::Mode m)
{
	if (m_internalCameraController->GetMode() != m) {
		// TODO: find a way around this, or move it to a dedicated system.
		Sound::PlaySfx("Click", 0.3, 0.3, false);
	}
	m_internalCameraController->SetMode(m);
	GameLocator::getGame()->GetPlayer()->GetPlayerController()->SetMouseForRearView(m_camType == CAM_INTERNAL && m_internalCameraController->GetMode() == InternalCameraController::MODE_REAR);
}

void ShipViewController::Update(const float frameTime)
{
	auto *cam = static_cast<MoveableCameraController *>(m_activeCameraController);

	// XXX ugly hack checking for console here
	if (!m_inputFrame->IsActive() || Pi::IsConsoleActive()) return;

	if (GetCamType() == CAM_INTERNAL) {
		if (m_inputBindings.frontCamera->IsActive())
			ChangeInternalCameraMode(InternalCameraController::MODE_FRONT);
		else if (m_inputBindings.rearCamera->IsActive())
			ChangeInternalCameraMode(InternalCameraController::MODE_REAR);
		else if (m_inputBindings.leftCamera->IsActive())
			ChangeInternalCameraMode(InternalCameraController::MODE_LEFT);
		else if (m_inputBindings.rightCamera->IsActive())
			ChangeInternalCameraMode(InternalCameraController::MODE_RIGHT);
		else if (m_inputBindings.topCamera->IsActive())
			ChangeInternalCameraMode(InternalCameraController::MODE_TOP);
		else if (m_inputBindings.bottomCamera->IsActive())
			ChangeInternalCameraMode(InternalCameraController::MODE_BOTTOM);

		vector3f rotate = vector3f(
			m_inputBindings.lookPitch->GetValue() * M_PI / 2.0,
			m_inputBindings.lookYaw->GetValue() * M_PI / 2.0,
			0.0);

		if (rotate.LengthSqr() > 0.0001) {
			cam->SetRotationAngles(rotate);
			headtracker_input_priority = true;
		} else if (headtracker_input_priority) {
			cam->SetRotationAngles({ 0.0, 0.0, 0.0 });
			headtracker_input_priority = false;
		}
	} else {
		vector3d rotate = vector3d(
			-m_inputBindings.cameraPitch->GetValue(),
			m_inputBindings.cameraYaw->GetValue(),
			m_inputBindings.cameraRoll->GetValue());

		rotate *= frameTime;

		// Horribly abuse our knowledge of the internals of cam->RotateUp/Down.
		// Applied in YXZ order because reasons.
		if (rotate.y != 0.0) cam->YawCamera(rotate.y);
		if (rotate.x != 0.0) cam->PitchCamera(rotate.x);
		if (rotate.z != 0.0) cam->RollCamera(rotate.z);

		if (m_inputBindings.cameraZoom->IsActive()) {
			cam->ZoomEvent(-m_inputBindings.cameraZoom->GetValue() * ZOOM_SPEED * frameTime);
		}
		cam->ZoomEventUpdate(frameTime);
	}

	// external camera mouselook
	auto motion = InputFWD::GetMouseMotion(MouseMotionBehaviour::Rotate);
	if (!headtracker_input_priority && std::get<0>(motion)) {
		// invert the mouse input to convert between screen coordinates and
		// right-hand coordinate system rotation.
		cam->YawCamera(-std::get<1>(motion) * MOUSELOOK_SPEED);
		cam->PitchCamera(-std::get<2>(motion) * MOUSELOOK_SPEED);
	}

	m_activeCameraController->Update();
}
