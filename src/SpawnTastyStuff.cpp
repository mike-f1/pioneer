// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "SpawnTastyStuff.h"

#include "Body.h"
#include "CargoBody.h"
#include "Frame.h"
#include "Game.h"
#include "GameLocator.h"
#include "Lua.h"
#include "LuaObject.h"
#include "Random.h"
#include "RandomSingleton.h"
#include "Space.h"
#include "galaxy/Galaxy.h"
#include "galaxy/StarSystem.h"
#include "galaxy/SystemBody.h"

void MiningLaserSpawnTastyStuff(FrameId fId, Body *shooter, const SystemBody *asteroid, const vector3d &pos)
{
	lua_State *l = Lua::manager->GetLuaState();

	// lua cant push "const SystemBody", needs to convert to non-const
	RefCountedPtr<StarSystem> s = GameLocator::getGame()->GetGalaxy()->GetStarSystem(asteroid->GetPath());
	SystemBody *liveasteroid = s->GetBodyByPath(asteroid->GetPath());

	// this is an adapted version of "CallMethod", because;
	// 1, there is no template for LuaObject<LuaTable>::CallMethod(..., SystemBody)
	// 2, this leaves the return value on the lua stack to be used by "new CargoBody()"
	LUA_DEBUG_START(l);
	LuaObject<Body>::PushToLua(shooter);
	lua_pushstring(l, "SpawnMiningContainer");
	lua_gettable(l, -2);
	lua_pushvalue(l, -2);
	lua_remove(l, -3);
	LuaObject<SystemBody>::PushToLua(liveasteroid);
	pi_lua_protected_call(l, 2, 1);

	CargoBody *cargo = new CargoBody(LuaRef(l, -1));
	lua_pop(l, 1);
	LUA_DEBUG_END(l, 0);

	cargo->SetFrame(fId);
	cargo->SetPosition(pos);
	const double x = RandomSingleton::getInstance().Double();
	vector3d dir = pos.Normalized();
	dir.ArbRotate(vector3d(x, 1 - x, 0), RandomSingleton::getInstance().Double() - .5);
	cargo->SetVelocity(RandomSingleton::getInstance().Double(100.0, 200.0) * dir);
	GameLocator::getGame()->GetSpace()->AddBody(cargo);
}
