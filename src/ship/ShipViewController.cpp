// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "ShipViewController.h"

#include "CameraController.h"
#include "WorldView.h"

#include "Game.h"
#include "GameLocator.h"
#include "Pi.h"
#include "Player.h"
#include "PlayerShipController.h"

namespace {
	static const float MOUSELOOK_SPEED = 0.01;
	static const float ZOOM_SPEED = 1.f;
	static const float WHEEL_SENSITIVITY = .05f; // Should be a variable in user settings.
} // namespace

ShipViewController::InputBinding ShipViewController::InputBindings;

void ShipViewController::InputBinding::RegisterBindings()
{
	using namespace KeyBindings;

	Input::BindingPage *page = Pi::input.GetBindingPage("ShipView");
	Input::BindingGroup *group = page->GetBindingGroup("GeneralViewControls");

	cameraRoll = Pi::input.AddAxisBinding("BindCameraRoll", group, AxisBinding(SDLK_KP_1, SDLK_KP_3));
	axes.push_back(cameraRoll);
	cameraPitch = Pi::input.AddAxisBinding("BindCameraPitch", group, AxisBinding(SDLK_KP_2, SDLK_KP_8));
	axes.push_back(cameraPitch);
	cameraYaw = Pi::input.AddAxisBinding("BindCameraYaw", group, AxisBinding(SDLK_KP_4, SDLK_KP_6));
	axes.push_back(cameraYaw);
	cameraZoom = Pi::input.AddAxisBinding("BindViewZoom", group, AxisBinding(SDLK_EQUALS, SDLK_MINUS));
	axes.push_back(cameraZoom);
	lookYaw = Pi::input.AddAxisBinding("BindLookYaw", group, AxisBinding(0, 0));
	axes.push_back(lookYaw);
	lookPitch = Pi::input.AddAxisBinding("BindLookPitch", group, AxisBinding(0, 0));
	axes.push_back(lookPitch);

	frontCamera = Pi::input.AddActionBinding("BindFrontCamera", group, ActionBinding(SDLK_KP_8, SDLK_UP));
	actions.push_back(frontCamera);
	rearCamera = Pi::input.AddActionBinding("BindRearCamera", group, ActionBinding(SDLK_KP_2, SDLK_DOWN));
	actions.push_back(rearCamera);
	leftCamera = Pi::input.AddActionBinding("BindLeftCamera", group, ActionBinding(SDLK_KP_4, SDLK_LEFT));
	actions.push_back(leftCamera);
	rightCamera = Pi::input.AddActionBinding("BindRightCamera", group, ActionBinding(SDLK_KP_6, SDLK_RIGHT));
	actions.push_back(rightCamera);
	topCamera = Pi::input.AddActionBinding("BindTopCamera", group, ActionBinding(SDLK_KP_9));
	actions.push_back(topCamera);
	bottomCamera = Pi::input.AddActionBinding("BindBottomCamera", group, ActionBinding(SDLK_KP_3));
	actions.push_back(bottomCamera);

	resetCamera = Pi::input.AddActionBinding("BindResetCamera", group, ActionBinding(SDLK_HOME));
	actions.push_back(resetCamera);

	mouseWheel = Pi::input.AddWheelBinding("MouseWheel", group, WheelBinding());
	wheel = mouseWheel;
}

void ShipViewController::OnCamReset()
{
	auto *cam = static_cast<MoveableCameraController *>(m_activeCameraController);
	if (cam) cam->Reset();
}

void ShipViewController::OnMouseWheel(bool up)
{
	if (m_activeCameraController == nullptr) return;

	if (m_activeCameraController->IsExternal()) {
		MoveableCameraController *cam = static_cast<MoveableCameraController *>(m_activeCameraController);

		if (!up) // Zoom out
			cam->ZoomEvent(ZOOM_SPEED * WHEEL_SENSITIVITY);
		else
			cam->ZoomEvent(-ZOOM_SPEED * WHEEL_SENSITIVITY);
	}
}

ShipViewController::~ShipViewController()
{
	Deactivated();
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
	if (!Pi::input.PushInputFrame(&InputBindings)) return;

	m_onMouseWheelCon =
		InputBindings.mouseWheel->onAxis.connect(sigc::mem_fun(this, &ShipViewController::OnMouseWheel));

	m_onResetCam = InputBindings.resetCamera->onPress.connect(sigc::mem_fun(this, &ShipViewController::OnCamReset));

	GameLocator::getGame()->GetPlayer()->GetPlayerController()->SetMouseForRearView(GetCamType() == CAM_INTERNAL && m_internalCameraController->GetMode() == InternalCameraController::MODE_REAR);
}

void ShipViewController::Deactivated()
{
	if (!Pi::input.RemoveInputFrame(&InputBindings)) return;

	m_onMouseWheelCon.disconnect();
	m_onResetCam.disconnect();
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
	if (!InputBindings.active || Pi::IsConsoleActive()) return;

	if (GetCamType() == CAM_INTERNAL) {
		if (InputBindings.frontCamera->IsActive())
			ChangeInternalCameraMode(InternalCameraController::MODE_FRONT);
		else if (InputBindings.rearCamera->IsActive())
			ChangeInternalCameraMode(InternalCameraController::MODE_REAR);
		else if (InputBindings.leftCamera->IsActive())
			ChangeInternalCameraMode(InternalCameraController::MODE_LEFT);
		else if (InputBindings.rightCamera->IsActive())
			ChangeInternalCameraMode(InternalCameraController::MODE_RIGHT);
		else if (InputBindings.topCamera->IsActive())
			ChangeInternalCameraMode(InternalCameraController::MODE_TOP);
		else if (InputBindings.bottomCamera->IsActive())
			ChangeInternalCameraMode(InternalCameraController::MODE_BOTTOM);

		vector3f rotate = vector3f(
			InputBindings.lookPitch->GetValue() * M_PI / 2.0,
			InputBindings.lookYaw->GetValue() * M_PI / 2.0,
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
			-InputBindings.cameraPitch->GetValue(),
			InputBindings.cameraYaw->GetValue(),
			InputBindings.cameraRoll->GetValue());

		rotate *= frameTime;

		// Horribly abuse our knowledge of the internals of cam->RotateUp/Down.
		// Applied in YXZ order because reasons.
		if (rotate.y != 0.0) cam->YawCamera(rotate.y);
		if (rotate.x != 0.0) cam->PitchCamera(rotate.x);
		if (rotate.z != 0.0) cam->RollCamera(rotate.z);

		if (InputBindings.cameraZoom->IsActive()) {
			cam->ZoomEvent(-InputBindings.cameraZoom->GetValue() * ZOOM_SPEED * frameTime);
		}
		cam->ZoomEventUpdate(frameTime);
	}

	int mouseMotion[2];
	Pi::input.GetMouseMotion(mouseMotion);

	// external camera mouselook
	if (!headtracker_input_priority && Pi::input.MouseButtonState(SDL_BUTTON_MIDDLE)) {
		// invert the mouse input to convert between screen coordinates and
		// right-hand coordinate system rotation.
		cam->YawCamera(-mouseMotion[0] * MOUSELOOK_SPEED);
		cam->PitchCamera(-mouseMotion[1] * MOUSELOOK_SPEED);
	}

	m_activeCameraController->Update();
}
