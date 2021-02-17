// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _COLLMESH_H
#define _COLLMESH_H

#include "Aabb.h"
#include "libs/RefCounted.h"

#include <vector>
#include <algorithm>

class GeomTree;

namespace Serializer {
	class Writer;
	class Reader;
} // namespace Serializer

//This simply stores the collision GeomTrees
//and AABB.
class CollMesh final: public RefCounted {
public:
	CollMesh(Aabb aabb, GeomTree *static_gt, std::vector<GeomTree *> dynamic_gt);
	CollMesh(Serializer::Reader &rd);

	~CollMesh();

	void Save(Serializer::Writer &wr) const;

	const Aabb &GetAabb() { return m_aabb; }

	double GetRadius() const { return m_aabb.GetRadius(); }

	const GeomTree *GetGeomTree() const { return m_geomTree; }

	const std::vector<GeomTree *> &GetDynGeomTrees() const { return m_dynGeomTrees; }

	//for statistics
	unsigned int GetNumTriangles() const { return m_totalTris; }

protected:
	Aabb m_aabb;
	GeomTree *m_geomTree;
	std::vector<GeomTree *> m_dynGeomTrees;
	unsigned int m_totalTris;
};

#endif
