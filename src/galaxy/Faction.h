// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef FACTION_H
#define FACTION_H

#include "Color.h"
#include "DeleteEmitter.h"
#include "Economy.h"
#include "Polit.h"
#include "Sector.h"
#include "libs/fixed.h"

class Galaxy;

class Faction : public DeleteEmitter {
	friend class FactionsDatabase;

public:
	static const uint32_t BAD_FACTION_IDX; // used by the no faction object to denote it's not a proper faction
	static const Color BAD_FACTION_COLOUR; // factionColour to use on failing to find an appropriate faction
	static const float FACTION_BASE_ALPHA; // Alpha to use on factionColour of systems with unknown population

	Faction();

	uint32_t idx; // faction index
	std::string name; // Formal name "Federation", "Empire", "Bob's Rib-shack consortium of delicious worlds (tm)", etc.
	std::string description_short; // short description
	std::string description; // detailed description describing formation, current status, etc

	// government types with weighting
	typedef std::pair<Polit::GovType, int32_t> GovWeight;
	typedef std::vector<GovWeight> GovWeightVec;
	typedef GovWeightVec::const_iterator GovWeightIterator;
	GovWeightVec govtype_weights;
	int32_t govtype_weights_total;

	bool hasHomeworld;
	SystemPath homeworld; // sector(x,y,x) + system index + body index = location in a (custom?) system of homeworld
	double foundingDate; // date faction came into existence
	double expansionRate; // lightyears per year that the volume expands.
	std::string military_name; // "Space Defense Force", "Imperial Will Enforcement Division"...
	//military logo
	std::string police_name; // "Police", "Polizia Locale"...
	std::string police_ship; // "kanara", "varada"...
	//police logo
	//goods/equipment availability (1-per-economy-type: aka agricultural, industrial, tourist, etc)

	typedef std::vector<SystemPath> ClaimList;
	ClaimList m_ownedsystemlist;
	void PushClaim(const SystemPath &path) { m_ownedsystemlist.push_back(path); }
	bool IsClaimed(const SystemPath &) const;

	// commodity legality
	typedef std::map<GalacticEconomy::Commodity, uint32_t> CommodityProbMap;
	CommodityProbMap commodity_legality;

	Color colour;

	double Radius() const { return (FACTION_CURRENT_YEAR - foundingDate) * expansionRate; };
	bool IsValid() const { return idx != BAD_FACTION_IDX; };
	Color AdjustedColour(fixed population, bool inRange) const;
	Polit::GovType PickGovType(Random &rand) const;

	// set the homeworld to one near the supplied co-ordinates
	void SetBestFitHomeworld(Galaxy *galaxy, int32_t x, int32_t y, int32_t z, int32_t si, uint32_t bi, int32_t axisChange);
	RefCountedPtr<const Sector> GetHomeSector(Galaxy *galaxy) const;

private:
	static const double FACTION_CURRENT_YEAR; // used to calculate faction radius

	mutable RefCountedPtr<const Sector> m_homesector; // cache of home sector to use in distance calculations
	const bool IsCloserAndContains(Galaxy *galaxy, double &closestFactionDist, const Sector::System *sys) const;
};

#endif // FACTION_H
