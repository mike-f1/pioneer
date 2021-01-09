// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _PROJECTILE_H
#define _PROJECTILE_H

#include "Body.h"
#include "Color.h"

struct ProjectileData;

class Frame;

namespace Graphics {
	class Material;
	class Renderer;
	class RenderState;
	class VertexArray;
} // namespace Graphics

class Projectile final: public Body {
public:
	OBJDEF(Projectile, Body, PROJECTILE);

	static void Add(Body *parent, const ProjectileData &prData, const vector3d &pos, const vector3d &baseVel, const vector3d &dirVel);

	Projectile() = delete;
	Projectile(Body *parent, const ProjectileData &prData, const vector3d &pos, const vector3d &baseVel, const vector3d &dirVel);
	Projectile(const Json &jsonObj, Space *space);
	virtual ~Projectile();

	Json SaveToJson(Space *space) const override;

	void Render(const Camera *camera, const vector3d &viewCoords, const matrix4x4d &viewTransform) override;
	void TimeStepUpdate(const float timeStep) override;
	void StaticUpdate(const float timeStep) override;
	void NotifyRemoved(const Body *const removedBody) override;
	void UpdateInterpTransform(double alpha) override;
	void PostLoadFixup(Space *space) override;

	static void FreeModel();

protected:

private:
	float GetDamage() const;
	double GetRadius() const;
	Body *m_parent;
	vector3d m_baseVel;
	vector3d m_dirVel;
	float m_age;
	float m_lifespan;
	float m_baseDam;
	float m_length;
	float m_width;
	bool m_mining;
	Color m_color;

	int m_parentIndex; // deserialisation

	static void BuildModel();

	static std::unique_ptr<Graphics::VertexArray> s_sideVerts;
	static std::unique_ptr<Graphics::VertexArray> s_glowVerts;
	static std::unique_ptr<Graphics::Material> s_sideMat;
	static std::unique_ptr<Graphics::Material> s_glowMat;
	static Graphics::RenderState *s_renderState;
};

#endif /* _PROJECTILE_H */
