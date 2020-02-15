// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "BaseSphere.h"

#include "GasGiant.h"
#include "GeoSphere.h"

#include "graphics/Drawables.h"
#include "graphics/Renderer.h"
#include "graphics/RendererLocator.h"
#include "graphics/VertexArray.h"
#include "graphics/VertexBuffer.h"

#include "terrain/Terrain.h"

BaseSphere::BaseSphere(const SystemBody *body) :
	SystemBodyWrapper(body),
	m_terrain(Terrain::InstanceTerrain(body)) {}

BaseSphere::~BaseSphere() {}

//static
void BaseSphere::Init(int detail)
{
	GeoSphere::Init(detail);
	GasGiant::Init(detail);
}

//static
void BaseSphere::Uninit()
{
	GeoSphere::Uninit();
	GasGiant::Uninit();
}

//static
void BaseSphere::UpdateAllBaseSphereDerivatives()
{
	GeoSphere::UpdateAllGeoSpheres();
	GasGiant::UpdateAllGasGiants();
}

//static
void BaseSphere::OnChangeDetailLevel(int new_detail)
{
	GeoSphere::OnChangeDetailLevel(new_detail);
	GasGiant::OnChangeDetailLevel(new_detail);
}

void BaseSphere::DrawAtmosphereSurface(const matrix4x4d &modelView, const vector3d &campos, float rad,
	Graphics::RenderState *rs, RefCountedPtr<Graphics::Material> mat)
{
	PROFILE_SCOPED()
	using namespace Graphics;
	const vector3d yaxis = campos.Normalized();
	const vector3d zaxis = vector3d(1.0, 0.0, 0.0).Cross(yaxis).Normalized();
	const vector3d xaxis = yaxis.Cross(zaxis);
	const matrix4x4d invrot = matrix4x4d::MakeRotMatrix(xaxis, yaxis, zaxis).Inverse();

	RendererLocator::getRenderer()->SetTransform(modelView * matrix4x4d::ScaleMatrix(rad) * invrot);

	if (!m_atmos)
		m_atmos.reset(new Drawables::Sphere3D(RendererLocator::getRenderer(), mat, rs, 4, 1.0f, ATTRIB_POSITION));
	m_atmos->Draw(RendererLocator::getRenderer());

	RendererLocator::getRenderer()->GetStats().AddToStatCount(Graphics::Stats::STAT_ATMOSPHERES, 1);
}
