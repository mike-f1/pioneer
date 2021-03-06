// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Faction.h"

#include <limits.h>
#include "Lang.h"
#include "Galaxy.h"
#include "StarSystem.h"

#include "profiler/Profiler.h"

const uint32_t Faction::BAD_FACTION_IDX = UINT_MAX;
const Color Faction::BAD_FACTION_COLOUR = Color(204, 204, 204, 128);
const float Faction::FACTION_BASE_ALPHA = 0.40f;
const double Faction::FACTION_CURRENT_YEAR = 3200;

Faction::Faction() :
	idx(BAD_FACTION_IDX),
	name(Lang::NO_CENTRAL_GOVERNANCE),
	hasHomeworld(false),
	foundingDate(0.0),
	expansionRate(0.0),
	colour(BAD_FACTION_COLOUR),
	m_homesector(0)
{
	PROFILE_SCOPED()
	govtype_weights_total = 0;
}

bool Faction::IsClaimed(const SystemPath &path) const
{
	// check the factions list of claimed systems/sectors, if there is one
	SystemPath sector = path;
	sector.systemIndex = -99;

	for (auto clam = m_ownedsystemlist.begin(); clam != m_ownedsystemlist.end(); clam++) {
		if (*clam == sector || *clam == path)
			return true;
	}
	return false;
}

/*	Answer whether the faction both contains the sysPath, and has a homeworld
	closer than the passed distance.

	if it is, then the passed distance will also be updated to be the distance
	from the factions homeworld to the sysPath.
*/
const bool Faction::IsCloserAndContains(Galaxy *galaxy, double &closestFactionDist, const Sector::System *sys) const
{
	PROFILE_SCOPED()
	/*	Treat factions without homeworlds as if they are of effectively infinite radius,
		so every world is potentially within their borders, but also treat them as if
		they had a homeworld that was infinitely far away, so every other faction has
		a better claim.
	*/
	float distance = HUGE_VAL;
	bool inside = true;

	/*	Factions that have a homeworld... */
	if (hasHomeworld) {
		/* ...automatically gain the allegiance of worlds within the same sector... */
		if (sys->InSameSector(homeworld)) {
			distance = 0;
		}

		/* ...otherwise we need to calculate whether the world is inside the
		   the faction border, and how far away it is. */
		else {
			RefCountedPtr<const Sector> homeSec = GetHomeSector(galaxy);
			const Sector::System *homeSys = &homeSec->m_systems[homeworld.systemIndex];
			distance = Sector::System::DistanceBetween(homeSys, sys);
			inside = distance < Radius();
		}
	}

	/*	if the faction contains the world, and its homeworld is closer, then this faction
		wins, and we update the closestFactionDist */
	if (inside && (distance <= closestFactionDist)) {
		closestFactionDist = distance;
		return true;

		/* otherwise this isn't the faction we were looking for */
	} else {
		return false;
	}
}

Color Faction::AdjustedColour(fixed population, bool inRange) const
{
	PROFILE_SCOPED()
	Color result;
	// Unexplored: population = -1, Uninhabited: population = 0.
	result = population <= 0 ? BAD_FACTION_COLOUR : colour;
	result.a = population > 0 ? (FACTION_BASE_ALPHA + (M_E + (logf(population.ToFloat() / 1.25))) / ((2 * M_E) + FACTION_BASE_ALPHA)) * 255 : FACTION_BASE_ALPHA * 255;
	result.a = inRange ? 255 : result.a;
	return result;
}

Polit::GovType Faction::PickGovType(Random &rand) const
{
	PROFILE_SCOPED()
	if (!govtype_weights.empty()) {
		// if we roll a number between one and the total weighting...
		int32_t roll = rand.Int32(1, govtype_weights_total);
		int32_t cumulativeWeight = 0;

		// ...the first govType with a cumulative weight >= the roll should be our pick
		GovWeightIterator it = govtype_weights.begin();
		while (roll > (cumulativeWeight + it->second)) {
			cumulativeWeight += it->second;
			++it;
		}
		return it->first;
	} else {
		return Polit::GOV_INVALID;
	}
}

/* If the si is negative, set the homeworld to our best shot at a system path
    pointing to a valid system, close to passed co-ordinates.

   Otherwise trust the caller, and just set the system path for the co-ordinates.

   Used by the Lua interface, to support autogenerated factions.
*/
void Faction::SetBestFitHomeworld(Galaxy *galaxy, int32_t x, int32_t y, int32_t z, int32_t si, uint32_t bi, int32_t axisChange)
{
	PROFILE_SCOPED()
	// search for a home system until we either find one sutiable, hit one of the axes,
	// or hit the edge of inhabited space
	uint32_t i = 0;
	RefCountedPtr<StarSystem> sys;
	while (si < 0 && (abs(x) != 90 && abs(y) != 90 && abs(z) != 90) && (x != 0 && y != 0 && z != 0)) {

		SystemPath path(x, y, z);
		// search for a suitable homeworld in the current sector
		RefCountedPtr<const Sector> sec = galaxy->GetSector(path);
		int32_t candidateSi = 0;
		while (uint32_t(candidateSi) < sec->m_systems.size()) {
			path.systemIndex = candidateSi;
			sys = galaxy->GetStarSystem(path);
			if (sys->HasSpaceStations()) {
				si = candidateSi;
				std::map<SystemPath, int> stationCount;
				for (auto station : sys->GetSpaceStations()) {
					if (stationCount.find(station->GetParent()->GetPath()) == stationCount.end()) {
						// new parent
						stationCount[station->GetParent()->GetPath()] = 1;
					} else {
						// increment count
						stationCount[station->GetParent()->GetPath()]++;
					}
				}
				uint32_t candidateBi = 0;
				int32_t candidateCount = 0;
				for (auto count : stationCount) {
					if (candidateCount < count.second) {
						candidateBi = count.first.bodyIndex;
						candidateCount = count.second;
					}
				}
				bi = candidateBi;
				break;
			}
			candidateSi++;
		}

		// set the co-ordinates of the next sector to examine, cycling through x, y and z
		if (si < 0 && i % 3 == 0) {
			if (x >= 0) {
				x = x + axisChange;
			} else {
				x = x - axisChange;
			};
		} else if (si < 0 && i % 3 == 1) {
			if (y >= 0) {
				y = y + axisChange;
			} else {
				y = y - axisChange;
			};
		} else if (si < 0 && i % 3 == 2) {
			if (z >= 0) {
				z = z + axisChange;
			} else {
				z = z - axisChange;
			};
		}
		i++;
	}
	homeworld = SystemPath(x, y, z, si, bi);
}

RefCountedPtr<const Sector> Faction::GetHomeSector(Galaxy *galaxy) const
{
	if (!m_homesector) // This will later be replaced by a Sector from the cache
		m_homesector = galaxy->GetSector(homeworld);
	return m_homesector;
}
