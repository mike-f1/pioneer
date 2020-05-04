// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "PlayerShipController.h"

#include "Frame.h"
#include "Game.h"
#include "GameConfig.h"
#include "GameConfSingleton.h"
#include "GameLocator.h"
#include "GameSaveError.h"
#include "InGameViews.h"
#include "InGameViewsLocator.h"
#include "KeyBindings.h"
#include "LuaObject.h"
#include "OS.h"
#include "Pi.h"
#include "Player.h"
#include "Ship.h"
#include "Space.h"
#include "WorldView.h"

#include <algorithm>

PlayerShipController::PlayerShipController() :
	ShipController(),
	m_combatTarget(0),
	m_navTarget(0),
	m_setSpeedTarget(0),
	m_controlsLocked(false),
	m_invertMouse(false),
	m_mouseActive(false),
	m_disableMouseFacing(false),
	m_rotationDamping(true),
	m_mouseX(0.0),
	m_mouseY(0.0),
	m_setSpeed(0.0),
	m_flightControlState(CONTROL_MANUAL),
	m_lowThrustPower(0.25), // note: overridden by the default value in GameConfig.cpp (DefaultLowThrustPower setting)
	m_mouseDir(0.0)
{
	const float deadzone = GameConfSingleton::getInstance().Float("JoystickDeadzone");
	m_joystickDeadzone = Clamp(deadzone, 0.01f, 1.0f); // do not use (deadzone * deadzone) as values are 0<>1 range, aka: 0.1 * 0.1 = 0.01 or 1% deadzone!!! Not what player asked for!
	m_fovY = GameConfSingleton::getInstance().Float("FOVVertical");
	m_lowThrustPower = GameConfSingleton::getInstance().Float("DefaultLowThrustPower");

	RegisterInputBindings();
}

void PlayerShipController::RegisterInputBindings()
{
	using namespace KeyBindings;
	using namespace std::placeholders;

	m_inputFrame.reset(new InputFrame("PlayerShipController"));

	auto &controlsPage = Pi::input.GetBindingPage("ShipControls");

	auto &weaponsGroup = controlsPage.GetBindingGroup("Weapons");
	m_inputBindings.targetObject = m_inputFrame->AddActionBinding("BindTargetObject", weaponsGroup, ActionBinding(SDLK_y));
	m_inputBindings.primaryFire = m_inputFrame->AddActionBinding("BindPrimaryFire", weaponsGroup, ActionBinding(SDLK_SPACE));
	m_inputBindings.secondaryFire = m_inputFrame->AddActionBinding("BindSecondaryFire", weaponsGroup, ActionBinding(SDLK_m));
	m_inputBindings.secondaryFire->StoreOnActionCallback(std::bind(&PlayerShipController::FireMissile, this, _1));

	std::string bindRecString = "BindWCRecall";
	for (int i = 0; i < WEAPON_CONFIG_SLOTS; i++) {
		m_inputBindings.weaponConfigRecall[i] =
			m_inputFrame->AddActionBinding(bindRecString + std::to_string(i + 1), weaponsGroup, ActionBinding(SDLK_1 + i));
	}

	std::string bindStoString = "BindWCStore";
	for (int i = 0; i < WEAPON_CONFIG_SLOTS; i++) {
		m_inputBindings.weaponConfigStore[i] =
			m_inputFrame->AddActionBinding(bindStoString + std::to_string(i + 1), weaponsGroup, ActionBinding(KeyBinding(SDLK_1 + i, KMOD_LCTRL)));
	}

	auto &flightGroup = controlsPage.GetBindingGroup("ShipOrient");
	m_inputBindings.pitch = m_inputFrame->AddAxisBinding("BindAxisPitch", flightGroup, AxisBinding(SDLK_k, SDLK_i));
	m_inputBindings.yaw = m_inputFrame->AddAxisBinding("BindAxisYaw", flightGroup, AxisBinding(SDLK_j, SDLK_l));
	m_inputBindings.roll = m_inputFrame->AddAxisBinding("BindAxisRoll", flightGroup, AxisBinding(SDLK_u, SDLK_o));
	m_inputBindings.killRot = m_inputFrame->AddActionBinding("BindKillRot", flightGroup, ActionBinding(SDLK_p, SDLK_x));
	m_inputBindings.toggleRotationDamping = m_inputFrame->AddActionBinding("BindToggleRotationDamping", flightGroup, ActionBinding(SDLK_v));
	m_inputBindings.toggleRotationDamping->StoreOnActionCallback(std::bind(&PlayerShipController::ToggleRotationDamping, this, _1));

	auto &thrustGroup = controlsPage.GetBindingGroup("ManualControl");
	m_inputBindings.thrustForward = m_inputFrame->AddAxisBinding("BindAxisThrustForward", thrustGroup, AxisBinding(SDLK_w, SDLK_s));
	m_inputBindings.thrustUp = m_inputFrame->AddAxisBinding("BindAxisThrustUp", thrustGroup, AxisBinding(SDLK_r, SDLK_f));
	m_inputBindings.thrustLeft = m_inputFrame->AddAxisBinding("BindAxisThrustLeft", thrustGroup, AxisBinding(SDLK_a, SDLK_d));
	m_inputBindings.thrustLowPower = m_inputFrame->AddActionBinding("BindThrustLowPower", thrustGroup, ActionBinding(SDLK_LSHIFT));
	m_inputBindings.toggleUC = m_inputFrame->AddActionBinding("BindToggleUC", thrustGroup, ActionBinding(SDLK_g));
	m_inputBindings.toggleUC->StoreOnActionCallback(std::bind(&PlayerShipController::ToggleUC, this, _1));

	auto &speedGroup = controlsPage.GetBindingGroup("SpeedControl");
	m_inputBindings.speedControl = m_inputFrame->AddAxisBinding("BindSpeedControl", speedGroup, AxisBinding(SDLK_RETURN, SDLK_RSHIFT));
	m_inputBindings.toggleSetSpeed = m_inputFrame->AddActionBinding("BindToggleSetSpeed", speedGroup, ActionBinding(SDLK_v));
	m_inputBindings.toggleSetSpeed->StoreOnActionCallback(std::bind(&PlayerShipController::ToggleSetSpeedMode, this, _1));

	m_inputFrame->SetActive(true);

	Pi::input.PushInputFrame(m_inputFrame.get());
}

PlayerShipController::~PlayerShipController()
{
	Pi::input.RemoveInputFrame(m_inputFrame.get());
}

void PlayerShipController::SaveToJson(Json &jsonObj, Space *space)
{
	Json playerShipControllerObj({}); // Create JSON object to contain player ship controller data.
	playerShipControllerObj["flight_control_state"] = m_flightControlState;
	playerShipControllerObj["set_speed"] = m_setSpeed;
	playerShipControllerObj["low_thrust_power"] = m_lowThrustPower;
	playerShipControllerObj["rotation_damping"] = m_rotationDamping;
	playerShipControllerObj["index_for_combat_target"] = space->GetIndexForBody(m_combatTarget);
	playerShipControllerObj["index_for_nav_target"] = space->GetIndexForBody(m_navTarget);
	playerShipControllerObj["index_for_set_speed_target"] = space->GetIndexForBody(m_setSpeedTarget);
	Json gunStatusesSlots = Json::array();
	for (std::vector<int> statuses : m_gunStatuses) {
		Json gunStatusesSlot = Json::array();
		for (int num = 0; num < statuses.size(); num++) {
			gunStatusesSlot.push_back(statuses[num]);
		}
		gunStatusesSlots.push_back(gunStatusesSlot);
	}
	playerShipControllerObj["gun_statuses"] = gunStatusesSlots;

	jsonObj["player_ship_controller"] = playerShipControllerObj; // Add player ship controller object to supplied object.
}

void PlayerShipController::LoadFromJson(const Json &jsonObj)
{
	try {
		Json playerShipControllerObj = jsonObj["player_ship_controller"];

		m_flightControlState = playerShipControllerObj["flight_control_state"];
		m_setSpeed = playerShipControllerObj["set_speed"];
		m_lowThrustPower = playerShipControllerObj["low_thrust_power"];
		m_rotationDamping = playerShipControllerObj["rotation_damping"];
		//figure out actual bodies in PostLoadFixup - after Space body index has been built
		m_combatTargetIndex = playerShipControllerObj["index_for_combat_target"];
		m_navTargetIndex = playerShipControllerObj["index_for_nav_target"];
		m_setSpeedTargetIndex = playerShipControllerObj["index_for_set_speed_target"];

		Json gunStatuses = playerShipControllerObj["gun_statuses"];
		assert(gunStatuses.size() == WEAPON_CONFIG_SLOTS);
		for (unsigned slot = 0; slot < WEAPON_CONFIG_SLOTS; slot++) {
			Json gunStatus = gunStatuses[slot];
			for (int gs = 0; gs < gunStatus.size(); gs++) {
				m_gunStatuses[slot].push_back(gunStatus[gs]);
			}
		}
	} catch (Json::type_error &) {
		throw SavedGameCorruptException();
	}
}

void PlayerShipController::PostLoadFixup(Space *space)
{
	m_combatTarget = space->GetBodyByIndex(m_combatTargetIndex);
	m_navTarget = space->GetBodyByIndex(m_navTargetIndex);
	m_setSpeedTarget = space->GetBodyByIndex(m_setSpeedTargetIndex);
}

void PlayerShipController::StaticUpdate(const float timeStep)
{
	vector3d v;
	matrix4x4d m;

	if (m_ship->GetFlightState() == Ship::FLYING) {
		switch (m_flightControlState) {
		case CONTROL_FIXSPEED:
			PollControls(timeStep, true);
			if (IsAnyLinearThrusterKeyDown()) break;
			v = -m_ship->GetOrient().VectorZ() * m_setSpeed;
			if (m_setSpeedTarget) {
				v += m_setSpeedTarget->GetVelocityRelTo(m_ship->GetFrame());
			}
			m_ship->AIMatchVel(v);
			break;
		case CONTROL_FIXHEADING_FORWARD:
		case CONTROL_FIXHEADING_BACKWARD:
		case CONTROL_FIXHEADING_NORMAL:
		case CONTROL_FIXHEADING_ANTINORMAL:
		case CONTROL_FIXHEADING_RADIALLY_INWARD:
		case CONTROL_FIXHEADING_RADIALLY_OUTWARD:
		case CONTROL_FIXHEADING_KILLROT:
			PollControls(timeStep, true);
			if (IsAnyAngularThrusterKeyDown()) break;
			v = m_ship->GetVelocity().NormalizedSafe();
			if (m_flightControlState == CONTROL_FIXHEADING_BACKWARD ||
				m_flightControlState == CONTROL_FIXHEADING_ANTINORMAL)
				v = -v;
			if (m_flightControlState == CONTROL_FIXHEADING_NORMAL ||
				m_flightControlState == CONTROL_FIXHEADING_ANTINORMAL)
				v = v.Cross(m_ship->GetPosition().NormalizedSafe());
			if (m_flightControlState == CONTROL_FIXHEADING_RADIALLY_INWARD)
				v = -m_ship->GetPosition().NormalizedSafe();
			if (m_flightControlState == CONTROL_FIXHEADING_RADIALLY_OUTWARD)
				v = m_ship->GetPosition().NormalizedSafe();
			if (m_flightControlState == CONTROL_FIXHEADING_KILLROT) {
				v = -m_ship->GetOrient().VectorZ();
				if (m_ship->GetAngVelocity().Length() < 0.0001) // fixme magic number
					SetFlightControlState(CONTROL_MANUAL);
			}

			m_ship->AIFaceDirection(v);
			break;
		case CONTROL_MANUAL:
			PollControls(timeStep, false);
			break;
		case CONTROL_AUTOPILOT:
			if (m_ship->AIIsActive()) break;
			GameLocator::getGame()->RequestTimeAccel(Game::TIMEACCEL_1X);
			//			AIMatchVel(vector3d(0.0));			// just in case autopilot doesn't...
			// actually this breaks last timestep slightly in non-relative target cases
			m_ship->AIMatchAngVelObjSpace(vector3d(0.0));
			if (Frame::GetFrame(m_ship->GetFrame())->IsRotFrame())
				SetFlightControlState(CONTROL_FIXSPEED);
			else
				SetFlightControlState(CONTROL_MANUAL);
			m_setSpeed = 0.0;
			break;
		default: assert(0); break;
		}
	} else
		SetFlightControlState(CONTROL_MANUAL);

	int i = 0;
	std::for_each(begin(m_inputBindings.weaponConfigRecall), end(m_inputBindings.weaponConfigRecall),
	[&](KeyBindings::ActionBinding *wCR) {
		assert(wCR != nullptr);
		if (wCR->IsActive()) {
			int numMountedGuns = m_ship->GetMountedGunsNum();
			int numStoredGuns = m_gunStatuses[i].size();
			if (numMountedGuns > numStoredGuns) {
				// Use stored status where present and do nothing on remaining guns
				for (size_t j = 0; j < numStoredGuns; j++) {
					m_ship->SetActivationStateOfGun(j, m_gunStatuses[i][j]);
				}
			} else {
				// Use status and drop unused
				for (size_t j = 0; j < numMountedGuns; j++) {
					m_ship->SetActivationStateOfGun(j, m_gunStatuses[i][j]);
				}
				m_gunStatuses[i].resize(numMountedGuns);
			}
		}
		i++;
	});
	i = 0; // reuse variable
	std::for_each(begin(m_inputBindings.weaponConfigStore), end(m_inputBindings.weaponConfigStore),
	[&](KeyBindings::ActionBinding *wCS) {
		assert(wCS != nullptr);
		if (wCS->IsActive()) {
			int numGuns = m_ship->GetMountedGunsNum();
			m_gunStatuses[i].resize(numGuns);
			for (size_t j = 0; j < numGuns; j++) {
				m_gunStatuses[i][j] = m_ship->GetActivationStateOfGun(j) ? 1 : 0;
			}
		}
		i++;
	});

	//call autopilot AI, if active (also applies to set speed and heading lock modes)
	OS::EnableFPE();
	m_ship->AITimeStep(timeStep);
	OS::DisableFPE();
}

void PlayerShipController::CheckControlsLock()
{
	m_controlsLocked = (GameLocator::getGame()->IsPaused() ||
		GameLocator::getGame()->GetPlayer()->IsDead() ||
		(m_ship->GetFlightState() != Ship::FLYING) ||
		Pi::IsConsoleActive() ||
		!InGameViewsLocator::getInGameViews()->IsWorldView()); //to prevent moving the ship in starmap etc.
}

vector3d PlayerShipController::GetMouseDir() const
{
	// translate from system to local frame
	return m_mouseDir * Frame::GetFrame(m_ship->GetFrame())->GetOrient();
}

// mouse wraparound control function
static double clipmouse(double cur, double inp)
{
	if (cur * cur > 0.7 && cur * inp > 0) return 0.0;
	if (inp > 0.2) return 0.2;
	if (inp < -0.2) return -0.2;
	return inp;
}

void PlayerShipController::PollControls(const float timeStep, const bool force_rotation_damping)
{
	static bool stickySpeedKey = false;
	CheckControlsLock();
	if (m_controlsLocked) return;

	// if flying
	{
		m_ship->ClearThrusterState();

		// vector3d wantAngVel(0.0);
		double angThrustSoftness = 10.0;

		const float linearThrustPower = (m_inputBindings.thrustLowPower->IsActive() ? m_lowThrustPower : 1.0f);

		// have to use this function. SDL mouse position event is bugged in windows
		auto motion = Pi::input->GetMouseMotion(MouseMotionBehaviour::DriveShip);
		if (std::get<0>(motion)) {
			std::array<int, 2> mouseMotion = {std::get<1>(motion), std::get<2>(motion)};
			// use ship rotation relative to system, unchanged by frame transitions
			matrix3x3d rot = m_ship->GetOrientRelTo(Frame::GetFrame(m_ship->GetFrame())->GetNonRotFrame());
			if (!m_mouseActive && !m_disableMouseFacing) {
				m_mouseDir = -rot.VectorZ();
				m_mouseX = m_mouseY = 0;
				m_mouseActive = true;
			}
			vector3d objDir = m_mouseDir * rot;

			const double radiansPerPixel = 0.00002 * m_fovY;
			const int maxMotion = std::max(abs(mouseMotion[0]), abs(mouseMotion[1]));
			const double accel = Clamp(maxMotion / 4.0, 0.0, 90.0 / m_fovY);

			m_mouseX += mouseMotion[0] * accel * radiansPerPixel;
			double modx = clipmouse(objDir.x, m_mouseX);
			m_mouseX -= modx;

			const bool invertY = (Pi::input.IsMouseYInvert() ? !m_invertMouse : m_invertMouse);

			m_mouseY += mouseMotion[1] * accel * radiansPerPixel * (invertY ? -1 : 1);
			double mody = clipmouse(objDir.y, m_mouseY);
			m_mouseY -= mody;

			if (!is_zero_general(modx) || !is_zero_general(mody)) {
				matrix3x3d mrot = matrix3x3d::RotateY(modx) * matrix3x3d::RotateX(mody);
				m_mouseDir = (rot * (mrot * objDir)).Normalized();
			}
		} else {
			m_mouseActive = false;
		}

		if (m_flightControlState == CONTROL_FIXSPEED) {
			double oldSpeed = m_setSpeed;
			if (stickySpeedKey && !m_inputBindings.speedControl->IsActive())
				stickySpeedKey = false;

			if (!stickySpeedKey) {
				const double MAX_SPEED = 300000000;
				m_setSpeed += m_inputBindings.speedControl->GetValue() * std::max(std::abs(m_setSpeed) * 0.05, 1.0);
				m_setSpeed = Clamp(m_setSpeed, -MAX_SPEED, MAX_SPEED);

				if (((oldSpeed < 0.0) && (m_setSpeed >= 0.0)) ||
					((oldSpeed > 0.0) && (m_setSpeed <= 0.0))) {
					// flipped from going forward to backwards. make the speed 'stick' at zero
					// until the player lets go of the key and presses it again
					stickySpeedKey = true;
					m_setSpeed = 0;
				}
			}
		}

		if (m_inputBindings.thrustForward->IsActive())
			m_ship->SetThrusterState(2, -linearThrustPower * m_inputBindings.thrustForward->GetValue());
		if (m_inputBindings.thrustUp->IsActive())
			m_ship->SetThrusterState(1, linearThrustPower * m_inputBindings.thrustUp->GetValue());
		if (m_inputBindings.thrustLeft->IsActive())
			m_ship->SetThrusterState(0, -linearThrustPower * m_inputBindings.thrustLeft->GetValue());

		auto fire = Pi::input->GetMouseMotion(MouseMotionBehaviour::Fire);
		if (m_inputBindings.primaryFire->IsActive() || std::get<0>(fire)) {
			//XXX worldview? madness, ask from ship instead
			GunDir dir = InGameViewsLocator::getInGameViews()->GetWorldView()->GetActiveWeapon() ? GunDir::GUN_REAR : GunDir::GUN_FRONT;
			m_ship->SetGunsState(dir, 1);
		} else {
			m_ship->SetGunsState(GunDir::GUN_FRONT, 0);
			m_ship->SetGunsState(GunDir::GUN_REAR, 0);
		}

		vector3d wantAngVel = vector3d(
			m_inputBindings.pitch->GetValue(),
			m_inputBindings.yaw->GetValue(),
			m_inputBindings.roll->GetValue());

		if (m_inputBindings.killRot->IsActive()) SetFlightControlState(CONTROL_FIXHEADING_KILLROT);

		if (m_inputBindings.thrustLowPower->IsActive())
			angThrustSoftness = 50.0;

		if (wantAngVel.Length() >= 0.001 || force_rotation_damping || m_rotationDamping) {
			if (GameLocator::getGame()->GetTimeAccel() != Game::TIMEACCEL_1X) {
				for (int axis = 0; axis < 3; axis++)
					wantAngVel[axis] = wantAngVel[axis] * GameLocator::getGame()->GetInvTimeAccelRate();
			}

			m_ship->AIModelCoordsMatchAngVel(wantAngVel, angThrustSoftness);
		}

		if (m_mouseActive && !m_disableMouseFacing) m_ship->AIFaceDirection(GetMouseDir());
	}
}

bool PlayerShipController::IsAnyAngularThrusterKeyDown()
{
	return !Pi::IsConsoleActive() && (m_inputBindings.pitch->IsActive() || m_inputBindings.yaw->IsActive() || m_inputBindings.roll->IsActive());
}

bool PlayerShipController::IsAnyLinearThrusterKeyDown()
{
	return !Pi::IsConsoleActive() && (m_inputBindings.thrustForward->IsActive() || m_inputBindings.thrustLeft->IsActive() || m_inputBindings.thrustUp->IsActive());
}

void PlayerShipController::SetFlightControlState(FlightControlState s)
{
	if (m_flightControlState != s) {
		m_flightControlState = s;
		m_ship->AIClearInstructions();
		//set desired velocity to current actual
		if (m_flightControlState == CONTROL_FIXSPEED) {
			// Speed is set to the projection of the velocity onto the target.

			vector3d shipVel = m_setSpeedTarget ?
				// Ship's velocity with respect to the target, in current frame's coordinates
				-m_setSpeedTarget->GetVelocityRelTo(m_ship) :
				// Ship's velocity with respect to current frame
				m_ship->GetVelocity();

			// A change from Manual to Set Speed never sets a negative speed.
			m_setSpeed = std::max(shipVel.Dot(-m_ship->GetOrient().VectorZ()), 0.0);
		}
	}
}

void PlayerShipController::SetLowThrustPower(float power)
{
	assert((power >= 0.0f) && (power <= 1.0f));
	m_lowThrustPower = power;
}

void PlayerShipController::SetRotationDamping(bool enabled)
{
	m_rotationDamping = enabled;
}

void PlayerShipController::ToggleRotationDamping(bool down)
{
	if (down) return;
	m_rotationDamping = !m_rotationDamping;
}

void PlayerShipController::ToggleUC(bool down)
{
	if (down) return;
	m_ship->SetWheelState(is_equal_exact(m_ship->GetWheelState(), 0.0f));
}

void PlayerShipController::FireMissile(bool down)
{
	if (down) return;
	if (!GameLocator::getGame()->GetPlayer()->GetCombatTarget())
		return;
	LuaObject<Ship>::CallMethod(GameLocator::getGame()->GetPlayer(), "FireMissileAt", "any", static_cast<Ship *>(GameLocator::getGame()->GetPlayer()->GetCombatTarget()));
}

void PlayerShipController::ToggleSetSpeedMode(bool down)
{
	if (down) return;
	if (GetFlightControlState() != CONTROL_FIXSPEED) {
		SetFlightControlState(CONTROL_FIXSPEED);
	} else {
		SetFlightControlState(CONTROL_MANUAL);
	}
}

Body *PlayerShipController::GetCombatTarget() const
{
	return m_combatTarget;
}

Body *PlayerShipController::GetNavTarget() const
{
	return m_navTarget;
}

Body *PlayerShipController::GetSetSpeedTarget() const
{
	return m_setSpeedTarget;
}

void PlayerShipController::SetCombatTarget(Body *const target, bool setSpeedTo)
{
	if (setSpeedTo)
		m_setSpeedTarget = target;
	else if (m_setSpeedTarget == m_combatTarget)
		m_setSpeedTarget = 0;
	m_combatTarget = target;
}

void PlayerShipController::SetNavTarget(Body *const target, bool setSpeedTo)
{
	if (setSpeedTo)
		m_setSpeedTarget = target;
	else if (m_setSpeedTarget == m_navTarget)
		m_setSpeedTarget = 0;
	m_navTarget = target;
}

void PlayerShipController::SetSetSpeedTarget(Body *const target)
{
	m_setSpeedTarget = target;
}
