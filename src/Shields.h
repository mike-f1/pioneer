// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _SHIELDS_H_
#define _SHIELDS_H_
/*
 * Mesh shields for ships and other objects.
 */

#include "Color.h"
#include "JsonFwd.h"
#include "graphics/ShieldRenderParameters.h"
#include "libs/RefCounted.h"
#include "libs/matrix4x4.h"
#include "libs/vector3.h"
#include <deque>
#include <vector>

namespace SceneGraph {
	class Model;
	class StaticGeometry;
} // namespace SceneGraph

class Shields {
public:
	struct Shield {
		Shield(const Color3ub &color, const matrix4x4f &matrix, SceneGraph::StaticGeometry *sg);
		Color3ub m_colour; // I'm English, so it's "colour" ;)
		matrix4x4f m_matrix;
		RefCountedPtr<SceneGraph::StaticGeometry> m_mesh;
	};

	explicit Shields(SceneGraph::Model *);
	virtual ~Shields();
	virtual void SaveToJson(Json &jsonObj);
	virtual void LoadFromJson(const Json &jsonObj);

	void SetEnabled(const bool on) { m_enabled = on; }
	void Update(const float coolDown, const float shieldStrength);
	void SetColor(const Color3ub &);
	void AddHit(const vector3d &hitPos);

	static void Init();
	static void Uninit();

	SceneGraph::StaticGeometry *GetFirstShieldMesh();

protected:
	struct Hits {
		Hits(const vector3d &_pos, const uint32_t _start, const uint32_t _end);
		vector3d pos;
		uint32_t start;
		uint32_t end;
	};
	typedef std::deque<Shields::Hits>::iterator HitIterator;
	std::deque<Hits> m_hits;
	std::vector<Shield> m_shields;
	bool m_enabled;

	static bool s_initialised;
};

#endif
