// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _COLLISION_SPACE
#define _COLLISION_SPACE

#include "CollisionCallbackFwd.h"
#include "libs/vector3.h"
#include <list>

class Geom;
struct isect_t;
struct CollisionContact;

struct Sphere {
	vector3d pos;
	double radius;
	void *userData;
};

class BvhTree;

typedef std::list<Geom *> GeomList;

/*
 * Collision spaces have a bunch of geoms and at most one sphere (for a planet).
 */
class CollisionSpace {
public:
	CollisionSpace();
	~CollisionSpace();
	void AddGeom(Geom *);
	void RemoveGeom(Geom *);
	void AddStaticGeom(Geom *);
	void RemoveStaticGeom(Geom *);
	CollisionContact TraceRay(const vector3d &start, const vector3d &dir, double len, const Geom *ignore = nullptr);
	void Collide(CollCallback &callback);
	void SetSphere(const vector3d &pos, double radius, void *user_data)
	{
		sphere.pos = pos;
		sphere.radius = radius;
		sphere.userData = user_data;
	}
	void FlagRebuildObjectTrees() { m_needStaticGeomRebuild = true; }
	inline void RebuildObjectTrees();

	// Geoms with the same handle will not be collision tested against each other
	// should be used for geoms that are part of the same body
	// could also be used for autopiloted groups and LRCs near stations
	// zero means ungrouped. assumes that wraparound => no old crap left
	static int GetGroupHandle()
	{
		if (!s_nextHandle) s_nextHandle++;
		return s_nextHandle++;
	}

private:
	void CollideGeoms(Geom *a, int minMailboxValue, CollCallback &callback);
	void CollideRaySphere(const vector3d &start, const vector3d &dir, isect_t *isect);
	GeomList m_geoms;
	GeomList m_staticGeoms;
	bool m_needStaticGeomRebuild;
	BvhTree *m_staticObjectTree;
	BvhTree *m_dynamicObjectTree;
	Sphere sphere;

	size_t m_oldGeomsNumber;

	static int s_nextHandle;
};

#endif /* _COLLISION_SPACE */
