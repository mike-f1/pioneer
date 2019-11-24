// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "GameConfSingleton.h"

std::unique_ptr<GameConfig> GameConfSingleton::m_gConfig = nullptr;

void GameConfSingleton::Init(const GameConfig::map_string &override_)
{
	m_gConfig.reset(new GameConfig(override_));
}
