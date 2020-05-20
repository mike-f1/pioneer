// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "StarSystemWriter.h"

#include "ExplorationState.h"
#include "Galaxy.h"
#include "Lang.h"
#include "LuaEvent.h"
#include "Sector.h"
#include "libs/utils.h"
#include "libs/StringF.h"
#include "libs/stringUtils.h"

SystemBody *StarSystemWriter::NewBody() const
{
	SystemBody *body = new SystemBody(SystemPath(m_ssys->m_path.sectorX, m_ssys->m_path.sectorY, m_ssys->m_path.sectorZ, m_ssys->m_path.systemIndex, static_cast<uint32_t>(m_ssys->m_bodies.size())), m_ssys);
	m_ssys->m_bodies.push_back(RefCountedPtr<SystemBody>(body));
	return body;
}

void StarSystemWriter::ExploreSystem(double time)
{
	if (m_ssys->m_explored != ExplorationState::eUNEXPLORED)
		return;
	m_ssys->m_explored = ExplorationState::eEXPLORED_BY_PLAYER;
	m_ssys->m_exploredTime = time;
	RefCountedPtr<Sector> sec = m_ssys->m_galaxy->GetMutableSector(m_ssys->m_path);
	Sector::System &secsys = sec->m_systems[m_ssys->m_path.systemIndex];
	secsys.SetExplored(m_ssys->m_explored, m_ssys->m_exploredTime);
	MakeShortDescription();
	LuaEvent::Queue("onSystemExplored", m_ssys);
}

void StarSystemWriter::MakeShortDescription()
{
	PROFILE_SCOPED()
	if (m_ssys->GetExplored() == ExplorationState::eUNEXPLORED)
		SetShortDesc(Lang::UNEXPLORED_SYSTEM_NO_DATA);

	else if (m_ssys->GetExplored() == ExplorationState::eEXPLORED_BY_PLAYER)
		SetShortDesc(stringf(Lang::RECENTLY_EXPLORED_SYSTEM, formatarg("date", stringUtils::format_date_only(m_ssys->GetExploredTime()))));

	/* Total population is in billions */
	else if (m_ssys->GetTotalPop() == 0) {
		SetShortDesc(Lang::SMALL_SCALE_PROSPECTING_NO_SETTLEMENTS);
	} else if (m_ssys->GetTotalPop() < fixed(1, 10)) {
		switch (m_ssys->GetEconType()) {
		case GalacticEconomy::EconType::INDUSTRY: SetShortDesc(Lang::SMALL_INDUSTRIAL_OUTPOST); break;
		case GalacticEconomy::EconType::MINING: SetShortDesc(Lang::SOME_ESTABLISHED_MINING); break;
		case GalacticEconomy::EconType::AGRICULTURE: SetShortDesc(Lang::YOUNG_FARMING_COLONY); break;
		}
	} else if (m_ssys->GetTotalPop() < fixed(1, 2)) {
		switch (m_ssys->GetEconType()) {
		case GalacticEconomy::EconType::INDUSTRY: SetShortDesc(Lang::INDUSTRIAL_COLONY); break;
		case GalacticEconomy::EconType::MINING: SetShortDesc(Lang::MINING_COLONY); break;
		case GalacticEconomy::EconType::AGRICULTURE: SetShortDesc(Lang::OUTDOOR_AGRICULTURAL_WORLD); break;
		}
	} else if (m_ssys->GetTotalPop() < fixed(5, 1)) {
		switch (m_ssys->GetEconType()) {
		case GalacticEconomy::EconType::INDUSTRY: SetShortDesc(Lang::HEAVY_INDUSTRY); break;
		case GalacticEconomy::EconType::MINING: SetShortDesc(Lang::EXTENSIVE_MINING); break;
		case GalacticEconomy::EconType::AGRICULTURE: SetShortDesc(Lang::THRIVING_OUTDOOR_WORLD); break;
		}
	} else {
		switch (m_ssys->GetEconType()) {
		case GalacticEconomy::EconType::INDUSTRY: SetShortDesc(Lang::INDUSTRIAL_HUB_SYSTEM); break;
		case GalacticEconomy::EconType::MINING: SetShortDesc(Lang::VAST_STRIP_MINE); break;
		case GalacticEconomy::EconType::AGRICULTURE: SetShortDesc(Lang::HIGH_POPULATION_OUTDOOR_WORLD); break;
		}
	}
}
