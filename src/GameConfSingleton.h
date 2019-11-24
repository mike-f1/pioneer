// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef GAMECONFSINGLETON_H
#define GAMECONFSINGLETON_H

#include "GameConfig.h"

#include <memory>

class DetailLevel {
public:
	DetailLevel() :
		planets(0),
		cities(0) {}
	int planets;
	int cities;
};

class GameConfSingleton
{
public:
	GameConfSingleton() = delete;

	static void Init(const GameConfig::map_string &override_ = GameConfig::map_string());

	static GameConfig &getInstance() { return *m_gConfig.get(); };

	static DetailLevel &getDetail() { return m_detail; };

	static void SetAmountBackgroundStars(const float pc);

	static float GetAmountBackgroundStars() { return amountOfBackgroundStarsDisplayed; }
	static bool MustRefreshBackgroundClearFlag()
	{
		const bool bRet = bRefreshBackgroundStars;
		bRefreshBackgroundStars = false;
		return bRet;
	}

	static bool IsNavTunnelDisplayed() { return navTunnelDisplayed; }
	static void SetNavTunnelDisplayed(bool state);
	static bool AreSpeedLinesDisplayed() { return speedLinesDisplayed; }
	static void SetSpeedLinesDisplayed(bool state);
	static bool AreHudTrailsDisplayed() { return hudTrailsDisplayed; }
	static void SetHudTrailsDisplayed(bool state);

private:
	static std::unique_ptr<GameConfig> m_gConfig;
	static DetailLevel m_detail;

	static bool navTunnelDisplayed;
	static bool speedLinesDisplayed;
	static bool hudTrailsDisplayed;
	static bool bRefreshBackgroundStars;
	static float amountOfBackgroundStarsDisplayed;
};

#endif // GAMECONFSINGLETON_H
