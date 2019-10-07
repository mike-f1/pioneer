// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Ship.h"

#include "CargoBody.h"
#include "CityOnPlanet.h"
#include "EnumStrings.h"
#include "Frame.h"
#include "Game.h"
#include "GameLog.h"
#include "GameSaveError.h"
#include "HeatGradientPar.h"
#include "HyperspaceCloud.h"
#include "Lang.h"
#include "LuaEvent.h"
#include "LuaObject.h"
#include "LuaUtils.h"
#include "Missile.h"
#include "NavLights.h"
#include "Planet.h"
#include "Player.h" // <-- Here only for 1 occurence of "Pi::player" in Ship::Explode
#include "Sensors.h"
#include "Sfx.h"
#include "Shields.h"
#include "ShipAICmd.h"
#include "Space.h"
#include "SpaceStation.h"
#include "StringF.h"
#include "WorldView.h"
#include "collider/CollisionContact.h"
#include "graphics/TextureBuilder.h"
#include "scenegraph/Animation.h"
#include "scenegraph/MatrixTransform.h"
#include "ship/PlayerShipController.h"

static const float TONS_HULL_PER_SHIELD = 10.f;
HeatGradientParameters_t Ship::s_heatGradientParams;
const float Ship::DEFAULT_SHIELD_COOLDOWN_TIME = 1.0f;
const double Ship::DEFAULT_LIFT_TO_DRAG_RATIO = 0.001;

Ship::Ship(const ShipType::Id &shipId) :
	DynamicBody(),
	m_controller(0),
	m_flightState(FLYING),
	m_alertState(ALERT_NONE),
	m_landingGearAnimation(nullptr)
{
	/*
		THIS CODE DOES NOT RUN WHEN LOADING SAVEGAMES!!
	*/
	AddFeature(Feature::PROPULSION); // add component propulsion
	AddFeature(Feature::FIXED_GUNS); // add component fixed guns
	Properties().Set("flightState", EnumStrings::GetString("ShipFlightState", m_flightState));
	Properties().Set("alertStatus", EnumStrings::GetString("ShipAlertStatus", m_alertState));

	SetFuel(1.0);
	SetFuelReserve(0.0);
	m_lastAlertUpdate = 0.0;
	m_lastFiringAlert = 0.0;
	m_shipNear = false;
	m_shipFiring = false;

	m_testLanded = false;
	m_launchLockTimeout = 0;
	m_wheelTransition = 0;
	m_wheelState = 0;
	m_dockedWith = nullptr;
	m_dockedWithPort = 0;
	SetShipId(shipId);
	ClearAngThrusterState();
	ClearLinThrusterState();

	InitEquipSet();

	m_hyperspace.countdown = 0;
	m_hyperspace.now = false;
	GetFixedGuns()->Init(this);
	m_ecmRecharge = 0;
	m_shieldCooldown = 0.0f;
	m_curAICmd = 0;
	m_aiMessage = AIERROR_NONE;
	m_decelerating = false;

	SetModel(m_type->modelName.c_str());
	// Setting thrusters colors
	if (m_type->isGlobalColorDefined) GetModel()->SetThrusterColor(m_type->globalThrusterColor);
	for (int i = 0; i < THRUSTER_MAX; i++) {
		if (!m_type->isDirectionColorDefined[i]) continue;
		vector3f dir;
		switch (i) {
		case THRUSTER_FORWARD: dir = vector3f(0.0, 0.0, 1.0); break;
		case THRUSTER_REVERSE: dir = vector3f(0.0, 0.0, -1.0); break;
		case THRUSTER_LEFT: dir = vector3f(1.0, 0.0, 0.0); break;
		case THRUSTER_RIGHT: dir = vector3f(-1.0, 0.0, 0.0); break;
		case THRUSTER_UP: dir = vector3f(1.0, 0.0, 0.0); break;
		case THRUSTER_DOWN: dir = vector3f(-1.0, 0.0, 0.0); break;
		}
		GetModel()->SetThrusterColor(dir, m_type->directionThrusterColor[i]);
	}
	SetLabel("UNLABELED_SHIP");
	m_skin.SetRandomColors(Pi::rng);
	m_skin.SetDecal(m_type->manufacturer);
	m_skin.Apply(GetModel());
	if (GetModel()->SupportsPatterns())
		GetModel()->SetPattern(Pi::rng.Int32(0, GetModel()->GetNumPatterns() - 1));

	Init();
	SetController(new ShipController());
}

Ship::Ship(const Json &jsonObj, Space *space) :
	DynamicBody(jsonObj, space)
{
	AddFeature(Feature::PROPULSION); // add component propulsion
	AddFeature(Feature::FIXED_GUNS); // add component fixed guns

	try {
		Json shipObj = jsonObj["ship"];

		GetPropulsion()->LoadFromJson(shipObj, space);

		SetShipId(shipObj["ship_type_id"]); // XXX handle missing thirdparty ship
		GetPropulsion()->SetFuelTankMass(GetShipType()->fuelTankMass);
		m_stats.fuel_tank_mass_left = GetPropulsion()->FuelTankMassLeft();

		m_skin.LoadFromJson(shipObj);
		m_skin.Apply(GetModel());
		// needs fixups
		m_wheelTransition = shipObj["wheel_transition"];
		m_wheelState = shipObj["wheel_state"];
		m_launchLockTimeout = shipObj["launch_lock_timeout"];
		m_testLanded = shipObj["test_landed"];
		m_flightState = shipObj["flight_state"];

		m_lastAlertUpdate = 0.0; // alertstate check cache timer
		m_shipNear = false; // alertstate check cache value
		m_shipFiring = false; // alertstate check cache value

		m_alertState = shipObj["alert_state"];
		Properties().Set("flightState", EnumStrings::GetString("ShipFlightState", m_flightState));
		Properties().Set("alertStatus", EnumStrings::GetString("ShipAlertStatus", m_alertState));
		m_lastFiringAlert = shipObj["last_firing_alert"];

		Json hyperspaceDestObj = shipObj["hyperspace_destination"];
		m_hyperspace.dest = SystemPath::FromJson(hyperspaceDestObj);
		m_hyperspace.countdown = shipObj["hyperspace_countdown"];
		m_hyperspace.duration = 0;
		m_hyperspace.sounds.warmup_sound = shipObj.value("hyperspace_warmup_sound", "");
		m_hyperspace.sounds.abort_sound = shipObj.value("hyperspace_abort_sound", "");
		m_hyperspace.sounds.jump_sound = shipObj.value("hyperspace_jump_sound", "");

		GetFixedGuns()->LoadFromJson(shipObj, space);

		m_ecmRecharge = shipObj["ecm_recharge"];
		SetShipId(shipObj["ship_type_id"]); // XXX handle missing thirdparty ship
		m_dockedWithPort = shipObj["docked_with_port"];
		m_dockedWithIndex = shipObj["index_for_body_docked_with"];
		Init();
		m_stats.hull_mass_left = shipObj["hull_mass_left"]; // must be after Init()...
		m_stats.shield_mass_left = shipObj["shield_mass_left"];
		m_shieldCooldown = shipObj["shield_cooldown"];
		m_curAICmd = 0;
		m_curAICmd = AICommand::LoadFromJson(shipObj);
		m_aiMessage = AIError(shipObj["ai_message"]);

		PropertyMap &p = Properties();

		p.Set("hullMassLeft", m_stats.hull_mass_left);
		p.Set("hullPercent", 100.0f * (m_stats.hull_mass_left / float(m_type->hullMass)));
		p.Set("shieldMassLeft", m_stats.shield_mass_left);
		p.Set("fuelMassLeft", m_stats.fuel_tank_mass_left);
		p.PushLuaTable();
		lua_State *l = Lua::manager->GetLuaState();
		lua_getfield(l, -1, "equipSet");
		m_equipSet = LuaRef(l, -1);
		lua_pop(l, 2);

		m_controller = 0;
		const ShipController::Type ctype = shipObj["controller_type"];
		if (ctype == ShipController::PLAYER)
			SetController(new PlayerShipController());
		else
			SetController(new ShipController());
		m_controller->LoadFromJson(shipObj);

		m_navLights->LoadFromJson(shipObj);

		m_shipName = shipObj["name"];
		Properties().Set("shipName", m_shipName);
	} catch (Json::type_error &) {
		throw SavedGameCorruptException();
	}
}

Ship::~Ship()
{
	if (m_curAICmd) delete m_curAICmd;
	delete m_controller;
}

void Ship::Init()
{
	m_invulnerable = false;

	m_sensors.reset(new Sensors(this));

	m_navLights.reset(new NavLights(GetModel()));
	m_navLights->SetEnabled(true);

	SetMassDistributionFromModel();
	UpdateEquipStats();
	m_stats.hull_mass_left = float(m_type->hullMass);
	m_stats.shield_mass_left = 0;

	PropertyMap &p = Properties();
	p.Set("hullMassLeft", m_stats.hull_mass_left);
	p.Set("hullPercent", 100.0f * (m_stats.hull_mass_left / float(m_type->hullMass)));
	p.Set("shieldMassLeft", m_stats.shield_mass_left);
	p.Set("fuelMassLeft", m_stats.fuel_tank_mass_left);

	// Init of Propulsion:
	GetPropulsion()->Init(this, GetModel(), m_type->fuelTankMass, m_type->effectiveExhaustVelocity, m_type->linThrust, m_type->angThrust, m_type->linAccelerationCap);

	p.Set("shipName", m_shipName);

	m_hyperspace.now = false; // TODO: move this on next savegame change, maybe
	m_hyperspaceCloud = 0;

	m_landingGearAnimation = GetModel()->FindAnimation("gear_down");

	GetFixedGuns()->InitGuns(GetModel());

	// If we've got the tag_landing set then use it for an offset
	// otherwise use zero so that it will dock but look clearly incorrect
	const SceneGraph::MatrixTransform *mt = GetModel()->FindTagByName("tag_landing");
	if (mt) {
		m_landingMinOffset = mt->GetTransform().GetTranslate().y;
	} else {
		m_landingMinOffset = 0.0; // GetAabb().min.y;
	}

	InitMaterials();
}

void Ship::PostLoadFixup(Space *space)
{
	DynamicBody::PostLoadFixup(space);
	m_dockedWith = static_cast<SpaceStation *>(space->GetBodyByIndex(m_dockedWithIndex));
	if (m_curAICmd) m_curAICmd->PostLoadFixup(space);
	m_controller->PostLoadFixup(space);
}

void Ship::SaveToJson(Json &jsonObj, Space *space)
{
	DynamicBody::SaveToJson(jsonObj, space);

	Json shipObj({}); // Create JSON object to contain ship data.

	GetPropulsion()->SaveToJson(shipObj, space);

	m_skin.SaveToJson(shipObj);
	shipObj["wheel_transition"] = m_wheelTransition;
	shipObj["wheel_state"] = m_wheelState;
	shipObj["launch_lock_timeout"] = m_launchLockTimeout;
	shipObj["test_landed"] = m_testLanded;
	shipObj["flight_state"] = int(m_flightState);
	shipObj["alert_state"] = int(m_alertState);
	shipObj["last_firing_alert"] = m_lastFiringAlert;

	// XXX make sure all hyperspace attrs and the cloud get saved
	Json hyperspaceDestObj({}); // Create JSON object to contain hyperspace destination data.
	m_hyperspace.dest.ToJson(hyperspaceDestObj);
	shipObj["hyperspace_destination"] = hyperspaceDestObj; // Add hyperspace destination object to ship object.
	shipObj["hyperspace_countdown"] = m_hyperspace.countdown;
	shipObj["hyperspace_warmup_sound"] = m_hyperspace.sounds.warmup_sound;
	shipObj["hyperspace_abort_sound"] = m_hyperspace.sounds.abort_sound;
	shipObj["hyperspace_jump_sound"] = m_hyperspace.sounds.jump_sound;

	GetFixedGuns()->SaveToJson(shipObj, space);

	shipObj["ecm_recharge"] = m_ecmRecharge;
	shipObj["ship_type_id"] = m_type->id;
	shipObj["docked_with_port"] = m_dockedWithPort;
	shipObj["index_for_body_docked_with"] = space->GetIndexForBody(m_dockedWith);
	shipObj["hull_mass_left"] = m_stats.hull_mass_left;
	shipObj["shield_mass_left"] = m_stats.shield_mass_left;
	shipObj["shield_cooldown"] = m_shieldCooldown;
	if (m_curAICmd) m_curAICmd->SaveToJson(shipObj);
	shipObj["ai_message"] = int(m_aiMessage);

	shipObj["controller_type"] = static_cast<int>(m_controller->GetType());
	m_controller->SaveToJson(shipObj, space);

	m_navLights->SaveToJson(shipObj);

	shipObj["name"] = m_shipName;

	jsonObj["ship"] = shipObj; // Add ship object to supplied object.
}

void Ship::InitEquipSet()
{
	lua_State *l = Lua::manager->GetLuaState();
	PropertyMap &p = Properties();
	LUA_DEBUG_START(l);
	pi_lua_import(l, "EquipSet");
	LuaTable es_class(l, -1);
	LuaTable slots = LuaTable(l).LoadMap(GetShipType()->slots.begin(), GetShipType()->slots.end());
	m_equipSet = es_class.Call<LuaRef>("New", slots);
	p.Set("equipSet", ScopedTable(m_equipSet));
	UpdateEquipStats();
	{
		ScopedTable es(m_equipSet);
		int usedCargo = es.CallMethod<int>("OccupiedSpace", "cargo");
		int totalCargo = std::min(m_stats.free_capacity + usedCargo, es.CallMethod<int>("SlotSize", "cargo"));
		p.Set("usedCargo", usedCargo);
		p.Set("totalCargo", totalCargo);
	}
	lua_pop(l, 2);
	LUA_DEBUG_END(l, 0);
}

void Ship::InitMaterials()
{
	SceneGraph::Model *pModel = GetModel();
	assert(pModel);
	const Uint32 numMats = pModel->GetNumMaterials();
	for (Uint32 m = 0; m < numMats; m++) {
		RefCountedPtr<Graphics::Material> mat = pModel->GetMaterialByIndex(m);
		mat->heatGradient = Graphics::TextureBuilder::Decal("textures/heat_gradient.dds").GetOrCreateTexture(Pi::renderer, "model");
		mat->specialParameter0 = &s_heatGradientParams;
	}
	s_heatGradientParams.heatingAmount = 0.0f;
	s_heatGradientParams.heatingNormal = vector3f(0.0f, -1.0f, 0.0f);
}

void Ship::SetController(ShipController *c)
{
	assert(c != 0);
	if (m_controller) delete m_controller;
	m_controller = c;
	m_controller->m_ship = this;
}

float Ship::GetPercentHull() const
{
	return 100.0f * (m_stats.hull_mass_left / float(m_type->hullMass));
}

float Ship::GetPercentShields() const
{
	if (m_stats.shield_mass <= 0)
		return 100.0f;
	else
		return 100.0f * (m_stats.shield_mass_left / m_stats.shield_mass);
}

void Ship::SetPercentHull(float p)
{
	m_stats.hull_mass_left = 0.01f * Clamp(p, 0.0f, 100.0f) * float(m_type->hullMass);
	Properties().Set("hullMassLeft", m_stats.hull_mass_left);
	Properties().Set("hullPercent", 100.0f * (m_stats.hull_mass_left / float(m_type->hullMass)));
}

void Ship::UpdateMass()
{
	SetMass(((double)m_stats.static_mass + GetPropulsion()->FuelTankMassLeft()) * 1000);
}

template <typename T>
inline int sign(T num)
{
	return (num >= 0) - (num < 0);
}

vector3d Ship::CalcAtmosphericForce() const
{
	// Data from ship.
	double m_topCrossSec = GetShipType()->topCrossSection;
	double m_sideCrossSec = GetShipType()->sideCrossSection;
	double m_frontCrossSec = GetShipType()->frontCrossSection;

	// TODO: vary drag coefficient based on Reynolds number, specifically by
	// atmospheric composition (viscosity) and airspeed (mach number).
	double m_topDragCoeff = GetShipType()->topDragCoeff;
	double m_sideDragCoeff = GetShipType()->sideDragCoeff;
	double m_frontDragCoeff = GetShipType()->frontDragCoeff;

	double m_shipLiftCoeff = GetShipType()->shipLiftCoefficient;

	// By converting the velocity into local space, we can apply the drag individually to each component.
	auto m_localVel = GetVelocity() * GetOrient();
	auto m_lVSqr = m_localVel.LengthSqr();

	// The drag forces applied to the craft, in local space.
	// TODO: verify dimensional accuracy and that we're not generating more drag than physically possible.
	// TODO: use a different drag constant for each side (front, back, etc).
	// This also handles (most of) the lift due to wing deflection.
	vector3d fAtmosDrag = vector3d(
		CalcAtmosphericDrag(m_lVSqr, m_sideCrossSec, m_sideDragCoeff),
		CalcAtmosphericDrag(m_lVSqr, m_topCrossSec, m_topDragCoeff),
		CalcAtmosphericDrag(m_lVSqr, m_frontCrossSec, m_frontDragCoeff));

	// The direction vector of the velocity also serves to scale and sign the generated drag.
	fAtmosDrag = fAtmosDrag * -m_localVel.NormalizedSafe();

	// The amount of lift produced by air pressure differential across the top and bottom of the lifting surfaces.
	vector3d fAtmosLift = vector3d(0.0);

	double m_AoAMultiplier = m_localVel.NormalizedSafe().y;

	// There's no lift produced once the wing hits the stall angle.
	if (std::abs(m_AoAMultiplier) < 0.61) {
		// Pioneer simulates non-cambered wings, with equal air displacement on either side of AoA.

		// Generated lift peaks at around 20 degrees here, and falls off fully at 35-ish.
		// TODO: handle AoA better / more gracefully with an actual angle- and curve-based implementation.
		m_AoAMultiplier = cos((std::abs(m_AoAMultiplier) - 0.31) * 5.0) * sign(m_AoAMultiplier);

		// TODO: verify dimensional accuracy and that we're not generating more lift than physically possible.
		// We scale down the lift contribution because fAtmosDrag handles deflection-based lift.
		fAtmosLift.y = CalcAtmosphericDrag(pow(m_localVel.z, 2), m_topCrossSec, m_shipLiftCoeff) * -m_AoAMultiplier * 0.2;
	}

	return GetOrient() * (fAtmosDrag + fAtmosLift);
}

//calculates torque to force the spacecraft go nose-first in atmosphere
vector3d Ship::CalcAtmoTorque() const
{
	double m_topCrossSec = GetShipType()->topCrossSection;
	double m_sideCrossSec = GetShipType()->sideCrossSection;
	double m_frontCrossSec = GetShipType()->frontCrossSection;
	double m_aeroStabilityMultiplier = GetShipType()->atmoStability;

	vector3d forward = GetOrient().VectorZ();
	vector3d m_vel = GetVelocity().NormalizedSafe();
	vector3d m_torqueDir = -m_vel.Cross(-forward); // <--- This is correct

	// TODO: evaluate this function and properly implement based upon ship cross-section.
	double m_drag = CalcAtmosphericDrag(GetVelocity().LengthSqr(), m_topCrossSec, DEFAULT_DRAG_COEFF);
	vector3d fAtmoTorque = vector3d(0.0);

	if (GetVelocity().Length() > 100) { //don't apply torque at minimal speeds
		fAtmoTorque = m_drag * m_torqueDir * ((m_topCrossSec + m_sideCrossSec) / (m_frontCrossSec * 4)) * 0.3 * m_aeroStabilityMultiplier * Pi::game->GetInvTimeAccelRate();
	}

	return fAtmoTorque;
}

bool Ship::OnDamage(Object *attacker, float kgDamage, const CollisionContact &contactData)
{
	if (m_invulnerable) {
		Sound::BodyMakeNoise(this, "Hull_hit_Small", 0.5f);
		return true;
	}

	if (!IsDead()) {
		float dam = kgDamage * 0.001f;
		if (m_stats.shield_mass_left > 0.0f) {
			if (m_stats.shield_mass_left > dam) {
				m_stats.shield_mass_left -= dam;
				dam = 0;
			} else {
				dam -= m_stats.shield_mass_left;
				m_stats.shield_mass_left = 0;
			}
			Properties().Set("shieldMassLeft", m_stats.shield_mass_left);
		}

		m_shieldCooldown = DEFAULT_SHIELD_COOLDOWN_TIME;
		// transform the collision location into the models local space (from world space) and add it as a hit.
		matrix4x4d mtx = GetOrient();
		mtx.SetTranslate(GetPosition());
		const matrix4x4d invmtx = mtx.Inverse();
		const vector3d localPos = invmtx * contactData.pos;
		GetShields()->AddHit(localPos);

		m_stats.hull_mass_left -= dam;
		Properties().Set("hullMassLeft", m_stats.hull_mass_left);
		Properties().Set("hullPercent", 100.0f * (m_stats.hull_mass_left / float(m_type->hullMass)));
		if (m_stats.hull_mass_left < 0) {
			if (attacker) {
				if (attacker->IsType(Object::BODY))
					LuaEvent::Queue("onShipDestroyed", this, dynamic_cast<Body *>(attacker));
				else if (attacker->IsType(Object::CITYONPLANET))
					LuaEvent::Queue("onShipDestroyed", this, dynamic_cast<CityOnPlanet *>(attacker)->GetPlanet());
			}

			Explode();
		} else {
			if (Pi::rng.Double() < kgDamage)
				SfxManager::Add(this, TYPE_DAMAGE);

			if (dam > float(GetShipType()->hullMass / 1000.)) {
				if (dam < 0.01 * float(GetShipType()->hullMass))
					Sound::BodyMakeNoise(this, "Hull_hit_Small", 1.0f);
				else
					Sound::BodyMakeNoise(this, "Hull_Hit_Medium", 1.0f);
			}
		}
	}

	//Output("Ouch! %s took %.1f kilos of damage from %s! (%.1f t hull left)\n", GetLabel().c_str(), kgDamage, attacker->GetLabel().c_str(), m_stats.hull_mass_left);
	return true;
}

bool Ship::OnCollision(Object *b, Uint32 flags, double relVel)
{
	// Collision with SpaceStation docking surface is
	// completely handled by SpaceStations, you only
	// need to return a "true" value in order to trigger
	// a bounce in Space::OnCollision
	// NOTE: 0x10 is a special flag set on docking surfaces
	if (b->IsType(Object::SPACESTATION) && (flags & 0x10)) {
		return true;
	}

	// hitting cargo scoop surface shouldn't do damage
	int cargoscoop_cap = 0;
	Properties().Get("cargo_scoop_cap", cargoscoop_cap);
	if (cargoscoop_cap > 0 && b->IsType(Object::CARGOBODY) && !dynamic_cast<Body *>(b)->IsDead()) {
		LuaRef item = dynamic_cast<CargoBody *>(b)->GetCargoType();
		if (LuaObject<Ship>::CallMethod<int>(this, "AddEquip", item) > 0) { // try to add it to the ship cargo.
			Pi::game->GetSpace()->KillBody(dynamic_cast<Body *>(b));
			if (this->IsType(Object::PLAYER))
				Pi::game->log->Add(stringf(Lang::CARGO_SCOOP_ACTIVE_1_TONNE_X_COLLECTED, formatarg("item", ScopedTable(item).CallMethod<std::string>("GetName"))));
			// XXX SfxManager::Add(this, TYPE_SCOOP);
			UpdateEquipStats();
			return true;
		}
		if (this->IsType(Object::PLAYER))
			Pi::game->log->Add(Lang::CARGO_SCOOP_ATTEMPTED);
	}

	if (b->IsType(Object::PLANET)) {
		// geoms still enabled when landed
		if (m_flightState != FLYING)
			return false;
		else {
			if (GetVelocity().Length() < MAX_LANDING_SPEED) {
				m_testLanded = true;
				return true;
			}
		}
	}

	if (b->IsType(Object::CITYONPLANET) ||
		b->IsType(Object::SHIP) ||
		b->IsType(Object::PLAYER) ||
		b->IsType(Object::SPACESTATION) ||
		b->IsType(Object::PLANET) ||
		b->IsType(Object::STAR) ||
		b->IsType(Object::CARGOBODY)) {
		LuaEvent::Queue("onShipCollided", this,
			b->IsType(Object::CITYONPLANET) ? dynamic_cast<CityOnPlanet *>(b)->GetPlanet() : dynamic_cast<Body *>(b));
	}

	return DynamicBody::OnCollision(b, flags, relVel);
}

//destroy ship in an explosion
void Ship::Explode()
{
	if (m_invulnerable) return;

	Pi::game->GetSpace()->KillBody(this);
	if (this->GetFrame() == Pi::player->GetFrame()) {
		SfxManager::AddExplosion(this);
		Sound::BodyMakeNoise(this, "Explosion_1", 1.0f);
	}
	ClearThrusterState();
}

bool Ship::DoDamage(float kgDamage)
{
	if (m_invulnerable) {
		return true;
	}

	if (!IsDead()) {
		float dam = kgDamage * 0.01f;
		if (m_stats.shield_mass_left > 0.0f) {
			if (m_stats.shield_mass_left > dam) {
				m_stats.shield_mass_left -= dam;
				dam = 0;
			} else {
				dam -= m_stats.shield_mass_left;
				m_stats.shield_mass_left = 0;
			}
			Properties().Set("shieldMassLeft", m_stats.shield_mass_left);
		}

		m_shieldCooldown = DEFAULT_SHIELD_COOLDOWN_TIME;
		// create a collision location in the models local space and add it as a hit.
		Random rnd;
		rnd.seed(time(0));
		const vector3d randPos(
			rnd.Double() * 2.0 - 1.0,
			rnd.Double() * 2.0 - 1.0,
			rnd.Double() * 2.0 - 1.0);
		GetShields()->AddHit(randPos * (GetPhysRadius() * 0.75));

		m_stats.hull_mass_left -= dam;
		Properties().Set("hullMassLeft", m_stats.hull_mass_left);
		Properties().Set("hullPercent", 100.0f * (m_stats.hull_mass_left / float(m_type->hullMass)));
		if (m_stats.hull_mass_left < 0) {
			Explode();
		} else {
			if (Pi::rng.Double() < dam)
				SfxManager::Add(this, TYPE_DAMAGE);
		}
	}

	//Output("Ouch! %s took %.1f kilos of damage from %s! (%.1f t hull left)\n", GetLabel().c_str(), kgDamage, attacker->GetLabel().c_str(), m_stats.hull_mass_left);
	return true;
}

void Ship::UpdateEquipStats()
{
	PropertyMap &p = Properties();

	m_stats.used_capacity = 0;
	p.Get("mass_cap", m_stats.used_capacity);
	m_stats.used_cargo = 0;

	m_stats.free_capacity = m_type->capacity - m_stats.used_capacity;
	m_stats.static_mass = m_stats.used_capacity + m_type->hullMass;

	p.Set("usedCapacity", m_stats.used_capacity);

	p.Set("freeCapacity", m_stats.free_capacity);
	p.Set("totalMass", m_stats.static_mass);
	p.Set("staticMass", m_stats.static_mass);

	int shield_cap = 0;
	Properties().Get("shield_cap", shield_cap);
	m_stats.shield_mass = TONS_HULL_PER_SHIELD * float(shield_cap);
	p.Set("shieldMass", m_stats.shield_mass);

	UpdateFuelStats();
	UpdateGunsStats();

	unsigned int thruster_power_cap = 0;
	Properties().Get("thruster_power_cap", thruster_power_cap);
	const double power_mul = m_type->thrusterUpgrades[Clamp(thruster_power_cap, 0U, 3U)];
	GetPropulsion()->SetThrustPowerMult(power_mul, m_type->linThrust, m_type->angThrust);

	m_stats.hyperspace_range = m_stats.hyperspace_range_max = 0;
	p.Set("hyperspaceRange", m_stats.hyperspace_range);
	p.Set("maxHyperspaceRange", m_stats.hyperspace_range_max);
}

void Ship::UpdateLuaStats()
{
	// This code cannot be in UpdateEquipStats itself because *Equip* needs to be
	// called in Init(), which is itself called in the constructor, but we absolutely
	// cannot use LuaObject<Ship>::* in a constructor, or else we'd fix the type of the
	// object to Ship forever, even though it could very well be a Player.
	// Updates at Gen 2019: Indeed, this function has been removed from
	// loading because loading is now a ctor: see Body.cpp
	UpdateEquipStats();
	PropertyMap &p = Properties();
	m_stats.hyperspace_range = m_stats.hyperspace_range_max = 0;
	int hyperclass = 0;
	p.Get<int>("hyperclass_cap", hyperclass);
	if (hyperclass) {
		std::tie(m_stats.hyperspace_range_max, m_stats.hyperspace_range) =
			LuaObject<Ship>::CallMethod<double, double>(this, "GetHyperspaceRange");
	}

	p.Set("hyperspaceRange", m_stats.hyperspace_range);
	p.Set("maxHyperspaceRange", m_stats.hyperspace_range_max);
}

void Ship::UpdateGunsStats()
{

	float cooler = 1.0f;
	Properties().Get("laser_cooler_cap", cooler);
	GetFixedGuns()->SetCoolingBoost(cooler);

	for (int num = 0; num < 2; num++) {
		std::string prefix(num ? "laser_rear_" : "laser_front_");
		int damage = 0;
		Properties().Get(prefix + "damage", damage);
		if (!damage) {
			GetFixedGuns()->UnMountGun(num);
		} else {
			Properties().PushLuaTable();
			LuaTable prop(Lua::manager->GetLuaState(), -1);

			const Color c(
				prop.Get<float>(prefix + "rgba_r"),
				prop.Get<float>(prefix + "rgba_g"),
				prop.Get<float>(prefix + "rgba_b"),
				prop.Get<float>(prefix + "rgba_a"));
			const float heatrate = prop.Get<float>(prefix + "heatrate", 0.01f);
			const float coolrate = prop.Get<float>(prefix + "coolrate", 0.01f);
			const float lifespan = prop.Get<float>(prefix + "lifespan");
			const float width = prop.Get<float>(prefix + "width");
			const float length = prop.Get<float>(prefix + "length");
			const bool mining = prop.Get<int>(prefix + "mining");
			const float speed = prop.Get<float>(prefix + "speed");
			const float recharge = prop.Get<float>(prefix + "rechargeTime");
			const bool beam = prop.Get<int>(prefix + "beam");

			GetFixedGuns()->MountGun(num, recharge, lifespan, damage, length, width, mining, c, speed, beam, heatrate, coolrate);

			if (prop.Get<int>(prefix + "dual"))
				GetFixedGuns()->IsDual(num, true);
			else
				GetFixedGuns()->IsDual(num, false);
			lua_pop(prop.GetLua(), 1);
		}
	}
}

void Ship::UpdateFuelStats()
{
	m_stats.fuel_tank_mass_left = GetPropulsion()->FuelTankMassLeft();
	Properties().Set("fuelMassLeft", m_stats.fuel_tank_mass_left);

	UpdateMass();
}

Ship::HyperjumpStatus Ship::CheckHyperjumpCapability() const
{
	if (GetFlightState() == HYPERSPACE)
		return HYPERJUMP_DRIVE_ACTIVE;

	if (GetFlightState() != FLYING && GetFlightState() != JUMPING)
		return HYPERJUMP_SAFETY_LOCKOUT;

	return HYPERJUMP_OK;
}

Ship::HyperjumpStatus Ship::InitiateHyperjumpTo(const SystemPath &dest, int warmup_time, double duration, const HyperdriveSoundsTable &sounds, LuaRef checks)
{
	if (!dest.HasValidSystem() || GetFlightState() != FLYING || warmup_time < 1)
		return HYPERJUMP_SAFETY_LOCKOUT;
	StarSystem *s = Pi::game->GetSpace()->GetStarSystem().Get();
	if (s && s->GetPath().IsSameSystem(dest))
		return HYPERJUMP_CURRENT_SYSTEM;

	m_hyperspace.dest = dest;
	m_hyperspace.countdown = warmup_time;
	m_hyperspace.now = false;
	m_hyperspace.duration = duration;
	m_hyperspace.checks = checks;
	m_hyperspace.sounds = sounds;

	return Ship::HYPERJUMP_OK;
}

void Ship::AbortHyperjump()
{
	m_hyperspace.countdown = 0;
	m_hyperspace.now = false;
	m_hyperspace.duration = 0;
	m_hyperspace.checks = LuaRef();
}

float Ship::GetECMRechargeTime()
{
	float ecm_recharge_cap = 0.f;
	Properties().Get("ecm_recharge_cap", ecm_recharge_cap);
	return ecm_recharge_cap;
}

Ship::ECMResult Ship::UseECM()
{
	int ecm_power_cap = 0;
	Properties().Get("ecm_power_cap", ecm_power_cap);
	if (m_ecmRecharge > 0.0f) return ECM_RECHARGING;

	if (ecm_power_cap > 0) {
		Sound::BodyMakeNoise(this, "ECM", 1.0f);
		m_ecmRecharge = GetECMRechargeTime();

		// damage neaby missiles
		const float ECM_RADIUS = 4000.0f;

		Space::BodyNearList nearby = Pi::game->GetSpace()->GetBodiesMaybeNear(this, ECM_RADIUS);
		for (Body *body : nearby) {
			if (body->GetFrame() != GetFrame()) continue;
			if (!body->IsType(Object::MISSILE)) continue;

			double dist = (body->GetPosition() - GetPosition()).Length();
			if (dist < ECM_RADIUS) {
				// increasing chance of destroying it with proximity
				if (Pi::rng.Double() > (dist / ECM_RADIUS)) {
					static_cast<Missile *>(body)->ECMAttack(ecm_power_cap);
				}
			}
		}
		return ECM_ACTIVATED;
	} else
		return ECM_NOT_INSTALLED;
}

Missile *Ship::SpawnMissile(ShipType::Id missile_type, int power)
{
	if (GetFlightState() != FLYING)
		return 0;

	Missile *missile = new Missile(missile_type, this, power);
	missile->SetOrient(GetOrient());
	missile->SetFrame(GetFrame());
	const vector3d pos = GetOrient() * vector3d(0, GetAabb().min.y - 10, GetAabb().min.z);
	const vector3d vel = -40.0 * GetOrient().VectorZ();
	missile->SetPosition(GetPosition() + pos);
	missile->SetVelocity(GetVelocity() + vel);
	Pi::game->GetSpace()->AddBody(missile);
	return missile;
}

void Ship::SetFlightState(Ship::FlightState newState)
{
	if (m_flightState == newState) return;
	if (IsHyperspaceActive() && (newState != FLYING))
		AbortHyperjump();

	if (newState == FLYING) {
		m_testLanded = false;
		if (m_flightState == DOCKING || m_flightState == DOCKED)
			onUndock.emit();

		m_dockedWith = nullptr;

		// lock thrusters on for amount of time needed to push us out of station
		static const double MASS_LOCK_REFERENCE(40000.0); // based purely on experimentation
		// limit the time to between 2.0 and 20.0 seconds of thrust, the player can override
		m_launchLockTimeout = std::min(std::max(2.0, 2.0 * (GetMass() / MASS_LOCK_REFERENCE)), 20.0);
	}

	if (newState == DOCKED) {
		m_launchLockTimeout = 0.0;
		ClearLinThrusterState();
		ClearAngThrusterState();
	}

	m_flightState = newState;
	Properties().Set("flightState", EnumStrings::GetString("ShipFlightState", m_flightState));

	switch (m_flightState) {
	case FLYING:
		SetMoving(true);
		SetColliding(true);
		SetStatic(false);
		break;
	case DOCKING:
		SetMoving(false);
		SetColliding(false);
		SetStatic(false);
		break;
	case UNDOCKING:
		SetMoving(false);
		SetColliding(false);
		SetStatic(false);
		break;
		// TODO: set collision index? dynamic stations... use landed for open-air?
	case DOCKED:
		SetMoving(false);
		SetColliding(false);
		SetStatic(false);
		break;
	case LANDED:
		SetMoving(false);
		SetColliding(true);
		SetStatic(true);
		break;
	case JUMPING:
		SetMoving(true);
		SetColliding(false);
		SetStatic(false);
		break;
	case HYPERSPACE:
		SetMoving(false);
		SetColliding(false);
		SetStatic(false);
		break;
	}
}

void Ship::Blastoff()
{
	if (m_flightState != LANDED) return;

	vector3d up = GetPosition().Normalized();

	Frame *f = Frame::GetFrame(GetFrame());

	assert(f->GetBody()->IsType(Object::PLANET));

	const double planetRadius = 2.0 + static_cast<Planet *>(f->GetBody())->GetTerrainHeight(up);
	SetVelocity(vector3d(0, 0, 0));
	SetAngVelocity(vector3d(0, 0, 0));
	SetFlightState(FLYING);

	SetPosition(up * planetRadius - GetAabb().min.y * up);
	SetThrusterState(1, 1.0); // thrust upwards

	LuaEvent::Queue("onShipTakeOff", this, f->GetBody());
}

void Ship::TestLanded()
{
	m_testLanded = false;
	if (m_launchLockTimeout > 0.0f) return;
	if (m_wheelState < 1.0f) return;

	Frame *f = Frame::GetFrame(GetFrame());

	if (f->GetBody()->IsType(Object::PLANET)) {
		double speed = GetVelocity().Length();
		vector3d up = GetPosition().Normalized();
		const double planetRadius = static_cast<Planet *>(f->GetBody())->GetTerrainHeight(up);

		if (speed < MAX_LANDING_SPEED) {
			// check player is sortof sensibly oriented for landing
			if (GetOrient().VectorY().Dot(up) > 0.99) {
				// position at zero altitude
				SetPosition(up * (planetRadius - GetAabb().min.y));

				// position facing in roughly the same direction
				vector3d right = up.Cross(GetOrient().VectorZ()).Normalized();
				SetOrient(matrix3x3d::FromVectors(right, up));

				SetVelocity(vector3d(0, 0, 0));
				SetAngVelocity(vector3d(0, 0, 0));
				ClearThrusterState();
				SetFlightState(LANDED);
				Sound::BodyMakeNoise(this, "Rough_Landing", 1.0f);
				LuaEvent::Queue("onShipLanded", this, f->GetBody());
				onLanded.emit();
			}
		}
	}
}

void Ship::SetLandedOn(Planet *p, float latitude, float longitude)
{
	m_wheelTransition = 0;
	m_wheelState = 1.0f;
	Frame *f_non_rot = Frame::GetFrame(p->GetFrame());
	Frame *f = Frame::GetFrame(f_non_rot->GetRotFrame());
	SetFrame(f_non_rot->GetRotFrame());

	vector3d up = vector3d(cos(latitude) * sin(longitude), sin(latitude), cos(latitude) * cos(longitude));
	const double planetRadius = p->GetTerrainHeight(up);
	SetPosition(up * (planetRadius - GetAabb().min.y));
	vector3d right = up.Cross(vector3d(0, 0, 1)).Normalized();
	SetOrient(matrix3x3d::FromVectors(right, up));
	SetVelocity(vector3d(0, 0, 0));
	SetAngVelocity(vector3d(0, 0, 0));
	ClearThrusterState();
	SetFlightState(LANDED);
	LuaEvent::Queue("onShipLanded", this, p);
	onLanded.emit();
}

void Ship::SetFrame(FrameId fId)
{
	DynamicBody::SetFrame(fId);
	m_sensors->ResetTrails();
}

void Ship::TimeStepUpdate(const float timeStep)
{
	// If docked, station is responsible for updating position/orient of ship
	// but we call this crap anyway and hope it doesn't do anything bad

	const vector3d thrust = GetPropulsion()->GetActualLinThrust();
	AddRelForce(thrust);
	AddRelTorque(GetPropulsion()->GetActualAngThrust());

	//apply extra atmospheric flight forces
	AddTorque(CalcAtmoTorque());

	if (m_landingGearAnimation)
		m_landingGearAnimation->SetProgress(m_wheelState);
	m_dragCoeff = DynamicBody::DEFAULT_DRAG_COEFF * (1.0 + 0.25 * m_wheelState);
	DynamicBody::TimeStepUpdate(timeStep);

	// fuel use decreases mass, so do this as the last thing in the frame
	UpdateFuel(timeStep);

	m_navLights->SetEnabled(m_wheelState > 0.01f);
	m_navLights->Update(timeStep);
	if (m_sensors.get()) m_sensors->Update(timeStep);
}

void Ship::DoThrusterSounds() const
{
	// XXX any ship being the current camera body should emit sounds
	// also, ship sounds could be split to internal and external sounds

	// XXX sound logic could be part of a bigger class (ship internal sounds)
	/* Ship engine noise. less loud inside */
	float v_env = (Pi::game->GetWorldView()->shipView.GetCameraController()->IsExternal() ? 1.0f : 0.5f) * Sound::GetSfxVolume();
	static Sound::Event sndev;
	float volBoth = 0.0f;
	volBoth += 0.5f * fabs(GetPropulsion()->GetLinThrusterState().y);
	volBoth += 0.5f * fabs(GetPropulsion()->GetLinThrusterState().z);

	float targetVol[2] = { volBoth, volBoth };
	if (GetPropulsion()->GetLinThrusterState().x > 0.0)
		targetVol[0] += 0.5f * float(GetPropulsion()->GetLinThrusterState().x);
	else
		targetVol[1] += -0.5f * float(GetPropulsion()->GetLinThrusterState().x);

	targetVol[0] = v_env * Clamp(targetVol[0], 0.0f, 1.0f);
	targetVol[1] = v_env * Clamp(targetVol[1], 0.0f, 1.0f);
	float dv_dt[2] = { 4.0f, 4.0f };
	if (!sndev.VolumeAnimate(targetVol, dv_dt)) {
		sndev.Play("Thruster_large", 0.0f, 0.0f, Sound::OP_REPEAT);
		sndev.VolumeAnimate(targetVol, dv_dt);
	}
	float angthrust = 0.1f * v_env * float(GetPropulsion()->GetAngThrusterState().Length());

	static Sound::Event angThrustSnd;
	if (!angThrustSnd.VolumeAnimate(angthrust, angthrust, 5.0f, 5.0f)) {
		angThrustSnd.Play("Thruster_Small", 0.0f, 0.0f, Sound::OP_REPEAT);
		angThrustSnd.VolumeAnimate(angthrust, angthrust, 5.0f, 5.0f);
	}
}

// for timestep changes, to stop autopilot overshoot
// either adds half of current accel if decelerating
void Ship::TimeAccelAdjust(const float timeStep)
{
	if (!AIIsActive()) return;
#ifdef DEBUG_AUTOPILOT
	if (this->IsType(Object::PLAYER))
		Output("Time accel adjustment, step = %.1f, decel = %s\n", double(timeStep),
			m_decelerating ? "true" : "false");
#endif
	vector3d vdiff = double(timeStep) * GetLastForce() * (1.0 / GetMass());
	if (!m_decelerating) vdiff = -2.0 * vdiff;
	SetVelocity(GetVelocity() + vdiff);
}

double Ship::GetHullTemperature() const
{
	// TODO: fix this to calculate appropriate skin friction and heating.
	//const double dragCoeff = DynamicBody::DEFAULT_DRAG_COEFF * 1.25;
	//const double dragGs = CalcAtmosphericDrag(GetVelocity().LengthSqr(), GetClipRadius(), dragCoeff) / (GetMass() * 9.81);
	//return dragGs / 25.0;
	// TODO: fix this to properly account for heating due to air friction instead of G-force.
	double dragGs = GetAtmosForce().Length() / (GetMass() * 9.81);
	int atmo_shield_cap = 0;
	const_cast<Ship *>(this)->Properties().Get(R"(atmo_shield_cap)", atmo_shield_cap);
	return dragGs / (15.0 * (1.0 + atmo_shield_cap + (2.0 * (1.0 - m_wheelState))));
}

void Ship::SetAlertState(AlertState as)
{
	m_alertState = as;
	Properties().Set("alertStatus", EnumStrings::GetString("ShipAlertStatus", as));
}

void Ship::UpdateAlertState()
{
	// no alerts if no radar
	int radar_cap = 0;
	Properties().Get("radar_cap", radar_cap);
	if (radar_cap <= 0) {
		// clear existing alert state if there was one
		if (GetAlertState() != ALERT_NONE) {
			SetAlertState(ALERT_NONE);
			LuaEvent::Queue("onShipAlertChanged", this, EnumStrings::GetString("ShipAlertStatus", ALERT_NONE));
		}
		return;
	}

	bool ship_is_near = false, ship_is_firing = false;

	// sanity check: m_lastAlertUpdate should not be in the future.
	// reset and re-check if it is.
	if (m_lastAlertUpdate > Pi::game->GetTime()) {
		m_lastAlertUpdate = 0;
		m_shipNear = false;
		m_shipFiring = false;
	}

	if (m_lastAlertUpdate + 1.0 <= Pi::game->GetTime()) {
		// time to update the list again, once per second should suffice
		m_lastAlertUpdate = Pi::game->GetTime();

		static const double ALERT_DISTANCE = 100000.0; // 100km
		Space::BodyNearList nearbyBodies = Pi::game->GetSpace()->GetBodiesMaybeNear(this, ALERT_DISTANCE);

		// handle the results
		for (auto i : nearbyBodies) {
			if ((i) == this) continue;
			if (!(i)->IsType(Object::SHIP) || (i)->IsType(Object::MISSILE)) continue;

			// TODO: Here there were a const on Ship*, now it cannot remain because of ship->firing and so, this open a breach...
			// A solution is to put a member on ship: true if is firing, false if is not
			Ship *ship = static_cast<Ship *>(i);

			if (ship->GetShipType()->tag == ShipType::TAG_STATIC_SHIP) continue;
			if (ship->GetFlightState() == LANDED || ship->GetFlightState() == DOCKED) continue;

			if (GetPositionRelTo(ship).LengthSqr() < ALERT_DISTANCE * ALERT_DISTANCE) {
				ship_is_near = true;

				Uint32 gunstate = ship->GetFixedGuns()->IsFiring();
				if (gunstate) {
					ship_is_firing = true;
					break;
				}
			}
		}

		// store
		m_shipNear = ship_is_near;
		m_shipFiring = ship_is_firing;
	} else {
		ship_is_near = m_shipNear;
		ship_is_firing = m_shipFiring;
	}

	bool changed = false;
	switch (m_alertState) {
	case ALERT_NONE:
		if (ship_is_near) {
			SetAlertState(ALERT_SHIP_NEARBY);
			changed = true;
		}
		if (ship_is_firing) {
			m_lastFiringAlert = Pi::game->GetTime();
			SetAlertState(ALERT_SHIP_FIRING);
			changed = true;
		}
		break;

	case ALERT_SHIP_NEARBY:
		if (!ship_is_near) {
			SetAlertState(ALERT_NONE);
			changed = true;
		} else if (ship_is_firing) {
			m_lastFiringAlert = Pi::game->GetTime();
			SetAlertState(ALERT_SHIP_FIRING);
			changed = true;
		}
		break;

	case ALERT_SHIP_FIRING:
		if (!ship_is_near) {
			SetAlertState(ALERT_NONE);
			changed = true;
		} else if (ship_is_firing) {
			m_lastFiringAlert = Pi::game->GetTime();
		} else if (m_lastFiringAlert + 60.0 <= Pi::game->GetTime()) {
			SetAlertState(ALERT_SHIP_NEARBY);
			changed = true;
		}
		break;
	}

	if (changed)
		LuaEvent::Queue("onShipAlertChanged", this, EnumStrings::GetString("ShipAlertStatus", GetAlertState()));
}

void Ship::UpdateFuel(const float timeStep)
{
	GetPropulsion()->UpdateFuel(timeStep);
	UpdateFuelStats();
	Properties().Set("fuel", GetFuel() * 100); // XXX to match SetFuelPercent

	if (GetPropulsion()->IsFuelStateChanged())
		LuaEvent::Queue("onShipFuelChanged", this, EnumStrings::GetString("PropulsionFuelStatus", GetPropulsion()->GetFuelState()));
}

void Ship::StaticUpdate(const float timeStep)
{
	// do player sounds before dead check, so they also turn off
	if (IsType(Object::PLAYER)) DoThrusterSounds();

	if (IsDead()) return;

	if (m_controller) m_controller->StaticUpdate(timeStep);

	const double hullTemp = GetHullTemperature();
	if (hullTemp > 1.0) {
		DoDamage(hullTemp);
	}

	if (m_flightState == FLYING) {
		Frame *frame = Frame::GetFrame(GetFrame());
		Body *astro = frame->GetBody();
		if (astro && astro->IsType(Object::PLANET)) {
			Planet *p = static_cast<Planet *>(astro);
			double dist = GetPosition().Length();
			double pressure, density;
			p->GetAtmosphericState(dist, &pressure, &density);

			int atmo_shield_cap = 0;
			const_cast<Ship *>(this)->Properties().Get("atmo_shield_cap", atmo_shield_cap);
			atmo_shield_cap = std::max(atmo_shield_cap, 1); // needs to have some shielding by default
			if (pressure > (m_type->atmosphericPressureLimit * atmo_shield_cap)) {
				float damage = float(pressure - m_type->atmosphericPressureLimit);
				DoDamage(damage);
			}
		}
	}

	UpdateAlertState();

	/* FUEL SCOOPING!!!!!!!!! */
	int capacity = 0;
	Properties().Get("fuel_scoop_cap", capacity);
	if (m_flightState == FLYING && capacity > 0) {
		Frame *frame = Frame::GetFrame(GetFrame());
		Body *astro = frame->GetBody();
		if (astro && astro->IsType(Object::PLANET)) {
			Planet *p = static_cast<Planet *>(astro);
			if (p->GetSystemBody()->IsScoopable()) {
				const double dist = GetPosition().Length();
				double pressure, density;
				p->GetAtmosphericState(dist, &pressure, &density);

				const double speed = GetVelocity().Length();
				const vector3d vdir = GetVelocity().Normalized();
				const vector3d pdir = -GetOrient().VectorZ();
				const double dot = vdir.Dot(pdir);
				if ((m_stats.free_capacity) && (dot > 0.90) && (speed > 1000.0) && (density > 0.5)) {
					const double rate = speed * density * 0.00000333 * double(capacity);
					if (Pi::rng.Double() < rate) {
						lua_State *l = Lua::manager->GetLuaState();
						pi_lua_import(l, "Equipment");
						LuaTable hydrogen = LuaTable(l, -1).Sub("cargo").Sub("hydrogen");
						LuaObject<Ship>::CallMethod(this, "AddEquip", hydrogen);
						UpdateEquipStats();
						if (this->IsType(Object::PLAYER)) {
							Pi::game->log->Add(stringf(Lang::FUEL_SCOOP_ACTIVE_N_TONNES_H_COLLECTED,
								formatarg("quantity", LuaObject<Ship>::CallMethod<int>(this, "CountEquip", hydrogen))));
						}
						lua_pop(l, 3);
					}
				}
			}
		}
	}

	// Cargo bay life support
	capacity = 0;
	Properties().Get("cargo_life_support_cap", capacity);
	if (!capacity) {
		// Hull is pressure-sealed, it just doesn't provide
		// temperature regulation and breathable atmosphere

		// kill stuff roughly every 5 seconds
		if ((!m_dockedWith) && (5.0 * Pi::rng.Double() < timeStep)) {
			std::string t(Pi::rng.Int32(2) ? "live_animals" : "slaves");

			lua_State *l = Lua::manager->GetLuaState();
			pi_lua_import(l, "Equipment");
			LuaTable cargo = LuaTable(l, -1).Sub("cargo");
			if (LuaObject<Ship>::CallMethod<int>(this, "RemoveEquip", cargo.Sub(t))) {
				LuaObject<Ship>::CallMethod<int>(this, "AddEquip", cargo.Sub("fertilizer"));
				if (this->IsType(Object::PLAYER)) {
					Pi::game->log->Add(Lang::CARGO_BAY_LIFE_SUPPORT_LOST);
				}
				lua_pop(l, 4);
			} else
				lua_pop(l, 3);
		}
	}

	if (m_flightState == FLYING)
		m_launchLockTimeout -= timeStep;
	if (m_launchLockTimeout < 0) m_launchLockTimeout = 0;
	if (m_flightState == JUMPING || m_flightState == HYPERSPACE)
		m_launchLockTimeout = 0;

	// lasers
	FixedGuns *fg = GetFixedGuns();
	fg->UpdateGuns(timeStep);
	for (int i = 0; i < 2; i++) {
		if (fg->Fire(i, this)) {
			if (fg->IsBeam(i)) {
				float vl, vr;
				Sound::CalculateStereo(this, 1.0f, &vl, &vr);
				m_beamLaser[i].Play("Beam_laser", vl, vr, Sound::OP_REPEAT);
			} else {
				Sound::BodyMakeNoise(this, "Pulse_Laser", 1.0f);
			}
			LuaEvent::Queue("onShipFiring", this);
		}

		if (fg->IsBeam(i)) {
			if (fg->IsFiring(i)) {
				float vl, vr;
				Sound::CalculateStereo(this, 1.0f, &vl, &vr);
				if (!m_beamLaser[i].IsPlaying()) {
					m_beamLaser[i].Play("Beam_laser", vl, vr, Sound::OP_REPEAT);
				} else {
					// update volume
					m_beamLaser[i].SetVolume(vl, vr);
				}
			} else if (!fg->IsFiring(i) && m_beamLaser[i].IsPlaying()) {
				m_beamLaser[i].Stop();
			}
		}
	}

	if (m_ecmRecharge > 0.0f) {
		m_ecmRecharge = std::max(0.0f, m_ecmRecharge - timeStep);
	}

	if (m_shieldCooldown > 0.0f) {
		m_shieldCooldown = std::max(0.0f, m_shieldCooldown - timeStep);
	}

	if (m_stats.shield_mass_left < m_stats.shield_mass) {
		// 250 second recharge
		float recharge_rate = 0.004f;
		float booster = 1.0f;
		Properties().Get("shield_energy_booster_cap", booster);
		recharge_rate *= booster;
		m_stats.shield_mass_left = Clamp(m_stats.shield_mass_left + m_stats.shield_mass * recharge_rate * timeStep, 0.0f, m_stats.shield_mass);
		Properties().Set("shieldMassLeft", m_stats.shield_mass_left);
	}

	if (m_wheelTransition) {
		m_wheelState += m_wheelTransition * 0.3f * timeStep;
		m_wheelState = Clamp(m_wheelState, 0.0f, 1.0f);
		if (is_equal_exact(m_wheelState, 0.0f) || is_equal_exact(m_wheelState, 1.0f))
			m_wheelTransition = 0;
	}

	if (m_testLanded) TestLanded();

	capacity = 0;
	Properties().Get("hull_autorepair_cap", capacity);
	if (capacity) {
		m_stats.hull_mass_left = std::min(m_stats.hull_mass_left + 0.1f * timeStep, float(m_type->hullMass));
		Properties().Set("hullMassLeft", m_stats.hull_mass_left);
		Properties().Set("hullPercent", 100.0f * (m_stats.hull_mass_left / float(m_type->hullMass)));
	}

	// After calling StartHyperspaceTo this Ship must not spawn objects
	// holding references to it (eg missiles), as StartHyperspaceTo
	// removes the ship from Space::bodies and so the missile will not
	// have references to this cleared by NotifyRemoved()
	if (m_hyperspace.now) {
		m_hyperspace.now = false;
		EnterHyperspace();
	}

	if (m_hyperspace.countdown > 0.0f) {
		// Check the Lua function
		bool abort = false;
		lua_State *l = m_hyperspace.checks.GetLua();
		if (l) {
			m_hyperspace.checks.PushCopyToStack();
			if (lua_isfunction(l, -1)) {
				lua_call(l, 0, 1);
				abort = !lua_toboolean(l, -1);
				lua_pop(l, 1);
			}
		}
		if (abort) {
			AbortHyperjump();
		} else {
			m_hyperspace.countdown = m_hyperspace.countdown - timeStep;
			if (!abort && m_hyperspace.countdown <= 0.0f && (is_equal_exact(m_wheelState, 0.0f))) {
				m_hyperspace.countdown = 0;
				m_hyperspace.now = true;
				SetFlightState(JUMPING);

				// We have to fire it here, because the event isn't actually fired until
				// after the whole physics update, which means the flight state on next
				// step would be HYPERSPACE, thus breaking quite a few things.
				LuaEvent::Queue("onLeaveSystem", this);
			} else if (!(is_equal_exact(m_wheelState, 0.0f)) && this->IsType(Object::PLAYER)) {
				AbortHyperjump();
				Sound::BodyMakeNoise(this, "Missile_Inbound", 1.0f);
			}
		}
	}
}

void Ship::NotifyRemoved(const Body *const removedBody)
{
	if (m_curAICmd) m_curAICmd->OnDeleted(removedBody);
}

bool Ship::Undock()
{
	return (m_dockedWith && m_dockedWith->LaunchShip(this, m_dockedWithPort));
}

void Ship::SetDockedWith(SpaceStation *s, int port)
{
	if (s) {
		m_dockedWith = s;
		m_dockedWithPort = port;
		m_wheelTransition = 0;
		m_wheelState = 1.0f;
		// hand position/state responsibility over to station
		m_dockedWith->SetDocked(this, port);
		onDock.emit();
	} else {
		Undock();
	}
}

void Ship::SetGunState(int idx, int state)
{
	if (m_flightState != FLYING)
		return;

	GetFixedGuns()->SetGunFiringState(idx, state);
}

bool Ship::SetWheelState(bool down)
{
	if (m_flightState != FLYING) return false;
	if (is_equal_exact(m_wheelState, down ? 1.0f : 0.0f)) return false;
	int newWheelTransition = (down ? 1 : -1);
	if (newWheelTransition == m_wheelTransition) return false;
	m_wheelTransition = newWheelTransition;
	return true;
}

void Ship::Render(Graphics::Renderer *renderer, const Camera *camera, const vector3d &viewCoords, const matrix4x4d &viewTransform)
{
	if (IsDead()) return;

	GetPropulsion()->Render(renderer, camera, viewCoords, viewTransform);

	matrix3x3f mt;
	matrix3x3dtof(viewTransform.Inverse().GetOrient(), mt);
	s_heatGradientParams.heatingMatrix = mt;
	s_heatGradientParams.heatingNormal = vector3f(GetVelocity().Normalized());
	s_heatGradientParams.heatingAmount = Clamp(GetHullTemperature(), 0.0, 1.0);

	// This has to be done per-model with a shield and just before it's rendered
	const bool shieldsVisible = m_shieldCooldown > 0.01f && m_stats.shield_mass_left > (m_stats.shield_mass / 100.0f);
	GetShields()->SetEnabled(shieldsVisible);
	GetShields()->Update(m_shieldCooldown, 0.01f * GetPercentShields());

	//strncpy(params.pText[0], GetLabel().c_str(), sizeof(params.pText));
	RenderModel(renderer, camera, viewCoords, viewTransform);
	m_navLights->Render(renderer);
	renderer->GetStats().AddToStatCount(Graphics::Stats::STAT_SHIPS, 1);

	if (m_ecmRecharge > 0.0f) {
		// ECM effect: a cloud of particles for a sparkly effect
		vector3f v[100];
		for (int i = 0; i < 100; i++) {
			const double r1 = Pi::rng.Double() - 0.5;
			const double r2 = Pi::rng.Double() - 0.5;
			const double r3 = Pi::rng.Double() - 0.5;
			v[i] = vector3f(GetPhysRadius() * vector3d(r1, r2, r3).NormalizedSafe());
		}
		Color c(128, 128, 255, 255);
		float totalRechargeTime = GetECMRechargeTime();
		if (totalRechargeTime >= 0.0f) {
			c.a = (m_ecmRecharge / totalRechargeTime) * 255;
		}

		SfxManager::ecmParticle->diffuse = c;

		matrix4x4f t;
		for (int i = 0; i < 12; i++)
			t[i] = float(viewTransform[i]);
		t[12] = viewCoords.x;
		t[13] = viewCoords.y;
		t[14] = viewCoords.z;
		t[15] = 1.0f;

		renderer->SetTransform(t);
		renderer->DrawPointSprites(100, v, SfxManager::additiveAlphaState, SfxManager::ecmParticle.get(), 50.f);
	}
}

bool Ship::SpawnCargo(CargoBody *c_body) const
{
	if (m_flightState != FLYING) return false;
	vector3d pos = GetOrient() * vector3d(0, GetAabb().min.y - 5, 0);
	c_body->SetFrame(GetFrame());
	c_body->SetPosition(GetPosition() + pos);
	c_body->SetVelocity(GetVelocity() + GetOrient() * vector3d(0, -10, 0));
	Pi::game->GetSpace()->AddBody(c_body);
	return true;
}

void Ship::EnterHyperspace()
{
	assert(GetFlightState() != Ship::HYPERSPACE);

	// Is it still a good idea, with the onLeaveSystem moved elsewhere?
	Ship::HyperjumpStatus status = CheckHyperjumpCapability();
	if (status != HYPERJUMP_OK && status != HYPERJUMP_INITIATED) {
		if (m_flightState == JUMPING)
			SetFlightState(FLYING);
		return;
	}

	SetFlightState(Ship::HYPERSPACE);

	// virtual call, do class-specific things
	OnEnterHyperspace();
}

void Ship::OnEnterHyperspace()
{
	Sound::BodyMakeNoise(this, m_hyperspace.sounds.jump_sound.c_str(), 1.f);
	m_hyperspaceCloud = new HyperspaceCloud(this, Pi::game->GetTime() + m_hyperspace.duration, false);
	m_hyperspaceCloud->SetFrame(GetFrame());
	m_hyperspaceCloud->SetPosition(GetPosition());

	Space *space = Pi::game->GetSpace();

	space->RemoveBody(this);
	space->AddBody(m_hyperspaceCloud);
}

void Ship::EnterSystem()
{
	PROFILE_SCOPED()
	assert(GetFlightState() == Ship::HYPERSPACE);

	// virtual call, do class-specific things
	OnEnterSystem();

	SetFlightState(Ship::FLYING);

	LuaEvent::Queue("onEnterSystem", this);
}

void Ship::OnEnterSystem()
{
	m_hyperspaceCloud = 0;
}

void Ship::SetShipId(const ShipType::Id &shipId)
{
	m_type = &ShipType::types[shipId];

	Properties().Set("shipId", shipId);
}

void Ship::SetShipType(const ShipType::Id &shipId)
{
	// clear all equipment so that any relevant capability properties (or other data) is wiped
	ScopedTable(m_equipSet).CallMethod("Clear", this);

	SetShipId(shipId);
	SetModel(m_type->modelName.c_str());
	m_skin.SetDecal(m_type->manufacturer);
	m_skin.Apply(GetModel());
	Init();
	onFlavourChanged.emit();
	if (IsType(Object::PLAYER))
		Pi::game->GetWorldView()->shipView.GetCameraController()->Reset();
	InitEquipSet();

	LuaEvent::Queue("onShipTypeChanged", this);
}

void Ship::SetLabel(const std::string &label)
{
	DynamicBody::SetLabel(label);
	m_skin.SetLabel(label);
	m_skin.Apply(GetModel());
}

void Ship::SetShipName(const std::string &shipName)
{
	m_shipName = shipName;
	Properties().Set("shipName", shipName);
}

void Ship::SetSkin(const SceneGraph::ModelSkin &skin)
{
	m_skin = skin;
	m_skin.Apply(GetModel());
}

void Ship::SetPattern(const unsigned int num)
{
	GetModel()->SetPattern(num);
}

Uint8 Ship::GetRelations(Body *other) const
{
	auto it = m_relationsMap.find(other);
	if (it != m_relationsMap.end())
		return it->second;

	return 50;
}

void Ship::SetRelations(Body *other, Uint8 percent)
{
	m_relationsMap[other] = percent;
	if (m_sensors.get()) m_sensors->UpdateIFF(other);
}
