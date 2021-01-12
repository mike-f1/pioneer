// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "GameConfSingleton.h"

#include "FileSystem.h"
#include "libs/utils.h"

std::unique_ptr<GameConfig> GameConfSingleton::m_gConfig = nullptr;
DetailLevel GameConfSingleton::m_detail;

bool GameConfSingleton::navTunnelDisplayed = false;
bool GameConfSingleton::speedLinesDisplayed = false;
bool GameConfSingleton::hudTrailsDisplayed = false;
bool GameConfSingleton::bRefreshBackgroundStars = true;
float GameConfSingleton::amountOfBackgroundStarsDisplayed = 1.0f;

constexpr char GameConfSingleton::SAVE_DIR_NAME[] = "savefiles";

void GameConfSingleton::Init(const GameConfig::map_string &override_)
{
	if (m_gConfig != nullptr) {
		Output("Warning: GameConfig already initialised!!!\n");
	}

	m_gConfig.reset(new GameConfig(override_));
	m_detail.planets = m_gConfig->Int("DetailPlanets");
	m_detail.cities = m_gConfig->Int("DetailCities");

	amountOfBackgroundStarsDisplayed = m_gConfig->Float("AmountOfBackgroundStars");
	navTunnelDisplayed = m_gConfig->Int("DisplayNavTunnel");
	speedLinesDisplayed = m_gConfig->Int("SpeedLines");
	hudTrailsDisplayed = m_gConfig->Int("HudTrails");
}

void GameConfSingleton::SetAmountBackgroundStars(const float amount)
{
	m_gConfig->SetFloat("AmountOfBackgroundStars", amount);
	m_gConfig->Save();

	amountOfBackgroundStarsDisplayed = Clamp(amount, 0.01f, 1.0f);
	bRefreshBackgroundStars = true;
}

void GameConfSingleton::SetNavTunnelDisplayed(bool state)
{
	m_gConfig->SetInt("DisplayNavTunnel", (state ? 1 : 0));
	m_gConfig->Save();

	navTunnelDisplayed = state;
}

void GameConfSingleton::SetSpeedLinesDisplayed(bool state)
{
	m_gConfig->SetInt("SpeedLines", (state ? 1 : 0));
	m_gConfig->Save();

	speedLinesDisplayed = state;
}

void GameConfSingleton::SetHudTrailsDisplayed(bool state)
{
	m_gConfig->SetInt("HudTrails", (state ? 1 : 0));
	m_gConfig->Save();

	hudTrailsDisplayed = state;
}

std::string GameConfSingleton::GetSaveDir()
{
	return SAVE_DIR_NAME;
}

std::string GameConfSingleton::GetSaveDirFull()
{
	return FileSystem::JoinPath(FileSystem::GetUserDir(), GameConfSingleton::SAVE_DIR_NAME);
}

