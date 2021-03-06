// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _FACTIONS_H
#define _FACTIONS_H

#include "Sector.h"
#include <memory>
#include <map>
#include <utility>
#include <vector>

class CustomSystem;
class Faction;
class Galaxy;

/* One day it might grow up to become a full tree, on the  other hand it might be
   cut down before it's full growth to be replaced by
   a proper spatial data structure.
*/

class FactionsDatabase {
public:
	FactionsDatabase(Galaxy *galaxy, const std::string &factionDir);
	~FactionsDatabase();

	void Init();
	void PostInit();
	void ClearCache() { ClearHomeSectors(); }
	bool IsInitialized() const;
	Galaxy *GetGalaxy() const { return m_galaxy; }
	void RegisterCustomSystem(CustomSystem *cs, const std::string &factionName);
	void AddFaction(Faction *faction);

	const Faction *GetFaction(const uint32_t index) const;
	const Faction *GetFaction(const std::string &factionName) const;
	const Faction *GetNearestClaimant(const Sector::System *sys) const;
	bool IsHomeSystem(const SystemPath &sysPath) const;

	uint32_t GetNumFactions() const;

	bool MayAssignFactions() const;

private:
	class Octsapling {
	public:
		void Add(Galaxy *galaxy, const Faction *faction);
		const std::vector<const Faction *> &CandidateFactions(const Sector::System *sys) const;

	private:
		std::vector<const Faction *> octbox[2][2][2];
		static int BoxIndex(int32_t sectorIndex) { return sectorIndex < 0 ? 0 : 1; };
		void PruneDuplicates(const int bx, const int by, const int bz);
	};

	typedef std::vector<Faction *> FactionList;
	typedef FactionList::iterator FactionIterator;
	typedef const std::vector<const Faction *> ConstFactionList;
	typedef ConstFactionList::const_iterator ConstFactionIterator;
	typedef std::map<std::string, Faction *> FactionMap;
	typedef std::set<SystemPath> HomeSystemSet;
	typedef std::map<std::string, std::list<CustomSystem *>> MissingFactionsMap;

	void ClearHomeSectors();
	void SetHomeSectors();

	Galaxy *const m_galaxy;
	const std::string m_factionDirectory;
	std::unique_ptr<Faction> m_no_faction; // instead of answering null, we often want to answer a working faction object for no faction
	FactionList m_factions;
	FactionMap m_factions_byName;
	HomeSystemSet m_homesystems;
	Octsapling m_spatial_index;
	bool m_may_assign_factions;
	bool m_initialized = false;
	MissingFactionsMap m_missingFactionsMap;
};

#endif /* _FACTIONS_H */
