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
	Input::BindingGroup *group;

#define BINDING_GROUP(n) group = page->GetBindingGroup(#n);
#define KEY_BINDING(n, id, k1, k2)                                    \
	n =                                                               \
		Pi::input.AddActionBinding(id, group, ActionBinding(k1, k2)); \
	actions.push_back(n);
#define AXIS_BINDING(n, id, k1, k2)                               \
	n =                                                           \
		Pi::input.AddAxisBinding(id, group, AxisBinding(k1, k2)); \
	axes.push_back(n);

	BINDING_GROUP(GeneralViewControls)
//	KEY_BINDING(cycleCameraMode, "BindCycleCameraMode", SDLK_F1, 0) <- In PiGui

	AXIS_BINDING(cameraRoll, "BindCameraRoll", SDLK_KP_1, SDLK_KP_3)
	AXIS_BINDING(cameraPitch, "BindCameraPitch", SDLK_KP_2, SDLK_KP_8)
	AXIS_BINDING(cameraYaw, "BindCameraYaw", SDLK_KP_4, SDLK_KP_6)
	AXIS_BINDING(cameraZoom, "BindViewZoom", SDLK_EQUALS, SDLK_MINUS)

	AXIS_BINDING(lookYaw, "BindLookYaw", 0, 0);
	AXIS_BINDING(lookPitch, "BindLookPitch", 0, 0);

	KEY_BINDING(frontCamera, "BindFrontCamera", SDLK_KP_8, SDLK_UP)
	KEY_BINDING(rearCamera, "BindRearCamera", SDLK_KP_2, SDLK_DOWN)
	KEY_BINDING(leftCamera, "BindLeftCamera", SDLK_KP_4, SDLK_LEFT)
	KEY_BINDING(rightCamera, "BindRightCamera", SDLK_KP_6, SDLK_RIGHT)
	KEY_BINDING(topCamera, "BindTopCamera", SDLK_KP_9, 0)
	KEY_BINDING(bottomCamera, "BindBottomCamera", SDLK_KP_3, 0)

	KEY_BINDING(resetCamera, "BindResetCamera", SDLK_HOME, 0)

#undef BINDING_GROUP
#undef KEY_BINDING
#undef AXIS_BINDING
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
	Pi::input.PushInputFrame(&InputBindings);

	m_onMouseWheelCon =
		Pi::input.onMouseWheel.connect(sigc::mem_fun(this, &ShipViewController::OnMouseWheel));

	m_onResetCam = InputBindings.resetCamera->onPress.connect(sigc::mem_fun(this, &ShipViewController::OnCamReset));

	GameLocator::getGame()->GetPlayer()->GetPlayerController()->SetMouseForRearView(GetCamType() == CAM_INTERNAL && m_internalCameraController->GetMode() == InternalCameraController::MODE_REAR);
}

void ShipViewController::Deactivated()
{
	Pi::input.RemoveInputFrame(&InputBindings);

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
	if (m_internalCameraController->GetMode() != m)
		// TODO: find a way around this, or move it to a dedicated system.
		Sound::PlaySfx("Click", 0.3, 0.3, false);
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
	if (Pi::input.MouseButtonState(SDL_BUTTON_MIDDLE) && !headtracker_input_priority) {
		// invert the mouse input to convert between screen coordinates and
		// right-hand coordinate system rotation.
		cam->YawCamera(-mouseMotion[0] * MOUSELOOK_SPEED);
		cam->PitchCamera(-mouseMotion[1] * MOUSELOOK_SPEED);
	}

	m_activeCameraController->Update();
}
