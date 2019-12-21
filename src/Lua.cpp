// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Lua.h"

#include "LuaManager.h"

namespace Lua {

	LuaManager *manager = nullptr;

	void Init()
	{
		manager = new LuaManager();
	}

	void Uninit()
	{
		delete manager;
		manager = nullptr;
	}

} // namespace Lua
