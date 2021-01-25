// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _COLLISIONVISITOR_H
#define _COLLISIONVISITOR_H
/*
 * Creates a new collision mesh from CollisionGeometry nodes
 * or the nodes' AABB, when no CGeoms found.
 */
#include "NodeVisitor.h"

#include "libs/RefCounted.h"
#include "libs/matrix4x4.h"
#include "libs/vector3.h"
#include <cstdint>
#include <vector>

class Aabb;
class CollMesh;

namespace SceneGraph {
	class Group;
	class MatrixTransform;
	class StaticGeometry;

	class CollisionVisitor final: public NodeVisitor {
	public:
		CollisionVisitor();
		void ApplyStaticGeometry(StaticGeometry &) override;
		void ApplyMatrixTransform(MatrixTransform &) override;
		void ApplyCollisionGeometry(CollisionGeometry &) override;
		//call after traversal complete
		RefCountedPtr<CollMesh> CreateCollisionMesh();
		float GetBoundingRadius() const { return m_boundingRadius; }

	private:
		void ApplyDynamicCollisionGeometry(CollisionGeometry &);
		void AabbToMesh(const Aabb &);
		//geomtree is not built until all nodes are visited and
		//BuildCollMesh called
		RefCountedPtr<CollMesh> m_collMesh;
		std::vector<matrix4x4f> m_matrixStack;
		float m_boundingRadius;
		bool m_properData;

		//temporary arrays for static geometry
		std::vector<vector3f> m_vertices;
		std::vector<uint32_t> m_indices;
		std::vector<uint32_t> m_flags;

		uint32_t m_totalTris;
	};
} // namespace SceneGraph
#endif
