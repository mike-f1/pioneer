// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _SCENEGRAPH_COLLISIONGEOMETRY_H
#define _SCENEGRAPH_COLLISIONGEOMETRY_H

/*
 * Non-renderable geometry node which CollisionVisitor can use
 * to build a collision mesh.
 */

#include "Node.h"

#include "libs/vector3.h"
#include <vector>
#include <cstdint>

class GeomTree;
class Geom;

namespace SceneGraph {
	class CollisionGeometry final: public Node {
	public:
		CollisionGeometry(const std::vector<vector3f> &, const std::vector<uint32_t> &, unsigned int flag);
		CollisionGeometry(const CollisionGeometry &, NodeCopyCache *cache = 0);

		Node *Clone(NodeCopyCache *cache = 0) override;
		~CollisionGeometry();

		const char *GetTypeName() const override { return "CollisionGeometry"; }
		void Accept(NodeVisitor &nv) override;
		void Save(NodeDatabase &) override;
		static CollisionGeometry *Load(NodeDatabase &);

		const std::vector<vector3f> &GetVertices() const { return m_vertices; }
		const std::vector<uint32_t> &GetIndices() const { return m_indices; }
		unsigned int GetTriFlag() const { return m_triFlag; }

		bool IsDynamic() const { return m_dynamic; }
		void SetDynamic(bool b) { m_dynamic = b; }

		//for linking game collision objects with these nodes
		GeomTree *GetGeomTree() const { return m_geomTree; }
		void SetGeomTree(GeomTree *c) { m_geomTree = c; }

		Geom *GetGeom() const { return m_geom; }
		void SetGeom(Geom *g) { m_geom = g; }

	private:
		std::vector<vector3f> m_vertices;
		std::vector<uint32_t> m_indices;
		unsigned int m_triFlag; //only one per node
		bool m_dynamic;

		//for dynamic collisions
		GeomTree *m_geomTree;
		Geom *m_geom;
	};
} // namespace SceneGraph

#endif
