// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Player.h"

#include "Frame.h"
#include "Game.h"
#include "GameConfig.h"
#include "GameConfSingleton.h"
#include "GameLocator.h"
#include "GameLog.h"
#include "HyperspaceCloud.h"
#include "KeyBindings.h"
#include "Lang.h"
#include "LuaObject.h"
#include "ModelCache.h"
#include "Orbit.h"
#include "Pi.h"
#include "Random.h"
#include "RandomSingleton.h"
#include "SectorView.h"
#include "Sfx.h"
#include "ship/PlayerShipController.h"
#include "StringF.h"
#include "SystemView.h" // for the transfer planner
#include "sound/Sound.h"
#include "galaxy/SystemBody.h"

class SpaceStation;

//Some player specific sounds
static Sound::Event s_soundUndercarriage;
static Sound::Event s_soundHyperdrive;

static int onEquipChangeListener(lua_State *l)
{
	Player *p = LuaObject<Player>::GetFromLua(lua_upvalueindex(1));
	p->onChangeEquipment.emit();
	return 0;
}

static void registerEquipChangeListener(Player *player)
{
	lua_State *l = Lua::manager->GetLuaState();
	LUA_DEBUG_START(l);

	LuaObject<Player>::PushToLua(player);
	lua_pushcclosure(l, onEquipChangeListener, 1);
	LuaRef lr(Lua::manager->GetLuaState(), -1);
	ScopedTable(player->GetEquipSet()).CallMethod("AddListener", lr);
	lua_pop(l, 1);

	LUA_DEBUG_END(l, 0);
}

Player::Player(const ShipType::Id &shipId) :
	Ship(shipId)
{
	SetController(new PlayerShipController());
	InitCockpit();
	registerEquipChangeListener(this);
}

Player::Player(const Json &jsonObj, Space *space) :
	Ship(jsonObj, space)
{
	InitCockpit();
	registerEquipChangeListener(this);
}

void Player::SetShipType(const ShipType::Id &shipId)
{
	Ship::SetShipType(shipId);
	registerEquipChangeListener(this);
	InitCockpit();
}

void Player::SaveToJson(Json &jsonObj, Space *space)
{
	Ship::SaveToJson(jsonObj, space);
}

void Player::InitCockpit()
{
	m_cockpit.release();
	if (!GameConfSingleton::getInstance().Int("EnableCockpit"))
		return;

	// XXX select a cockpit model. this is all quite skanky because we want a
	// fallback if the name is not found, which means having to actually try to
	// load the model. but ModelBody (on which ShipCockpit is currently based)
	// requires a model name, not a model object. it won't hurt much because it
	// all stays in the model cache anyway, its just awkward. the fix is to fix
	// ShipCockpit so its not a ModelBody and thus does its model work
	// directly, but we're not there yet
	std::string cockpitModelName;
	if (!GetShipType()->cockpitName.empty()) {
		if (ModelCache::FindModel(GetShipType()->cockpitName, false))
			cockpitModelName = GetShipType()->cockpitName;
	}
	if (cockpitModelName.empty()) {
		if (ModelCache::FindModel("default_cockpit", false))
			cockpitModelName = "default_cockpit";
	}
	if (!cockpitModelName.empty())
		m_cockpit.reset(new ShipCockpit(cockpitModelName));

	OnCockpitActivated();
}

bool Player::DoDamage(float kgDamage)
{
	bool r = Ship::DoDamage(kgDamage);

	// Don't fire audio on EVERY iteration (aka every 16ms, or 60fps), only when exceeds a value randomly
	const float dam = kgDamage * 0.01f;
	if (RandomSingleton::getInstance().Double() < dam) {
		if (!IsDead() && (GetPercentHull() < 25.0f)) {
			Sound::BodyMakeNoise(this, "warning", .5f);
		}
		if (dam < (0.01 * float(GetShipType()->hullMass)))
			Sound::BodyMakeNoise(this, "Hull_hit_Small", 1.0f);
		else
			Sound::BodyMakeNoise(this, "Hull_Hit_Medium", 1.0f);
	}
	return r;
}

//XXX perhaps remove this, the sound is very annoying
bool Player::OnDamage(Object *attacker, float kgDamage, const CollisionContact &contactData)
{
	bool r = Ship::OnDamage(attacker, kgDamage, contactData);
	if (!IsDead() && (GetPercentHull() < 25.0f)) {
		Sound::BodyMakeNoise(this, "warning", .5f);
	}
	return r;
}

//XXX handle killcounts in lua
void Player::SetDockedWith(SpaceStation *s, int port)
{
	Ship::SetDockedWith(s, port);
}

//XXX all ships should make this sound
bool Player::SetWheelState(bool down)
{
	bool did = Ship::SetWheelState(down);
	if (did) {
		s_soundUndercarriage.Play(down ? "UC_out" : "UC_in", 1.0f, 1.0f, 0);
	}
	return did;
}

//XXX all ships should make this sound
Missile *Player::SpawnMissile(ShipType::Id missile_type, int power)
{
	Missile *m = Ship::SpawnMissile(missile_type, power);
	if (m)
		Sound::PlaySfx("Missile_launch", 1.0f, 1.0f, 0);
	return m;
}

//XXX do in lua, or use the alert concept for all ships
void Player::SetAlertState(Ship::AlertState as)
{
	Ship::AlertState prev = GetAlertState();

	switch (as) {
	case ALERT_NONE:
		if (prev != ALERT_NONE)
			GameLocator::getGame()->log->Add(Lang::ALERT_CANCELLED);
		break;

	case ALERT_SHIP_NEARBY:
		if (prev == ALERT_NONE)
			GameLocator::getGame()->log->Add(Lang::SHIP_DETECTED_NEARBY);
		else
			GameLocator::getGame()->log->Add(Lang::DOWNGRADING_ALERT_STATUS);
		Sound::PlaySfx("OK");
		break;

	case ALERT_SHIP_FIRING:
		GameLocator::getGame()->log->Add(Lang::LASER_FIRE_DETECTED);
		Sound::PlaySfx("warning", 0.2f, 0.2f, 0);
		break;
	}

	Ship::SetAlertState(as);
}

void Player::NotifyRemoved(const Body *const removedBody)
{
	if (GetNavTarget() == removedBody)
		SetNavTarget(0);

	if (GetCombatTarget() == removedBody) {
		SetCombatTarget(0);

		if (!GetNavTarget() && removedBody->IsType(Object::SHIP))
			SetNavTarget(static_cast<const Ship *>(removedBody)->GetHyperspaceCloud());
	}

	Ship::NotifyRemoved(removedBody);
}

//XXX ui stuff
void Player::OnEnterHyperspace()
{
	s_soundHyperdrive.Play(m_hyperspace.sounds.jump_sound.c_str());
	SetNavTarget(0);
	SetCombatTarget(0);

	m_controller->SetFlightControlState(CONTROL_MANUAL); //could set CONTROL_HYPERDRIVE
	ClearThrusterState();
	GameLocator::getGame()->WantHyperspace();
}

void Player::OnEnterSystem()
{
	m_controller->SetFlightControlState(CONTROL_MANUAL);
	//XXX don't call sectorview from here, use signals instead
	GameLocator::getGame()->GetSectorView()->ResetHyperspaceTarget();
}

//temporary targeting stuff
PlayerShipController *Player::GetPlayerController() const
{
	return static_cast<PlayerShipController *>(GetController());
}

Body *Player::GetCombatTarget() const
{
	return static_cast<PlayerShipController *>(m_controller)->GetCombatTarget();
}

Body *Player::GetNavTarget() const
{
	return static_cast<PlayerShipController *>(m_controller)->GetNavTarget();
}

Body *Player::GetSetSpeedTarget() const
{
	return static_cast<PlayerShipController *>(m_controller)->GetSetSpeedTarget();
}

void Player::SetCombatTarget(Body *const target, bool setSpeedTo)
{
	static_cast<PlayerShipController *>(m_controller)->SetCombatTarget(target, setSpeedTo);
	Pi::onPlayerChangeTarget.emit();
}

void Player::SetNavTarget(Body *const target, bool setSpeedTo)
{
	static_cast<PlayerShipController *>(m_controller)->SetNavTarget(target, setSpeedTo);
	Pi::onPlayerChangeTarget.emit();
}

void Player::SetSetSpeedTarget(Body *const target)
{
	static_cast<PlayerShipController *>(m_controller)->SetSetSpeedTarget(target);
	// TODO: not sure, do we actually need this? we are only changing the set speed target
	Pi::onPlayerChangeTarget.emit();
}

void Player::ChangeSetSpeed(double delta)
{
	static_cast<PlayerShipController *>(m_controller)->ChangeSetSpeed(delta);
}

//temporary targeting stuff ends

Ship::HyperjumpStatus Player::InitiateHyperjumpTo(const SystemPath &dest, int warmup_time, double duration, const HyperdriveSoundsTable &sounds, LuaRef checks)
{
	HyperjumpStatus status = Ship::InitiateHyperjumpTo(dest, warmup_time, duration, sounds, checks);

	if (status == HYPERJUMP_OK)
		s_soundHyperdrive.Play(m_hyperspace.sounds.warmup_sound.c_str());

	return status;
}

void Player::AbortHyperjump()
{
	s_soundHyperdrive.Play(m_hyperspace.sounds.abort_sound.c_str());
	Ship::AbortHyperjump();
}

void Player::OnCockpitActivated()
{
	if (m_cockpit)
		m_cockpit->OnActivated(this);
}

void Player::StaticUpdate(const float timeStep)
{
	Ship::StaticUpdate(timeStep);

	// XXX even when not on screen. hacky, but really cockpit shouldn't be here
	// anyway so this will do for now
	if (m_cockpit)
		m_cockpit->Update(this, timeStep);
}

int Player::GetManeuverTime() const
{
	if (Pi::planner->GetOffsetVel().ExactlyEqual(vector3d(0, 0, 0))) {
		return 0;
	}
	return Pi::planner->GetStartTime();
}

vector3d Player::GetManeuverVelocity() const
{
	Frame *frame = Frame::GetFrame(GetFrame());
	if (frame->IsRotFrame())
		frame = Frame::GetFrame(frame->GetNonRotFrame());
	const SystemBody *systemBody = frame->GetSystemBody();

	if (Pi::planner->GetOffsetVel().ExactlyEqual(vector3d(0, 0, 0))) {
		return vector3d(0, 0, 0);
	} else if (systemBody) {
		Orbit playerOrbit = ComputeOrbit();
		if (!is_zero_exact(playerOrbit.GetSemiMajorAxis())) {
			double mass = systemBody->GetMass();
			// XXX The best solution would be to store the mass(es) on Orbit
			const vector3d velocity = (Pi::planner->GetVel() - playerOrbit.OrbitalVelocityAtTime(mass, playerOrbit.OrbitalTimeAtPos(Pi::planner->GetPosition(), mass)));
			return velocity;
		}
	}
	return vector3d(0, 0, 0);
}
