// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "CollisionVisitor.h"

#include "CollMesh.h"
#include "CollisionGeometry.h"
#include "Group.h"
#include "MatrixTransform.h"
#include "StaticGeometry.h"
#include "collider/GeomTree.h"

#include "profiler/Profiler.h"

namespace SceneGraph {
	CollisionVisitor::CollisionVisitor() :
		m_boundingRadius(0.),
		m_properData(false),
		m_is_not_moved(false), // ...not moved but empty
		m_totalTris(0)
	{
		m_vertices.reserve(500);
		m_indices.reserve(500 * 3);
		m_flags.reserve(500);
		m_matrixStack.reserve(10);
	}

	void CollisionVisitor::ApplyStaticGeometry(StaticGeometry &g)
	{
		PROFILE_SCOPED()
		m_is_not_moved = true;
		if (m_matrixStack.empty()) {
			m_aabb.Update(g.m_boundingBox.min);
			m_aabb.Update(g.m_boundingBox.max);
		} else {
			const matrix4x4f &matrix = m_matrixStack.back();
			const vector3f min = matrix * vector3f(g.m_boundingBox.min);
			const vector3f max = matrix * vector3f(g.m_boundingBox.max);
			m_aabb.Update(vector3d(min));
			m_aabb.Update(vector3d(max));
		}
	}

	void CollisionVisitor::ApplyMatrixTransform(MatrixTransform &m)
	{
		PROFILE_SCOPED()
		m_is_not_moved = true;
		matrix4x4f matrix = matrix4x4f::Identity();
		if (!m_matrixStack.empty()) matrix = m_matrixStack.back();

		m_matrixStack.push_back(matrix * m.GetTransform());
		m.Traverse(*this);
		m_matrixStack.pop_back();
	}

	void CollisionVisitor::ApplyCollisionGeometry(CollisionGeometry &cg)
	{
		m_is_not_moved = true;
		if (cg.IsDynamic()) ApplyDynamicCollisionGeometry(cg);
		else ApplyStaticCollisionGeometry(cg);
	}

	void CollisionVisitor::ApplyStaticCollisionGeometry(CollisionGeometry &cg)
	{
		PROFILE_SCOPED()

		using std::vector;

		const matrix4x4f matrix = m_matrixStack.empty() ? matrix4x4f::Identity() : m_matrixStack.back();

		//copy data (with index offset)
		unsigned idxOffset = m_vertices.size();
		m_vertices.reserve(m_vertices.size() + cg.GetVertices().size());
		for (vector<vector3f>::const_iterator it = cg.GetVertices().begin(); it != cg.GetVertices().end(); ++it) {
			const vector3f pos = matrix * (*it);
			m_vertices.emplace_back(pos);
			m_aabb.Update(pos.x, pos.y, pos.z);
		}

		m_indices.reserve(m_indices.size() + cg.GetIndices().size());
		for (vector<uint32_t>::const_iterator it = cg.GetIndices().begin(); it != cg.GetIndices().end(); ++it)
			m_indices.emplace_back(*it + idxOffset);

		//at least some of the geoms should be default collision
		if (cg.GetTriFlag() == 0)
			m_properData = true;

		//iterator insert(const_iterator position, size_type n, const value_type& val);
		m_flags.insert(m_flags.end(), cg.GetIndices().size() / 3, cg.GetTriFlag());
	}

	void CollisionVisitor::ApplyDynamicCollisionGeometry(CollisionGeometry &cg)
	{
		PROFILE_SCOPED()
		//don't transform geometry, one geomtree per cg, create tree right away

		const int numIndices = cg.GetIndices().size();
		const int numTris = numIndices / 3;
		std::vector<vector3f> vertices = cg.GetVertices();
		std::vector<uint32_t> indices = cg.GetIndices();
		std::vector<unsigned> triFlags(numTris, cg.GetTriFlag());

		//create geomtree
		//takes ownership of data
		GeomTree *gt = new GeomTree(numTris,
			std::move(vertices), std::move(indices), std::move(triFlags));
		cg.SetGeomTree(gt);

		m_dynGeomTree.push_back(gt);

		m_totalTris += numTris;
	}

	void CollisionVisitor::AabbToMesh(const Aabb &bb)
	{
		PROFILE_SCOPED()
		std::vector<vector3f> &vts = m_vertices;
		std::vector<uint32_t> &ind = m_indices;
		const int offs = vts.size();

		const vector3f min(bb.min.x, bb.min.y, bb.min.z);
		const vector3f max(bb.max.x, bb.max.y, bb.max.z);
		const vector3f fbl(min.x, min.y, min.z); //front bottom left
		const vector3f fbr(max.x, min.y, min.z); //front bottom right
		const vector3f ftl(min.x, max.y, min.z); //front top left
		const vector3f ftr(max.x, max.y, min.z); //front top right
		const vector3f rtl(min.x, max.y, max.z); //rear top left
		const vector3f rtr(max.x, max.y, max.z); //rear top right
		const vector3f rbl(min.x, min.y, max.z); //rear bottom left
		const vector3f rbr(max.x, min.y, max.z); //rear bottom right

		vts.push_back(fbl); //0
		vts.push_back(fbr); //1
		vts.push_back(ftl); //2
		vts.push_back(ftr); //3

		vts.push_back(rtl); //4
		vts.push_back(rtr); //5
		vts.push_back(rbl); //6
		vts.push_back(rbr); //7

#define ADDTRI(_i1, _i2, _i3)  \
	ind.push_back(offs + _i1); \
	ind.push_back(offs + _i2); \
	ind.push_back(offs + _i3);
		//indices
		//Front face
		ADDTRI(3, 1, 0);
		ADDTRI(0, 2, 3);

		//Rear face
		ADDTRI(7, 5, 6);
		ADDTRI(6, 5, 4);

		//Top face
		ADDTRI(4, 5, 3);
		ADDTRI(3, 2, 4);

		//bottom face
		ADDTRI(1, 7, 6);
		ADDTRI(6, 0, 1);

		//left face
		ADDTRI(0, 6, 4);
		ADDTRI(4, 2, 0);

		//right face
		ADDTRI(5, 7, 1);
		ADDTRI(1, 3, 5);
#undef ADDTRI

		for (unsigned int i = 0; i < ind.size() / 3; i++)
			m_flags.push_back(0);
	}

	RefCountedPtr<CollMesh> CollisionVisitor::CreateCollisionMesh()
	{
		PROFILE_SCOPED()
		//Profiler::Timer timer;
		//timer.Start();

		if (!m_is_not_moved) throw std::runtime_error { "Call of CreateCollisionMesh while data is empty or moved out" };

		//convert from model AABB if no collisiongeoms found
		if (!m_properData)
			AabbToMesh(m_aabb);

		assert(!m_vertices.empty() && !m_indices.empty());

		m_totalTris += m_indices.size() / 3;

		//create geomtree
		//takes ownership of data
		GeomTree *gt = new GeomTree(m_indices.size() / 3,
			std::move(m_vertices), std::move(m_indices), std::move(m_flags));

		RefCountedPtr<CollMesh> collMesh;
		collMesh.Reset(new CollMesh(m_aabb, gt, m_dynGeomTree));

		m_boundingRadius = collMesh->GetAabb().GetRadius();

		m_vertices.clear();
		m_indices.clear();
		m_flags.clear();

		//timer.Stop();
		//Output(" - CreateCollisionMesh took: %lf milliseconds\n", timer.millicycles());

		m_is_not_moved = false;
		return collMesh;
	}
} // namespace SceneGraph
