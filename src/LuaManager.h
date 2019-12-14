// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _LUAMANAGER_H
#define _LUAMANAGER_H

#include <lua.hpp>

class LuaManager {
public:
	LuaManager();
	~LuaManager();

	LuaManager(const LuaManager &) = delete;
	LuaManager &operator=(const LuaManager &) = delete;

	lua_State *GetLuaState() { return m_lua; }
	size_t GetMemoryUsage() const;
	void CollectGarbage();

private:
	lua_State *m_lua;
};

#endif
