// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "TerrainBody.h"

#include "Frame.h"
#include "GameSaveError.h"
#include "GasGiant.h"
#include "GeoSphere.h"
#include "Json.h"
#include "Space.h"
#include "matrix4x4.h"
#include "collider/Geom.h"
#include "galaxy/SystemBody.h"
#include "graphics/Graphics.h"
#include "graphics/Renderer.h"

TerrainBody::TerrainBody(SystemBody *sbody) :
	Body(),
	m_sbody(sbody),
	m_mass(0),
	m_previous_gt(nullptr),
	m_terrainGeom(nullptr)
{
	InitTerrainBody();
}

TerrainBody::~TerrainBody()
{
	m_baseSphere.reset();
}

void TerrainBody::InitTerrainBody()
{
	assert(m_sbody);
	m_mass = m_sbody->GetMass();
	if (!m_baseSphere) {
		if (SystemBody::SUPERTYPE_GAS_GIANT == m_sbody->GetSuperType()) {
			m_baseSphere.reset(new GasGiant(m_sbody));
		} else {
			m_baseSphere.reset(new GeoSphere(m_sbody));
		}
	}
	m_maxFeatureHeight = (m_baseSphere->GetMaxFeatureHeight() + 1.0) * m_sbody->GetRadius();
}

void TerrainBody::SaveToJson(Json &jsonObj, Space *space)
{
	Body::SaveToJson(jsonObj, space);

	Json terrainBodyObj({}); // Create JSON object to contain terrain body data.

	terrainBodyObj["index_for_system_body"] = space->GetIndexForSystemBody(m_sbody);

	jsonObj["terrain_body"] = terrainBodyObj; // Add terrain body object to supplied object.
}

TerrainBody::TerrainBody(const Json &jsonObj, Space *space) :
	Body(jsonObj, space),
	m_previous_gt(nullptr),
	m_terrainGeom(nullptr)
{

	try {
		Json terrainBodyObj = jsonObj["terrain_body"];

		m_sbody = space->GetSystemBodyByIndex(terrainBodyObj["index_for_system_body"]);
	} catch (Json::type_error &) {
		throw SavedGameCorruptException();
	}

	InitTerrainBody();
}
/*
#include "Player.h"
#include "Pi.h"
#include "graphics/RenderState.h"
#include "MathUtil.h"
*/
void TerrainBody::Render(Graphics::Renderer *renderer, const Camera *camera, const vector3d &viewCoords, const matrix4x4d &viewTransform)
{
	m_renderer = renderer;

	matrix4x4d ftran = viewTransform;
	vector3d fpos = viewCoords;
	double rad = m_sbody->GetRadius();

	float znear, zfar;
	renderer->GetNearFarRange(znear, zfar);

	//stars very far away are downscaled, because they cannot be
	//accurately drawn using actual distances
	int shrink = 0;
	if (m_sbody->GetSuperType() == SystemBody::SUPERTYPE_STAR) {
		double len = fpos.Length();
		double dist_to_horizon;
		for (;;) {
			if (len < rad) // player inside radius case
				break;

			dist_to_horizon = sqrt(len * len - rad * rad);

			if (dist_to_horizon < zfar * 0.5)
				break;

			rad *= 0.25;
			fpos = 0.25 * fpos;
			len *= 0.25;
			++shrink;
		}
	}

	vector3d campos = fpos;
	ftran.ClearToRotOnly();

	campos = campos * ftran;

	campos = campos * (1.0 / rad); // position of camera relative to planet "model"

	std::vector<Camera::Shadow> shadows;
	if (camera) {
		shadows = camera->PrincipalShadows(this, 3);
		std::for_each(begin(shadows), end(shadows), [&ftran](Camera::Shadow &sh)
		{
			sh.centre = ftran * sh.centre;
		});
	}

	ftran.Scale(rad);

	// translation not applied until patch render to fix jitter
	m_baseSphere->Render(renderer, ftran, -campos, m_sbody->GetRadius(), shadows);

	ftran.Translate(campos);
	SubRender(renderer, ftran, campos);

	//clear depth buffer, shrunken objects should not interact with foreground
	if (shrink)
		renderer->ClearDepthBuffer();

	if (m_collisionMeshVB.Valid() && m_enable_debug) {
		renderer->SetWireFrameMode(true);
		Graphics::RenderStateDesc rsd;
		rsd.cullMode = Graphics::CULL_NONE;

		renderer->SetTransform(viewTransform);

		renderer->DrawBuffer(m_collisionMeshVB.Get(), renderer->CreateRenderState(rsd), Graphics::vtxColorMaterial);
		renderer->SetWireFrameMode(false);
	}

}

void TerrainBody::StaticUpdate(const float timeStep)
{
	GeoSphere *geoSphere = static_cast<GeoSphere*>(m_baseSphere.get());
	if (geoSphere != nullptr) {
		matrix4x4d geom_matrix = matrix4x4d::Identity();
		geom_matrix.Scale(m_sbody->GetRadius());
		vector3d pos;
		GeomTree *gt = geoSphere->GetGeomTree(geom_matrix, pos);

		if (gt != nullptr && gt != m_previous_gt) {
			// Remove old (if any), add new:
			printf("TerrainBody::Render : Update StaticGeom\n");
			m_previous_gt = gt;
			printf("*** TerrainBody::StaticUpdate:\n");
			printf("  Radius: %f\n", m_previous_gt->GetRadius());
			const Aabb &aabb = m_previous_gt->GetAabb();
			printf("  Aabb min: "); aabb.min.Print();
			printf("  AAbb max: "); aabb.max.Print();
			if (m_terrainGeom != nullptr) {
				GetFrame()->RemoveStaticGeom(m_terrainGeom);
				delete m_terrainGeom;
			}
            m_terrainGeom = new Geom(gt, matrix4x4d::Identity(), pos, this);
			GetFrame()->AddStaticGeom(m_terrainGeom);

			if (m_enable_debug) RebuildDebugMesh(gt, pos);
		}
	}
}

void TerrainBody::SetFrame(Frame *f)
{
	if (GetFrame()) {
		GetFrame()->SetPlanetGeom(0, 0);
	}
	Body::SetFrame(f);
	if (f) {
		GetFrame()->SetPlanetGeom(0, 0);
	}
}

double TerrainBody::GetTerrainHeight(const vector3d &pos_) const
{
	double radius = m_sbody->GetRadius();
	if (m_baseSphere) {
		return radius * (1.0 + m_baseSphere->GetHeight(pos_));
	} else {
		assert(0);
		return radius;
	}
}

//static
void TerrainBody::OnChangeDetailLevel()
{
	GeoSphere::OnChangeDetailLevel();
	GasGiant::OnChangeDetailLevel();
}

void TerrainBody::RebuildDebugMesh(GeomTree *debug_gt, vector3d position)
{
	if (debug_gt == nullptr || m_renderer == nullptr) return;

	const std::vector<vector3f> vertices = debug_gt->GetVertices();
	const Uint32 *indices = debug_gt->GetIndices();
	const unsigned int *triFlags = debug_gt->GetTriFlags();
	const unsigned int numIndices = debug_gt->GetNumTris() * 3;

	// Scale a little
	float factor = (m_sbody->GetRadius() + 0.1) / m_sbody->GetRadius();
	// Reposition
	vector3f pos = vector3f(position);
	std::transform(begin(vertices), end(vertices), begin(vertices), [&factor, &pos](vector3f v)
	{
		return (v + pos) * factor;
	});

	printf("TerrainBody::RebuildDebugMesh for '%s'\n    Vertices are %i\n    Triangles are %i\n\n",
		m_sbody->GetName().c_str(), debug_gt->GetNumVertices(), debug_gt->GetNumTris());

	Graphics::VertexArray va(Graphics::ATTRIB_POSITION | Graphics::ATTRIB_DIFFUSE, numIndices * 3);
	int trindex = -1;
	for (unsigned int i = 0; i < numIndices; i++) {
		if (i % 3 == 0)
			trindex++;

		va.Add(vertices[indices[i]], Color::WHITE);
	}

	//create buffer and upload data
	Graphics::VertexBufferDesc vbd;
	vbd.attrib[0].semantic = Graphics::ATTRIB_POSITION;
	vbd.attrib[0].format = Graphics::ATTRIB_FORMAT_FLOAT3;
	vbd.attrib[1].semantic = Graphics::ATTRIB_DIFFUSE;
	vbd.attrib[1].format = Graphics::ATTRIB_FORMAT_UBYTE4;
	vbd.numVertices = va.GetNumVerts();
	vbd.usage = Graphics::BUFFER_USAGE_STATIC;
	m_collisionMeshVB.Reset(m_renderer->CreateVertexBuffer(vbd));
	m_collisionMeshVB->Populate(va);
}
