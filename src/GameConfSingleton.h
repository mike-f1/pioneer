// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef GAMECONFSINGLETON_H
#define GAMECONFSINGLETON_H

#include "GameConfig.h"

#include <memory>

class GameConfSingleton
{
public:
	GameConfSingleton() = delete;

	static void Init(const GameConfig::map_string &override_ = GameConfig::map_string());

	static GameConfig &getInstance() { return *m_gConfig.get(); };

private:
	static std::unique_ptr<GameConfig> m_gConfig;
};

#endif // GAMECONFSINGLETON_H
