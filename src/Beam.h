// Copyright © 2008-2016 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#pragma once

#ifndef _BEAM_H
#define _BEAM_H

#include "Body.h"
#include "Color.h"
#include "Object.h"
#include "libs/matrix4x4.h"
#include "libs/vector3.h"

class Camera;
class Space;

namespace Graphics {
	class Material;
	class RenderState;
	class VertexArray;
} // namespace Graphics

struct ProjectileData;

class Beam final: public Body {
public:
	OBJDEF(Beam, Body, PROJECTILE);

	static void Add(Body *parent, const ProjectileData &prData, const vector3d &pos, const vector3d &baseVel, const vector3d &dir);

	Beam() = delete;
	Beam(Body *parent, const ProjectileData &prData, const vector3d &pos, const vector3d &baseVel, const vector3d &dir);
	Beam(const Json &jsonObj, Space *space);
	virtual ~Beam();

	Json SaveToJson(Space *space) override;

	void Render(const Camera *camera, const vector3d &viewCoords, const matrix4x4d &viewTransform) override;
	void TimeStepUpdate(const float timeStep) override;
	void StaticUpdate(const float timeStep) override;
	void NotifyRemoved(const Body *const removedBody) override;
	void PostLoadFixup(Space *space) override;
	void UpdateInterpTransform(double alpha) override;

	static void FreeModel();

protected:

private:
	float GetDamage() const;
	double GetRadius() const;
	Body *m_parent;
	vector3d m_baseVel;
	vector3d m_dir;
	Color m_color;
	float m_baseDam;
	float m_length;
	float m_age;
	bool m_mining;
	bool m_active;

	int m_parentIndex; // deserialisation

	static void BuildModel();

	static std::unique_ptr<Graphics::VertexArray> s_sideVerts;
	static std::unique_ptr<Graphics::VertexArray> s_glowVerts;
	static std::unique_ptr<Graphics::Material> s_sideMat;
	static std::unique_ptr<Graphics::Material> s_glowMat;
	static Graphics::RenderState *s_renderState;
};

#endif /* _BEAM_H */
