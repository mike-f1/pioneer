// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _COLLMESH_H
#define _COLLMESH_H

#include "Aabb.h"
#include "scenegraph/DynCollisionFwd.h"
#include "libs/matrix4x4.h"
#include "libs/RefCounted.h"

#include <vector>
#include <algorithm>

namespace SceneGraph {
	class CollisionGeometry;
	class MatrixTransform;
}

class GeomTree;

namespace Serializer {
	class Writer;
	class Reader;
} // namespace Serializer

//This simply stores the collision GeomTrees (both static and dynamic)
//and AABB for each model

class CollMesh final: public RefCounted {
public:
	CollMesh(Aabb aabb, GeomTree *static_gt, std::vector<TupleForDynCollision> dynamic_gt);
	~CollMesh();

	const Aabb &GetAabb() { return m_aabb; }

	double GetRadius() const { return m_aabb.GetRadius(); }

	const GeomTree *GetGeomTree() const { return m_geomTree; }

	const std::vector<TupleForDynCollision> &GetDynGeomTrees() const { return m_dynGeomTrees; }

	//for statistics
	unsigned int GetNumTriangles() const { return m_totalTris; }

private:
	Aabb m_aabb;
	GeomTree *m_geomTree;
	std::vector<TupleForDynCollision> m_dynGeomTrees;
	unsigned int m_totalTris;
};

#endif
