// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _GEOM_H
#define _GEOM_H

#include <memory>
#include <vector>

#include "CollisionCallbackFwd.h"
#include "libs/matrix4x4.h"
#include "libs/vector3.h"

#include "CSGDefinitions.h"

static constexpr unsigned MAX_CONTACTS = 8;

struct CollisionContact;
class GeomTree;
struct isect_t;
struct Sphere;
struct BVHNode;

class Geom {
public:
	Geom(const GeomTree *geomtree, const matrix4x4d &m, const vector3d &pos, void *data);

	void MoveTo(const matrix4x4d &m);
	void MoveTo(const matrix4x4d &m, const vector3d &pos);
	inline const matrix4x4d &GetInvTransform() const { return m_invOrient; }
	inline const matrix4x4d &GetTransform() const { return m_orient; }
	//matrix4x4d GetRotation() const;
	inline const vector3d &GetPosition() const { return m_pos; }
	inline void Enable() { m_active = true; }
	inline void Disable() { m_active = false; }
	inline bool IsEnabled() const { return m_active; }
	inline const GeomTree *GetGeomTree() const { return m_geomtree; }
	void Collide(Geom *b, CollisionContactVector &accum) const;
	void CollideSphere(Sphere &sphere, CollisionContactVector &accum) const;
	inline void *GetUserData() const { return m_data; }
	inline void SetMailboxIndex(int idx) { m_mailboxIndex = idx; }
	inline int GetMailboxIndex() const { return m_mailboxIndex; }
	inline void SetGroup(int g) { m_group = g; }
	inline int GetGroup() const { return m_group; }

	matrix4x4d m_animTransform;

	void SetCentralCylinder(std::unique_ptr<CSG_CentralCylinder> centralcylinder);
	void AddBox(std::unique_ptr<CSG_Box> box);

	bool CheckCollisionCylinder(Geom* b, CollisionContactVector &accum);
	bool CheckBoxes(Geom* b, CollisionContactVector &accum);

private:
	void CollideEdgesWithTrisOf(int &maxContacts, const Geom *b, const matrix4x4d &transTo, CollisionContactVector &accum) const;
	void CollideEdgesTris(int &maxContacts, const BVHNode *edgeNode, const matrix4x4d &transToB,
		const Geom *b, const BVHNode *btriNode, CollisionContactVector &accum) const;

	// double-buffer position so we can keep previous position
	matrix4x4d m_orient, m_invOrient;
	vector3d m_pos;
	const GeomTree *m_geomtree;
	void *m_data;
	int m_group;
	int m_mailboxIndex; // used to avoid duplicate collisions
	bool m_active;

	std::vector<CSG_Box> m_Boxes;
	std::unique_ptr<CSG_CentralCylinder> m_centralCylinder;
};

#endif /* _GEOM_H */
