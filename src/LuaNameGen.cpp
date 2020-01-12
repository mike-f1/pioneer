// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt
#include "buildopts.h"

#include "LuaNameGen.h"

#include "LuaManager.h"
#include "LuaObject.h"
#include "Random.h"
#include "galaxy/SystemBody.h"

static const std::string DEFAULT_FULL_NAME_MALE("Tom Morton");
static const std::string DEFAULT_FULL_NAME_FEMALE("Thomasina Mortonella");
static const std::string DEFAULT_SURNAME("Jameson");
static const std::string DEFAULT_BODY_NAME("Planet Rock");

LuaNameGen::LuaNameGen()
{
#ifndef NDEBUG
	if (Lua::manager == nullptr) {
		Error("Lua manager is null during LuaNameGen ctor");
		abort();
	}
#endif // NDEBUG
}

static bool getNameGenFunc(lua_State *l, const char *func)
{
	LUA_DEBUG_START(l);

	if (!pi_lua_import(l, "NameGen"))
		return false;

	lua_getfield(l, -1, func);
	if (lua_isnil(l, -1)) {
		lua_pop(l, 2);
		LUA_DEBUG_END(l, 0);
		return false;
	}

	lua_remove(l, -2);

	LUA_DEBUG_END(l, 1);
	return true;
}

std::string LuaNameGen::FullName(bool isFemale, RefCountedPtr<Random> &rng)
{
	lua_State *l = Lua::manager->GetLuaState();

	if (!getNameGenFunc(l, "FullName"))
		return isFemale ? DEFAULT_FULL_NAME_FEMALE : DEFAULT_FULL_NAME_MALE;

	lua_pushboolean(l, isFemale);
	LuaObject<Random>::PushToLua(rng.Get());
	pi_lua_protected_call(l, 2, 1);

	std::string fullname = luaL_checkstring(l, -1);
	lua_pop(l, 1);

	return fullname;
}

std::string LuaNameGen::Surname(RefCountedPtr<Random> &rng)
{
	lua_State *l = Lua::manager->GetLuaState();

	if (!getNameGenFunc(l, "Surname"))
		return DEFAULT_SURNAME;

	LuaObject<Random>::PushToLua(rng.Get());
	pi_lua_protected_call(l, 1, 1);

	std::string surname = luaL_checkstring(l, -1);
	lua_pop(l, 1);

	return surname;
}

std::string LuaNameGen::BodyName(SystemBody *body, RefCountedPtr<Random> &rng)
{
	lua_State *l = Lua::manager->GetLuaState();

	if (!getNameGenFunc(l, "BodyName"))
		return DEFAULT_BODY_NAME;

	LuaObject<SystemBody>::PushToLua(body);
	LuaObject<Random>::PushToLua(rng.Get());
	pi_lua_protected_call(l, 2, 1);

	std::string bodyname = luaL_checkstring(l, -1);
	lua_pop(l, 1);

	return bodyname;
}
