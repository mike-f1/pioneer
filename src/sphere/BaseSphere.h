// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _BASESPHERE_H
#define _BASESPHERE_H

#include "Camera.h"
#include "BaseSphereDebugFlags.h"
#include "galaxy/AtmosphereParameters.h"
#include "galaxy/SystemBodyWrapper.h"
#include "graphics/Material.h" // <- For some inlined function...
#include "libs/vector3.h"

#include <vector>
#include <memory>

class Terrain;

namespace Graphics {
	class RenderState;
	namespace Drawables {
		class Sphere3D;
	}
} // namespace Graphics

class BaseSphere: public SystemBodyWrapper {
public:
	BaseSphere(const SystemBody *body);
	virtual ~BaseSphere();

	virtual void Update() = 0;
	virtual void Render(const matrix4x4d &modelView, vector3d campos, const float radius, const std::vector<Camera::Shadow> &shadows) = 0;

	virtual double GetHeight(const vector3d &) const { return 0.0; }

	static void Init(int detail);
	static void Uninit();
	static void UpdateAllBaseSphereDerivatives();
	static void OnChangeDetailLevel(int new_detail);

	void DrawAtmosphereSurface(const matrix4x4d &modelView, const vector3d &campos, float rad,
		Graphics::RenderState *rs, RefCountedPtr<Graphics::Material> mat);

	// in sbody radii
	virtual double GetMaxFeatureHeight() const = 0;

	struct MaterialParameters {
		AtmosphereParameters atmosphere;
		std::vector<Camera::Shadow> shadows;
		int32_t patchDepth;
		int32_t maxPatchDepth;
	};

	virtual void Reset() = 0;

	const SystemBody *GetSystemBody() const { return SystemBodyWrapper::GetSystemBody(); }
	Terrain *GetTerrain() const { return m_terrain.Get(); }

	Graphics::RenderState *GetSurfRenderState() const { return m_surfRenderState; }
	RefCountedPtr<Graphics::Material> GetSurfaceMaterial() const { return m_surfaceMaterial; }
	MaterialParameters &GetMaterialParameters() { return m_materialParameters; }

	virtual void SetDebugFlags(GSDebugFlags flags) {}
	virtual GSDebugFlags GetDebugFlags() { return GSDebugFlags::NONE; }

protected:
	// all variables for GetHeight(), GetColor()
	RefCountedPtr<Terrain> m_terrain;

	virtual void SetUpMaterials() = 0;

	Graphics::RenderState *m_surfRenderState;
	Graphics::RenderState *m_atmosRenderState;
	RefCountedPtr<Graphics::Material> m_surfaceMaterial;
	RefCountedPtr<Graphics::Material> m_atmosphereMaterial;

	// atmosphere geometry
	std::unique_ptr<Graphics::Drawables::Sphere3D> m_atmos;

	//special parameters for shaders
	MaterialParameters m_materialParameters;
};

#endif /* _GEOSPHERE_H */
