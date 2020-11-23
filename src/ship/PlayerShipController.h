// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#pragma once

#include "ShipController.h"

#include "input/InputFwd.h"
#include "libs/vector3.h"

#include <array>

class InputFrame;

namespace KeyBindings {
	struct ActionBinding;
	struct AxisBinding;
} // namespace KeyBindings

constexpr int WEAPON_CONFIG_SLOTS = 4;

// autopilot AI + input
class PlayerShipController : public ShipController {
public:
	PlayerShipController();
	~PlayerShipController();

	ControllerType GetType() override { return ControllerType::PLAYER; }
	void SaveToJson(Json &jsonObj, Space *s) override;
	void LoadFromJson(const Json &jsonObj) override;
	void PostLoadFixup(Space *s) override;
	void StaticUpdate(float timeStep) override;
	// Poll controls, set thruster states, gun states and target velocity
	void PollControls(float timeStep, const bool force_rotation_damping);
	bool IsMouseActive() const { return m_mouseActive; }
	void SetDisableMouseFacing(bool disabled) { m_disableMouseFacing = disabled; }
	double GetSetSpeed() const override { return m_setSpeed; }
	void ChangeSetSpeed(double delta) override { m_setSpeed += delta; }
	FlightControlState GetFlightControlState() const override { return m_flightControlState; }
	vector3d GetMouseDir() const; // in local frame
	void SetMouseForRearView(bool enable) { m_invertMouse = enable; }
	void SetFlightControlState(FlightControlState s) override;
	float GetLowThrustPower() const { return m_lowThrustPower; }
	void SetLowThrustPower(float power);

	bool GetRotationDamping() const { return m_rotationDamping; }
	void SetRotationDamping(bool enabled);
	void FireMissile(bool down);

	//targeting
	//XXX AI should utilize one or more of these
	Body *GetCombatTarget() const;
	Body *GetNavTarget() const;
	Body *GetSetSpeedTarget() const override;
	void SetCombatTarget(Body *const target, bool setSpeedTo = false);
	void SetNavTarget(Body *const target, bool setSpeedTo = false);
	void SetSetSpeedTarget(Body *const target);

private:
	void RegisterInputBindings();

	void ToggleRotationDamping(bool down);
	void ToggleUC(bool down);
	void ToggleSetSpeedMode(bool down);

	struct InputBinding {
		// Weapons
		ActionId targetObject;
		ActionId primaryFire;
		ActionId secondaryFire;

		std::array<ActionId , WEAPON_CONFIG_SLOTS> weaponConfigRecall;
		std::array<ActionId , WEAPON_CONFIG_SLOTS> weaponConfigStore;

		// Flight
		AxisId pitch;
		AxisId yaw;
		AxisId roll;
		ActionId killRot;
		ActionId toggleRotationDamping;

		// Manual Control
		AxisId thrustForward;
		AxisId thrustUp;
		AxisId thrustLeft;
		ActionId thrustLowPower;
		ActionId toggleUC;

		// Speed Control
		AxisId speedControl;
		ActionId toggleSetSpeed;
	} m_inputBindings;

	std::unique_ptr<InputFrame> m_inputFrame;

	bool IsAnyAngularThrusterKeyDown();
	bool IsAnyLinearThrusterKeyDown();

	//do a variety of checks to see if input is allowed
	void CheckControlsLock();
	Body *m_combatTarget;
	Body *m_navTarget;
	Body *m_setSpeedTarget;
	bool m_controlsLocked;
	bool m_invertMouse; // used for rear view, *not* for invert Y-axis option (which is Pi::input.IsMouseYInvert)
	bool m_mouseActive;
	bool m_disableMouseFacing;
	bool m_rotationDamping;
	double m_mouseX;
	double m_mouseY;
	double m_setSpeed;
	FlightControlState m_flightControlState;
	float m_fovY; //for mouse acceleration adjustment
	float m_joystickDeadzone;
	float m_lowThrustPower;
	int m_combatTargetIndex; //for PostLoadFixUp
	int m_navTargetIndex;
	int m_setSpeedTargetIndex;
	vector3d m_mouseDir;

	// TODO: Store/Recall barrels configuration
	std::array<std::vector<int>, WEAPON_CONFIG_SLOTS> m_gunStatuses;
};
