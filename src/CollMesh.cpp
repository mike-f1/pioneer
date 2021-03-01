// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "CollMesh.h"

#include "scenegraph/Serializer.h"
#include "collider/GeomTree.h"

#include "profiler/Profiler.h"

#include <stdexcept>

CollMesh::CollMesh(Aabb aabb, GeomTree *static_gt, std::vector<PairOfCollGeomGeomTree> dynamic_gt) :
	m_aabb(std::move(aabb)),
	m_geomTree(static_gt),
	m_dynGeomTrees(std::move(dynamic_gt)),
	m_totalTris(static_gt->GetNumTris())
{
	if (!static_gt) throw std::runtime_error { "CollMesh should be initialized with a non empty static GeomTree\n" };
}

CollMesh::~CollMesh()
{
	for (auto &it : m_dynGeomTrees)
		delete std::get<2>(it);
	delete m_geomTree;
}
