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

CollMesh::CollMesh(Serializer::Reader &rd)
{
	PROFILE_SCOPED()
	m_aabb.max = rd.Vector3d();
	m_aabb.min = rd.Vector3d();
	m_aabb.radius = rd.Double();

	m_geomTree = new GeomTree(rd);

	const uint32_t numDynGeomTrees = rd.Int32();
	m_dynGeomTrees.reserve(numDynGeomTrees);
	for (uint32_t it = 0; it < numDynGeomTrees; ++it) {
		m_dynGeomTrees.push_back({nullptr, nullptr, new GeomTree(rd)}); //FIXME Soooner!!!!!!!!!!
	}

	m_totalTris = rd.Int32();
}

CollMesh::~CollMesh()
{
	for (auto &it : m_dynGeomTrees)
		delete std::get<2>(it);
	delete m_geomTree;
}

void CollMesh::Save(Serializer::Writer &wr) const
{
	PROFILE_SCOPED()
	wr.Vector3d(m_aabb.max);
	wr.Vector3d(m_aabb.min);
	wr.Double(m_aabb.radius);

	m_geomTree->Save(wr);

	wr.Int32(m_dynGeomTrees.size());
	for (auto &it : m_dynGeomTrees) {
		std::get<2>(it)->Save(wr);
	}

	wr.Int32(m_totalTris);
}
