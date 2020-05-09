// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _SYSTEMPATH_H
#define _SYSTEMPATH_H

#include "JsonFwd.h"
#include "LuaWrappable.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>

class SystemPath : public LuaWrappable {
public:
	struct ParseFailure : public std::invalid_argument {
		ParseFailure() :
			std::invalid_argument("invalid SystemPath format") {}
	};
	static SystemPath Parse(const char *const str);

	SystemPath() :
		sectorX(0),
		sectorY(0),
		sectorZ(0),
		systemIndex(std::numeric_limits<uint32_t>::max()),
		bodyIndex(std::numeric_limits<uint32_t>::max()) {}

	SystemPath(int32_t x, int32_t y, int32_t z) :
		sectorX(x),
		sectorY(y),
		sectorZ(z),
		systemIndex(std::numeric_limits<uint32_t>::max()),
		bodyIndex(std::numeric_limits<uint32_t>::max()) {}
	SystemPath(int32_t x, int32_t y, int32_t z, uint32_t si) :
		sectorX(x),
		sectorY(y),
		sectorZ(z),
		systemIndex(si),
		bodyIndex(std::numeric_limits<uint32_t>::max()) {}
	SystemPath(int32_t x, int32_t y, int32_t z, uint32_t si, uint32_t bi) :
		sectorX(x),
		sectorY(y),
		sectorZ(z),
		systemIndex(si),
		bodyIndex(bi) {}

	SystemPath(const SystemPath &path) :
		sectorX(path.sectorX),
		sectorY(path.sectorY),
		sectorZ(path.sectorZ),
		systemIndex(path.systemIndex),
		bodyIndex(path.bodyIndex) {}
	SystemPath(const SystemPath *path) :
		sectorX(path->sectorX),
		sectorY(path->sectorY),
		sectorZ(path->sectorZ),
		systemIndex(path->systemIndex),
		bodyIndex(path->bodyIndex) {}

	int32_t sectorX;
	int32_t sectorY;
	int32_t sectorZ;
	uint32_t systemIndex;
	uint32_t bodyIndex;

	friend bool operator==(const SystemPath &a, const SystemPath &b)
	{
		if (a.sectorX != b.sectorX) return false;
		if (a.sectorY != b.sectorY) return false;
		if (a.sectorZ != b.sectorZ) return false;
		if (a.systemIndex != b.systemIndex) return false;
		if (a.bodyIndex != b.bodyIndex) return false;
		return true;
	}

	friend bool operator!=(const SystemPath &a, const SystemPath &b)
	{
		return !(a == b);
	}

	friend bool operator<(const SystemPath &a, const SystemPath &b)
	{
		if (a.sectorX != b.sectorX) return (a.sectorX < b.sectorX);
		if (a.sectorY != b.sectorY) return (a.sectorY < b.sectorY);
		if (a.sectorZ != b.sectorZ) return (a.sectorZ < b.sectorZ);
		if (a.systemIndex != b.systemIndex) return (a.systemIndex < b.systemIndex);
		return (a.bodyIndex < b.bodyIndex);
	}

	static inline double SectorDistance(const SystemPath &a, const SystemPath &b)
	{
		const int32_t x = b.sectorX - a.sectorX;
		const int32_t y = b.sectorY - a.sectorY;
		const int32_t z = b.sectorZ - b.sectorZ;
		return sqrt(x * x + y * y + z * z); // sqrt is slow
	}

	static inline double SectorDistanceSqr(const SystemPath &a, const SystemPath &b)
	{
		const int32_t x = b.sectorX - a.sectorX;
		const int32_t y = b.sectorY - a.sectorY;
		const int32_t z = b.sectorZ - b.sectorZ;
		return (x * x + y * y + z * z); // return the square of the distance
	}

	class LessSectorOnly {
	public:
		bool operator()(const SystemPath &a, const SystemPath &b) const
		{
			if (a.sectorX != b.sectorX) return (a.sectorX < b.sectorX);
			if (a.sectorY != b.sectorY) return (a.sectorY < b.sectorY);
			return (a.sectorZ < b.sectorZ);
		}
	};

	class LessSystemOnly {
	public:
		bool operator()(const SystemPath &a, const SystemPath &b) const
		{
			if (a.sectorX != b.sectorX) return (a.sectorX < b.sectorX);
			if (a.sectorY != b.sectorY) return (a.sectorY < b.sectorY);
			if (a.sectorZ != b.sectorZ) return (a.sectorZ < b.sectorZ);
			return (a.systemIndex < b.systemIndex);
		}
	};

	bool IsSectorPath() const
	{
		return (systemIndex == std::numeric_limits<uint32_t>::max() && bodyIndex == std::numeric_limits<uint32_t>::max());
	}

	bool IsSystemPath() const
	{
		return (systemIndex != std::numeric_limits<uint32_t>::max() && bodyIndex == std::numeric_limits<uint32_t>::max());
	}
	bool HasValidSystem() const
	{
		return (systemIndex != std::numeric_limits<uint32_t>::max());
	}

	bool IsBodyPath() const
	{
		return (systemIndex != std::numeric_limits<uint32_t>::max() && bodyIndex != std::numeric_limits<uint32_t>::max());
	}
	bool HasValidBody() const
	{
		assert((bodyIndex == std::numeric_limits<uint32_t>::max()) || (systemIndex != std::numeric_limits<uint32_t>::max()));
		return (bodyIndex != std::numeric_limits<uint32_t>::max());
	}

	bool IsSameSector(const SystemPath &b) const
	{
		if (sectorX != b.sectorX) return false;
		if (sectorY != b.sectorY) return false;
		if (sectorZ != b.sectorZ) return false;
		return true;
	}

	bool IsSameSystem(const SystemPath &b) const
	{
		assert(HasValidSystem());
		assert(b.HasValidSystem());
		if (sectorX != b.sectorX) return false;
		if (sectorY != b.sectorY) return false;
		if (sectorZ != b.sectorZ) return false;
		if (systemIndex != b.systemIndex) return false;
		return true;
	}

	SystemPath SectorOnly() const
	{
		return SystemPath(sectorX, sectorY, sectorZ);
	}

	SystemPath SystemOnly() const
	{
		assert(systemIndex != std::numeric_limits<uint32_t>::max());
		return SystemPath(sectorX, sectorY, sectorZ, systemIndex);
	}

	void ToJson(Json &jsonObj) const;
	static SystemPath FromJson(const Json &jsonObj);

	// sometimes it's useful to be able to get the SystemPath data as a blob
	// (for example, to be used for hashing)
	// see, LuaObject<SystemPath>::PushToLua in LuaSystemPath.cpp
	static_assert(sizeof(int32_t) == sizeof(uint32_t), "something crazy is going on!");
	static const size_t SizeAsBlob = 5 * sizeof(uint32_t);
	void SerializeToBlob(char *blob) const
	{
		// could just memcpy(blob, this, sizeof(SystemPath))
		// but that might include packing and/or vtable pointer
		memcpy(blob + 0 * sizeof(uint32_t), &sectorX, sizeof(uint32_t));
		memcpy(blob + 1 * sizeof(uint32_t), &sectorY, sizeof(uint32_t));
		memcpy(blob + 2 * sizeof(uint32_t), &sectorZ, sizeof(uint32_t));
		memcpy(blob + 3 * sizeof(uint32_t), &systemIndex, sizeof(uint32_t));
		memcpy(blob + 4 * sizeof(uint32_t), &bodyIndex, sizeof(uint32_t));
	}
};

#endif
