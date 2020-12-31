// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _PLAYER_H
#define _PLAYER_H

#include "Ship.h"

class PlayerShipController;
class ShipCockpit;

namespace Graphics {
	class Renderer;
}

class Player final: public Ship {
public:
	OBJDEF(Player, Ship, PLAYER);
	Player() = delete;
	Player(const Json &jsonObj, Space *space);
	Player(const ShipType::Id &shipId);

	~Player();

	Json SaveToJson(Space *space) override;

	void SetInputActive(bool active);

	void SetDockedWith(SpaceStation *, int port) override;
	bool DoDamage(float kgDamage) override; // overloaded to add "crush" audio
	bool OnDamage(Object *attacker, float kgDamage, const CollisionContact &contactData) override;
	bool SetWheelState(bool down) override; // returns success of state change, NOT state itself
	Missile *SpawnMissile(ShipType::Id missile_type, int power = -1) override;
	void SetAlertState(Ship::AlertState as) override;
	void NotifyRemoved(const Body *const removedBody) override;

	void SetShipType(const ShipType::Id &shipId) override;

	PlayerShipController *GetPlayerController() const;
	//XXX temporary things to avoid causing too many changes right now
	Body *GetCombatTarget() const;
	Body *GetNavTarget() const;
	Body *GetSetSpeedTarget() const;
	void SetCombatTarget(Body *const target, bool setSpeedTo = false);
	void SetNavTarget(Body *const target, bool setSpeedTo = false);
	void SetSetSpeedTarget(Body *const target);
	void ChangeSetSpeed(double delta);

	Ship::HyperjumpStatus InitiateHyperjumpTo(const SystemPath &dest, int warmup_time, double duration, const HyperdriveSoundsTable &sounds, LuaRef checks) override;
	void AbortHyperjump() override;

	// XXX cockpit is here for now because it has a physics component
	void InitCockpit();
	ShipCockpit *GetCockpit() const;
	void OnCockpitActivated();

	void StaticUpdate(const float timeStep) override;
	virtual vector3d GetManeuverVelocity() const;
	virtual int GetManeuverTime() const;

	sigc::signal<void> onChangeEquipment;
	sigc::signal<void> onPlayerChangeTarget; // navigation or combat

protected:
	void OnEnterSystem() override;
	void OnEnterHyperspace() override;

private:
	std::unique_ptr<ShipCockpit> m_cockpit;
};

#endif /* _PLAYER_H */
