// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _GALAXY_H
#define _GALAXY_H

#include "CustomSystemDB.h"
#include "Factions.h"
#include "GalaxyCache.h"
#include "JsonFwd.h"
#include "libs/RefCounted.h"
#include <cstdio>

struct SDL_Surface;
class GalaxyGenerator;
class StarSystem;

class Galaxy : public RefCounted {
protected:
	friend class GalaxyGenerator;
	Galaxy(RefCountedPtr<GalaxyGenerator> galaxyGenerator, float radius, float sol_offset_x, float sol_offset_y,
		const std::string &factionsDir, const std::string &customSysDir);
	void SetGalaxyGenerator(RefCountedPtr<GalaxyGenerator> galaxyGenerator);
	virtual void Init();

public:
	// lightyears
	const float GALAXY_RADIUS;
	const float SOL_OFFSET_X;
	const float SOL_OFFSET_Y;

	static RefCountedPtr<Galaxy> LoadFromJson(const Json &jsonObj);
	void ToJson(Json &jsonObj);

	~Galaxy();

	bool IsInitialized() const { return m_initialized; }
	/* 0 - 255 */
	virtual uint8_t GetSectorDensity(const int sx, const int sy, const int sz) const = 0;
	FactionsDatabase *GetFactions() { return &m_factions; } // XXX const correctness
	CustomSystemsDatabase *GetCustomSystems() { return &m_customSystems; } // XXX const correctness

	RefCountedPtr<const Sector> GetSector(const SystemPath &path) { return m_sectorCache.GetCached(path); }
	RefCountedPtr<Sector> GetMutableSector(const SystemPath &path) { return m_sectorCache.GetCached(path); }
	RefCountedPtr<SectorCache::Slave> NewSectorSlaveCache() { return m_sectorCache.NewSlaveCache(); }
	size_t FillSectorCache(RefCountedPtr<SectorCache::Slave> &sc, const SystemPath &center,
		int sectorRadius, StarSystemCache::CacheFilledCallback callback = StarSystemCache::CacheFilledCallback());

	vector3d GetInterSystemPosition(const SystemPath &source, const SystemPath &dest);

	RefCountedPtr<StarSystem> GetStarSystem(const SystemPath &path);
	RefCountedPtr<StarSystemCache::Slave> NewStarSystemSlaveCache() { return m_starSystemCache.NewSlaveCache(); }
	size_t FillStarSystemCache(RefCountedPtr<StarSystemCache::Slave> &ssc, const SystemPath &center,
		int sectorRadius, RefCountedPtr<SectorCache::Slave> &source);

	std::vector<RefCountedPtr<StarSystem>> GetNearStarSystemLy(const SystemPath &here, const double light_year);

	void FlushCaches();
	void Dump(FILE *file, int32_t centerX, int32_t centerY, int32_t centerZ, int32_t radius);

	RefCountedPtr<GalaxyGenerator> GetGenerator() const;
	const std::string &GetGeneratorName() const;
	int GetGeneratorVersion() const;

private:
	bool m_initialized;
	RefCountedPtr<GalaxyGenerator> m_galaxyGenerator;
	SectorCache m_sectorCache;
	StarSystemCache m_starSystemCache;
	FactionsDatabase m_factions;
	CustomSystemsDatabase m_customSystems;
};

class DensityMapGalaxy : public Galaxy {
private:
	friend class GalaxyGenerator;
	DensityMapGalaxy(RefCountedPtr<GalaxyGenerator> galaxyGenerator, const std::string &mapfile,
		float radius, float sol_offset_x, float sol_offset_y, const std::string &factionsDir, const std::string &customSysDir);

public:
	virtual uint8_t GetSectorDensity(const int sx, const int sy, const int sz) const override;

private:
	std::unique_ptr<float[]> m_galaxyMap;
	int32_t m_mapWidth, m_mapHeight;
};

#endif /* _GALAXY_H */
