// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Geom.h"

#include "BVHTree.h"
#include "CollisionContact.h"
#include "CollisionSpace.h"
#include "GeomTree.h"

//#include <SDL_timer.h>

Geom::Geom(const GeomTree *geomtree, const matrix4x4d &m, const vector3d &pos, void *data) :
	m_orient(m),
	m_pos(pos),
	m_geomtree(geomtree),
	m_data(data),
	m_group(0),
	m_mailboxIndex(0),
	m_active(true)
{
	m_orient.SetTranslate(pos);
	m_invOrient = m_orient.Inverse();
}

/*matrix4x4d Geom::GetRotation() const
{
	PROFILE_SCOPED()
	matrix4x4d m = GetTransform();
	m[12] = 0; m[13] = 0; m[14] = 0;
	return m;
}*/

void Geom::MoveTo(const matrix4x4d &m)
{
	PROFILE_SCOPED()
	m_orient = m;
	m_pos = m_orient.GetTranslate();
	m_invOrient = m.Inverse();
}

void Geom::MoveTo(const matrix4x4d &m, const vector3d &pos)
{
	PROFILE_SCOPED()
	m_orient = m;
	m_pos = pos;
	m_orient.SetTranslate(pos);
	m_invOrient = m_orient.Inverse();
}

void Geom::CollideSphere(Sphere &sphere, CollisionContactVector &accum) const
{
	PROFILE_SCOPED()
	/* if the geom is actually within the sphere, create a contact so
	 * that we can't fall into spheres forever and ever */
	vector3d v = GetPosition() - sphere.pos;
	const double len = v.Length();
	if (len < sphere.radius) {
		accum.emplace_back(GetPosition(), (1.0 / len) * v, sphere.radius - len, 0, this->m_data, sphere.userData, 0x0);
		return;
	}
}

/*
 * This geom has moved, causing a possible collision with geom b.
 * Collide meshes to see.
 */
void Geom::Collide(Geom *b, CollisionContactVector &accum) const
{
	PROFILE_SCOPED()
	int max_contacts = MAX_CONTACTS;

	//unsigned int t = SDL_GetTicks();

	/* Collide this geom's edges against tri-mesh of geom b */
	matrix4x4d transTo = b->m_invOrient * m_orient;
	this->CollideEdgesWithTrisOf(max_contacts, b, transTo, accum);

	/* Collide b's edges against this geom's tri-mesh */
	if (max_contacts > 0) {
		transTo = m_invOrient * b->m_orient;
		b->CollideEdgesWithTrisOf(max_contacts, this, transTo, accum);
	}
	//t = SDL_GetTicks() - t;
	//int numEdges = GetGeomTree()->GetNumEdges() + b->GetGeomTree()->GetNumEdges();
	//Output("%d 'rays' in %dms (%f rps)\n", numEdges, t, 1000.0*numEdges / (double)t);
}

void Geom::SetCentralCylinder(std::unique_ptr<CSG_CentralCylinder> centralCylinder) {
	if ((centralCylinder == nullptr) ||
		(centralCylinder->m_diameter < 0.0) ||
		(centralCylinder->m_minH > centralCylinder->m_maxH)) {
		m_centralCylinder.reset();
		assert(0 && "Cylinder data aren't valid");
		return;
	}
	m_centralCylinder = std::move(centralCylinder);
}

void Geom::AddBox(std::unique_ptr<CSG_Box> box)
{
	m_Boxes.push_back(*std::move(box));
}

bool Geom::CheckCollisionCylinder(Geom* b, CollisionContactVector &accum)
{
	PROFILE_SCOPED();
	// NOTE: below check is inside this function to avoid cluttering of interface,
	// but it could be made faster having a dedicated inlined function
	if (!m_centralCylinder) return false;

	float max_dist = (m_centralCylinder->m_diameter / 2.0) - b->GetGeomTree()->GetRadius();
	// TODO: In order to ease math, pick radius instead of AAbb of the geom,
	// indeed that AAbb should be rotated and rebuilt
	const vector3f pos2 = vector3f((b->GetPosition() - GetPosition()) * GetTransform());
	// cylinder rotation axis is in y direction (see spacestations)
	const vector2f pos2xy(pos2.x, pos2.z);
	float dist_sqr = pos2xy.LengthSqr();

	if (dist_sqr < (max_dist * max_dist) &&
		(pos2.y < m_centralCylinder->m_maxH) &&
		(pos2.y > m_centralCylinder->m_minH)) {
			if (m_centralCylinder->m_shouldTriggerDocking) {
				accum.emplace_back(GetPosition(), vector3d(0.0), 0.1, 0, this->m_data, b->m_data, 0x10);
			}
			return true;
	}
	return false;
}

bool Geom::CheckBoxes(Geom* b, CollisionContactVector &accum)
{
	PROFILE_SCOPED();
	// NOTE: below check is inside this function to avoid cluttering of interface,
	// but it could be made faster having a dedicated inlined function
	if (m_Boxes.empty()) return false;

	const vector3f p = vector3f((b->GetPosition() - GetPosition()) * GetTransform());
	// TODO: In order to ease math, pick radius instead of AAbb of the geom,
	// indeed that AAbb should be rotated and rebuilt
	const float radius = b->GetGeomTree()->GetRadius();
	for (auto &box : m_Boxes) {
		bool collide = ((p.x >= box.m_min.x + radius) && (p.x <= box.m_max.x - radius) &&
			(p.y >= box.m_min.y + radius) && (p.y <= box.m_max.y - radius) &&
			(p.z >= box.m_min.z + radius) && (p.z <= box.m_max.z - radius));

		if (collide) {
			if (box.m_shouldTriggerDocking) {
				accum.emplace_back(GetPosition(), vector3d(0.0), 0.1, 0, this->m_data, b->m_data, 0x10);
			}
			return true;
		}
	}
	return false;
}

static bool rotatedAabbIsectsNormalOne(Aabb &a, const matrix4x4d &transA, Aabb &b)
{
	PROFILE_SCOPED()
	Aabb arot;
	constexpr unsigned points_size = 8;
	std::array<vector3d, points_size> p {
		vector3d(a.min.x, a.min.y, a.min.z),
		vector3d(a.min.x, a.min.y, a.max.z),
		vector3d(a.min.x, a.max.y, a.min.z),
		vector3d(a.min.x, a.max.y, a.max.z),
		vector3d(a.max.x, a.min.y, a.min.z),
		vector3d(a.max.x, a.min.y, a.max.z),
		vector3d(a.max.x, a.max.y, a.min.z),
		vector3d(a.max.x, a.max.y, a.max.z),
	};

	for (unsigned i = 0; i < points_size; i++)
		p[i] = transA * p[i];

	arot.min = arot.max = p[0];
	for (unsigned i = 1; i < points_size; i++)
		arot.Update(p[i]);

	return b.Intersects(arot);
}

/*
 * Intersect this Geom's edge BVH tree with geom b's triangle BVH tree.
 * Generate collision contacts.
 */
void Geom::CollideEdgesWithTrisOf(int &maxContacts, const Geom *b, const matrix4x4d &transTo, CollisionContactVector &accum) const
{
	PROFILE_SCOPED()
	struct stackobj {
		BVHNode *edgeNode;
		BVHNode *triNode;
	} stack[32];
	int stackpos = 0;

	stack[0].edgeNode = GetGeomTree()->GetEdgeTree()->GetRoot();
	stack[0].triNode = b->GetGeomTree()->GetTriTree()->GetRoot();

	while ((stackpos >= 0) && (maxContacts > 0)) {
		BVHNode *edgeNode = stack[stackpos].edgeNode;
		BVHNode *triNode = stack[stackpos].triNode;
		stackpos--;

		// does the edgeNode (with its aabb described in 6 planes transformed and rotated to
		// b's coordinates) intersect with one or other of b's child nodes?
		if (triNode->triIndicesStart || edgeNode->triIndicesStart) {
			// reached triangle leaf node or edge leaf node.
			// Intersect all edges under edgeNode with this leaf
			CollideEdgesTris(maxContacts, edgeNode, transTo, b, triNode, accum);
		} else {
			BVHNode *left = triNode->kids[0];
			BVHNode *right = triNode->kids[1];
			bool edgeNodeIsectsLeftChild = rotatedAabbIsectsNormalOne(edgeNode->aabb, transTo, left->aabb);
			bool edgeNodeIsectsRightChild = rotatedAabbIsectsNormalOne(edgeNode->aabb, transTo, right->aabb);
			//edgeNodeIsectsRightChild = edgeNodeIsectsLeftChild = true;
			if (edgeNodeIsectsRightChild) {
				if (edgeNodeIsectsLeftChild) {
					// isects both. split edgeNode and try again
					++stackpos;
					stack[stackpos].edgeNode = edgeNode->kids[0];
					stack[stackpos].triNode = triNode;
					++stackpos;
					stack[stackpos].edgeNode = edgeNode->kids[1];
					stack[stackpos].triNode = triNode;
				} else {
					// hits only right child. go down into that
					// side with same edge node
					++stackpos;
					stack[stackpos].edgeNode = edgeNode;
					stack[stackpos].triNode = triNode->kids[1];
				}
			} else if (edgeNodeIsectsLeftChild) {
				// hits only left child
				++stackpos;
				stack[stackpos].edgeNode = edgeNode;
				stack[stackpos].triNode = triNode->kids[0];
			} else {
				// hits none
			}
		}
	}
}

/*
 * Collide one edgeNode (all edges below it) of this Geom with the triangle
 * BVH of another geom (b), starting from btriNode.
 */
void Geom::CollideEdgesTris(int &maxContacts, const BVHNode *edgeNode, const matrix4x4d &transToB,
	const Geom *b, const BVHNode *btriNode, CollisionContactVector &accum) const
{
	PROFILE_SCOPED()
	if (maxContacts <= 0) return;
	if (edgeNode->triIndicesStart) {
		const GeomTree::Edge *edges = this->GetGeomTree()->GetEdges();
		vector3f dir;
		isect_t isect;
		const std::vector<vector3f> &rVertices = GetGeomTree()->GetVertices();
		for (int i = 0; i < edgeNode->numTris; i++) {
			const int vtxNum = edges[edgeNode->triIndicesStart[i]].v1i;
			const vector3d v1 = transToB * vector3d(rVertices[vtxNum]);
			const vector3f _from(float(v1.x), float(v1.y), float(v1.z));

			vector3d _dir(
				double(edges[edgeNode->triIndicesStart[i]].dir.x),
				double(edges[edgeNode->triIndicesStart[i]].dir.y),
				double(edges[edgeNode->triIndicesStart[i]].dir.z));
			_dir = transToB.ApplyRotationOnly(_dir);
			dir = vector3f(&_dir.x);
			isect.dist = edges[edgeNode->triIndicesStart[i]].len;
			isect.triIdx = -1;

			b->GetGeomTree()->TraceRay(btriNode, _from, dir, &isect);

			if (isect.triIdx == -1) continue;
			const double depth = edges[edgeNode->triIndicesStart[i]].len - isect.dist;
			// in world coords
			vector3d normal = vector3d(b->m_geomtree->GetTriNormal(isect.triIdx));

			accum.emplace_back(b->GetTransform() * (v1 + vector3d(&dir.x) * double(isect.dist)),
				b->GetTransform().ApplyRotationOnly(normal),
				depth,
				isect.triIdx,
				this->m_data,
				b->m_data,
				// contact geomFlag is bitwise OR of triangle's and edge's flags
				b->m_geomtree->GetTriFlag(isect.triIdx) | edges[edgeNode->triIndicesStart[i]].triFlag);
			accum.back().distance = isect.dist;
			if (--maxContacts <= 0) return;
		}
	} else {
		CollideEdgesTris(maxContacts, edgeNode->kids[0], transToB, b, btriNode, accum);
		CollideEdgesTris(maxContacts, edgeNode->kids[1], transToB, b, btriNode, accum);
	}
}
