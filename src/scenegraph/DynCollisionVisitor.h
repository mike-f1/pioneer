#ifndef _DYNCOLLISIONVISITOR_H
#define _DYNCOLLISIONVISITOR_H

#include "CollisionGeometry.h"
#include "MatrixTransform.h"
#include "NodeVisitor.h"
#include "collider/Geom.h"
#include "libs/matrix4x4.h"

class GeomTree;

namespace SceneGraph {

	class DynGeomFinder : public NodeVisitor {
	public:
		virtual void ApplyCollisionGeometry(CollisionGeometry &cg)
		{
			if (cg.IsDynamic()) {
				results.push_back(&cg);
				printf("push %p (size = %lu)\n", &cg, results.size());
			}
		}

		CollisionGeometry *FindCgForTree(GeomTree *t)
		{
			printf("Results size %lu\n", results.size());
			for (auto it = results.begin(); it != results.end(); ++it) {
				//printf("check with %p\n", (*it)->GetGeomTree());
				//if ((*it)->GetGeomTree() == t) return (*it);
			}
			printf("Not found\n");
			return nullptr;
		}

	private:
		std::vector<CollisionGeometry *> results;
	};

	class DynCollUpdateVisitor : public NodeVisitor {
	public:
		virtual void ApplyMatrixTransform(MatrixTransform &m)
		{
			matrix4x4f matrix = matrix4x4f::Identity();
			if (!m_matrixStack.empty()) matrix = m_matrixStack.back();

			m_matrixStack.push_back(matrix * m.GetTransform());
			m.Traverse(*this);
			m_matrixStack.pop_back();
		}

		virtual void ApplyCollisionGeometry(CollisionGeometry &cg)
		{
			//if (!cg.GetGeom()) return;

			//matrix4x4ftod(m_matrixStack.back(), cg.GetGeom()->m_animTransform);
		}

	private:
		std::vector<matrix4x4f> m_matrixStack;
	};

}

#endif
