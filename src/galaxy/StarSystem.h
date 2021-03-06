// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _STARSYSTEM_H
#define _STARSYSTEM_H

#include "Polit.h"
#include "Economy.h"
#include "ExplorationState.h"
#include "GalaxyCache.h"
#include "Polit.h"
#include "SystemBody.h"
#include "SystemPath.h"
#include "libs/fixed.h"
#include "libs/gameconsts.h"
#include "libs/RefCounted.h"

#include <cstdint>
#include <string>
#include <vector>

class Faction;
class Galaxy;

// doubles - all masses in Kg, all lengths in meters
// fixed - any mad scheme

class StarSystem : public RefCounted {
	friend class StarSystemWriter;
	friend void SetCache(RefCountedPtr<StarSystem> ssys, StarSystemCache *cache);
public:
	StarSystem(const SystemPath &path, RefCountedPtr<Galaxy> galaxy, StarSystemCache *cache);
	virtual ~StarSystem();

	void ExportToLua(const char *filename);

	const std::string &GetName() const { return m_name; }
	std::vector<std::string> GetOtherNames() const { return m_other_names; }
	SystemPath GetPathOf(const SystemBody *sbody) const;
	SystemBody *GetBodyByPath(const SystemPath &path) const;
	static void ToJson(Json &jsonObj, StarSystem *);
	static RefCountedPtr<StarSystem> FromJson(RefCountedPtr<Galaxy> galaxy, const Json &jsonObj);
	const SystemPath &GetPath() const { return m_path; }
	const std::string &GetShortDescription() const { return m_shortDesc; }
	const std::string &GetLongDescription() const { return m_longDesc; }
	unsigned GetNumStars() const { return m_numStars; }
	const SysPolit &GetSysPolit() const { return m_polit; }

	RefCountedPtr<const SystemBody> GetRootBody() const { return m_rootBody; }
	RefCountedPtr<SystemBody> GetRootBody() { return m_rootBody; }
	bool HasSpaceStations() const { return !m_spaceStations.empty(); }
	uint32_t GetNumSpaceStations() const { return static_cast<uint32_t>(m_spaceStations.size()); }
	IterationProxy<std::vector<SystemBody *>> GetSpaceStations() { return MakeIterationProxy(m_spaceStations); }
	const IterationProxy<const std::vector<SystemBody *>> GetSpaceStations() const { return MakeIterationProxy(m_spaceStations); }
	IterationProxy<std::vector<SystemBody *>> GetStars() { return MakeIterationProxy(m_stars); }
	const IterationProxy<const std::vector<SystemBody *>> GetStars() const { return MakeIterationProxy(m_stars); }
	uint32_t GetNumBodies() const { return static_cast<uint32_t>(m_bodies.size()); }
	IterationProxy<std::vector<RefCountedPtr<SystemBody>>> GetBodies() { return MakeIterationProxy(m_bodies); }
	const IterationProxy<const std::vector<RefCountedPtr<SystemBody>>> GetBodies() const { return MakeIterationProxy(m_bodies); }

	bool IsCommodityLegal(const GalacticEconomy::Commodity t) const
	{
		return m_commodityLegal[int(t)];
	}

	int GetCommodityBasePriceModPercent(GalacticEconomy::Commodity t) const
	{
		return m_tradeLevel[int(t)];
	}

	const Faction *GetFaction() const { return m_faction; }
	bool GetUnexplored() const { return m_explored == ExplorationState::eUNEXPLORED; }
	ExplorationState GetExplored() const { return m_explored; }
	double GetExploredTime() const { return m_exploredTime; }

	fixed GetMetallicity() const { return m_metallicity; }
	fixed GetIndustrial() const { return m_industrial; }
	fixed GetAgricultural() const { return m_agricultural; }
	GalacticEconomy::EconType GetEconType() const { return m_econType; }
	const int *GetTradeLevel() const { return m_tradeLevel; }
	int GetSeed() const { return m_seed; }
	fixed GetHumanProx() const { return m_humanProx; }
	fixed GetTotalPop() const { return m_totalPop; }

	bool WithinBox(const int Xmin, const int Xmax, const int Ymin, const int Ymax, const int Zmin, const int Zmax) const;

	void Dump(FILE *file, const char *indent = "", bool suppressSectorData = false) const;

	const RefCountedPtr<Galaxy> GetGalaxy() const;

private:
	const RefCountedPtr<Galaxy> m_galaxy;

	std::string ExportBodyToLua(FILE *f, SystemBody *body);
	std::string GetStarTypes(SystemBody *body);

	SystemPath m_path;
	unsigned m_numStars;
	std::string m_name;
	std::vector<std::string> m_other_names;
	std::string m_shortDesc, m_longDesc;
	SysPolit m_polit;

	bool m_isCustom;
	bool m_hasCustomBodies;

	const Faction *m_faction;
	ExplorationState m_explored;
	double m_exploredTime;
	fixed m_metallicity;
	fixed m_industrial;
	GalacticEconomy::EconType m_econType;
	uint32_t m_seed;

	// percent price alteration
	int m_tradeLevel[GalacticEconomy::COMMODITY_COUNT];

	fixed m_agricultural;
	fixed m_humanProx;
	fixed m_totalPop;

	RefCountedPtr<SystemBody> m_rootBody;
	// index into this will be the SystemBody ID used by SystemPath
	std::vector<RefCountedPtr<SystemBody>> m_bodies;
	std::vector<SystemBody *> m_spaceStations;
	std::vector<SystemBody *> m_stars;
	std::vector<bool> m_commodityLegal;

	StarSystemCache *m_cache;
};

#endif /* _STARSYSTEM_H */
