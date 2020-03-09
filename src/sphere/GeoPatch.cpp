// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "GeoPatch.h"

#include "GeoPatchContext.h"
#include "GeoPatchJobs.h"
#include "GeoSphere.h"
#include "Pi.h"
#include "graphics/Drawables.h"
#include "graphics/Frustum.h"
#include "graphics/Material.h"
#include "graphics/Renderer.h"
#include "graphics/RendererLocator.h"
#include "graphics/RenderState.h"
#include "graphics/TextureBuilder.h"
#include "graphics/VertexBuffer.h"
#include "libs/Sphere.h"
#include "perlin.h"
#include <algorithm>
#include <deque>

#ifdef DEBUG_BOUNDING_SPHERES
#include "graphics/RenderState.h"
#endif

// tri edge lengths
static const double GEOPATCH_SUBDIVIDE_AT_CAMDIST = 5.0;

GeoPatch::GeoPatch(const RefCountedPtr<GeoPatchContext> &ctx_, GeoSphere *gs,
	const vector3d &v0_, const vector3d &v1_, const vector3d &v2_, const vector3d &v3_,
	const int depth, const GeoPatchID &ID_) :
	m_ctx(ctx_),
	m_v0(v0_),
	m_v1(v1_),
	m_v2(v2_),
	m_v3(v3_),
	m_parent(nullptr),
	m_geosphere(gs),
	m_depth(depth),
	m_needUpdateVBOs(false),
	m_PatchID(ID_),
	m_HasJobRequest(false)
{

	m_clipCentroid = (m_v0 + m_v1 + m_v2 + m_v3) * 0.25;
	m_centroid = m_clipCentroid.Normalized();
	m_clipRadius = 0.0;
	m_clipRadius = std::max(m_clipRadius, (m_v0 - m_clipCentroid).Length());
	m_clipRadius = std::max(m_clipRadius, (m_v1 - m_clipCentroid).Length());
	m_clipRadius = std::max(m_clipRadius, (m_v2 - m_clipCentroid).Length());
	m_clipRadius = std::max(m_clipRadius, (m_v3 - m_clipCentroid).Length());
	double distMult;
	if (m_geosphere->GetSystemBodyType() < GalaxyEnums::BodyType::TYPE_PLANET_ASTEROID) {
		distMult = 10.0 / Clamp(m_depth, 1, 10);
	} else {
		distMult = 5.0 / Clamp(m_depth, 1, 5);
	}
	m_roughLength = GEOPATCH_SUBDIVIDE_AT_CAMDIST / pow(2.0, m_depth) * distMult;
	m_needUpdateVBOs = false;
}

GeoPatch::~GeoPatch()
{
	m_HasJobRequest = false;
	for (int i = 0; i < NUM_KIDS; i++) {
		m_kids[i].reset();
	}
}

void GeoPatch::UpdateVBOs()
{
	PROFILE_SCOPED()
	assert(RendererLocator::getRenderer());
	m_needUpdateVBOs = false;

	//create buffer and upload data
	Graphics::VertexBufferDesc vbd;
	vbd.attrib[0].semantic = Graphics::ATTRIB_POSITION;
	vbd.attrib[0].format = Graphics::ATTRIB_FORMAT_FLOAT3;
	vbd.attrib[1].semantic = Graphics::ATTRIB_NORMAL;
	vbd.attrib[1].format = Graphics::ATTRIB_FORMAT_FLOAT3;
	vbd.attrib[2].semantic = Graphics::ATTRIB_DIFFUSE;
	vbd.attrib[2].format = Graphics::ATTRIB_FORMAT_UBYTE4;
	vbd.attrib[3].semantic = Graphics::ATTRIB_UV0;
	vbd.attrib[3].format = Graphics::ATTRIB_FORMAT_FLOAT2;
	vbd.numVertices = m_ctx->NUMVERTICES();
	vbd.usage = Graphics::BUFFER_USAGE_STATIC;
	m_vertexBuffer.reset(RendererLocator::getRenderer()->CreateVertexBuffer(vbd));

	GeoPatchContext::VBOVertex *VBOVtxPtr = m_vertexBuffer->Map<GeoPatchContext::VBOVertex>(Graphics::BUFFER_MAP_WRITE);
	assert(m_vertexBuffer->GetDesc().stride == sizeof(GeoPatchContext::VBOVertex));

	const int32_t edgeLen = m_ctx->GetEdgeLen();
	const double frac = m_ctx->GetFrac();
	const double *pHts = m_heights.data();
	const vector3f *pNorm = m_normals.data();
	const Color3ub *pColr = m_colors.data();

	double minh = DBL_MAX;

	#ifdef DEBUG_BOUNDING_SPHERES
	Uint8 r = Uint8(m_rand.Int32(0, 255));
	Uint8 g = Uint8(m_rand.Int32(0, 255));
	Uint8 b = Uint8(m_rand.Int32(0, 255));
	#endif // DEBUG_BOUNDING_SPHERES
	// ----------------------------------------------------
	// inner loops
	for (int32_t y = 1; y < edgeLen - 1; y++) {
		for (int32_t x = 1; x < edgeLen - 1; x++) {
			const double height = *pHts;
			minh = std::min(height, minh);
			const double xFrac = double(x - 1) * frac;
			const double yFrac = double(y - 1) * frac;
			const vector3d p((GetSpherePoint(xFrac, yFrac) * (height + 1.0)) - m_clipCentroid);
			m_clipRadius = std::max(m_clipRadius, p.Length());

			GeoPatchContext::VBOVertex *vtxPtr = &VBOVtxPtr[x + (y * edgeLen)];
			vtxPtr->pos = vector3f(p);
			++pHts; // next height

			const vector3f norma(pNorm->Normalized());
			vtxPtr->norm = norma;
			++pNorm; // next normal

			#ifndef DEBUG_BOUNDING_SPHERES
			vtxPtr->col[0] = pColr->r;
			vtxPtr->col[1] = pColr->g;
			vtxPtr->col[2] = pColr->b;
			vtxPtr->col[3] = 255;
			++pColr; // next colour
			#else
			vtxPtr->col[0] = r;
			vtxPtr->col[1] = g;
			vtxPtr->col[2] = b;
			vtxPtr->col[3] = 255;
			++pColr;
			#endif // DEBUG_BOUNDING_SPHERES

			// uv coords
			vtxPtr->uv.x = 1.0f - xFrac;
			vtxPtr->uv.y = yFrac;

			++vtxPtr; // next vertex
		}
	}
	const double minhScale = (minh + 1.0) * 0.999995;
	// ----------------------------------------------------
	const int32_t innerLeft = 1;
	const int32_t innerRight = edgeLen - 2;
	const int32_t outerLeft = 0;
	const int32_t outerRight = edgeLen - 1;
	// vertical edges
	// left-edge
	for (int32_t y = 1; y < edgeLen - 1; y++) {
		const int32_t x = innerLeft - 1;
		const double xFrac = double(x - 1) * frac;
		const double yFrac = double(y - 1) * frac;
		const vector3d p((GetSpherePoint(xFrac, yFrac) * minhScale) - m_clipCentroid);

		GeoPatchContext::VBOVertex *vtxPtr = &VBOVtxPtr[outerLeft + (y * edgeLen)];
		GeoPatchContext::VBOVertex *vtxInr = &VBOVtxPtr[innerLeft + (y * edgeLen)];
		vtxPtr->pos = vector3f(p);
		vtxPtr->norm = vtxInr->norm;
		vtxPtr->col = vtxInr->col;
		vtxPtr->uv = vtxInr->uv;
	}
	// right-edge
	for (int32_t y = 1; y < edgeLen - 1; y++) {
		const int32_t x = innerRight + 1;
		const double xFrac = double(x - 1) * frac;
		const double yFrac = double(y - 1) * frac;
		const vector3d p((GetSpherePoint(xFrac, yFrac) * minhScale) - m_clipCentroid);

		GeoPatchContext::VBOVertex *vtxPtr = &VBOVtxPtr[outerRight + (y * edgeLen)];
		GeoPatchContext::VBOVertex *vtxInr = &VBOVtxPtr[innerRight + (y * edgeLen)];
		vtxPtr->pos = vector3f(p);
		vtxPtr->norm = vtxInr->norm;
		vtxPtr->col = vtxInr->col;
		vtxPtr->uv = vtxInr->uv;
	}
	// ----------------------------------------------------
	const int innerTop = 1;
	const int innerBottom = edgeLen - 2;
	const int outerTop = 0;
	const int outerBottom = edgeLen - 1;
	// horizontal edges
	// top-edge
	for (int x = 1; x < edgeLen - 1; x++) {
		const int y = innerTop - 1;
		const double xFrac = double(x - 1) * frac;
		const double yFrac = double(y - 1) * frac;
		const vector3d p((GetSpherePoint(xFrac, yFrac) * minhScale) - m_clipCentroid);

		GeoPatchContext::VBOVertex *vtxPtr = &VBOVtxPtr[x + (outerTop * edgeLen)];
		GeoPatchContext::VBOVertex *vtxInr = &VBOVtxPtr[x + (innerTop * edgeLen)];
		vtxPtr->pos = vector3f(p);
		vtxPtr->norm = vtxInr->norm;
		vtxPtr->col = vtxInr->col;
		vtxPtr->uv = vtxInr->uv;
	}
	// bottom-edge
	for (int x = 1; x < edgeLen - 1; x++) {
		const int y = innerBottom + 1;
		const double xFrac = double(x - 1) * frac;
		const double yFrac = double(y - 1) * frac;
		const vector3d p((GetSpherePoint(xFrac, yFrac) * minhScale) - m_clipCentroid);

		GeoPatchContext::VBOVertex *vtxPtr = &VBOVtxPtr[x + (outerBottom * edgeLen)];
		GeoPatchContext::VBOVertex *vtxInr = &VBOVtxPtr[x + (innerBottom * edgeLen)];
		vtxPtr->pos = vector3f(p);
		vtxPtr->norm = vtxInr->norm;
		vtxPtr->col = vtxInr->col;
		vtxPtr->uv = vtxInr->uv;
	}
	// ----------------------------------------------------
	// corners
	{
		// top left
		GeoPatchContext::VBOVertex *tarPtr = &VBOVtxPtr[0];
		GeoPatchContext::VBOVertex *srcPtr = &VBOVtxPtr[1];
		(*tarPtr) = (*srcPtr);
	}
	{
		// top right
		GeoPatchContext::VBOVertex *tarPtr = &VBOVtxPtr[(edgeLen - 1)];
		GeoPatchContext::VBOVertex *srcPtr = &VBOVtxPtr[(edgeLen - 2)];
		(*tarPtr) = (*srcPtr);
	}
	{
		// bottom left
		GeoPatchContext::VBOVertex *tarPtr = &VBOVtxPtr[(edgeLen - 1) * edgeLen];
		GeoPatchContext::VBOVertex *srcPtr = &VBOVtxPtr[(edgeLen - 2) * edgeLen];
		(*tarPtr) = (*srcPtr);
	}
	{
		// bottom right
		GeoPatchContext::VBOVertex *tarPtr = &VBOVtxPtr[(edgeLen - 1) + ((edgeLen - 1) * edgeLen)];
		GeoPatchContext::VBOVertex *srcPtr = &VBOVtxPtr[(edgeLen - 1) + ((edgeLen - 2) * edgeLen)];
		(*tarPtr) = (*srcPtr);
	}

	// ----------------------------------------------------
	// end of mapping
	m_vertexBuffer->Unmap();

	// Don't need this anymore so throw it away
	m_normals.resize(0);
	m_colors.resize(0);

	RefCountedPtr<Graphics::Material> mat(RendererLocator::getRenderer()->CreateMaterial(Graphics::MaterialDescriptor()));
	switch (m_depth) {
		case GEOPATCH_MAX_DEPTH - 0: mat->diffuse = Color::WHITE; break;
		case GEOPATCH_MAX_DEPTH - 1: mat->diffuse = Color::RED; break;
		case GEOPATCH_MAX_DEPTH - 2: mat->diffuse = Color::GREEN; break;
		case GEOPATCH_MAX_DEPTH - 3: mat->diffuse = Color::BLUE; break;
		case GEOPATCH_MAX_DEPTH - 4: mat->diffuse = Color(255, 255, 0); break;
		case GEOPATCH_MAX_DEPTH - 5: mat->diffuse = Color(255, 0, 255); break;
		case GEOPATCH_MAX_DEPTH - 6: mat->diffuse = Color(0, 255, 255); break;
		default: mat->diffuse = Color::BLACK; break;
	}
	m_boundsphere.reset(new Graphics::Drawables::Sphere3D(RendererLocator::getRenderer(), mat, RendererLocator::getRenderer()->CreateRenderState(Graphics::RenderStateDesc()), 4, m_clipRadius));
}

// the default sphere we do the horizon culling against
static const SSphere s_sph;
void GeoPatch::Render(const vector3d &campos, const matrix4x4d &modelView, const Graphics::Frustum &frustum)
{
	PROFILE_SCOPED()
	// must update the VBOs to calculate the clipRadius...
	if (m_needUpdateVBOs) {
		UpdateVBOs();
	}
	// ...before doing the furstum culling that relies on it.
	if (!frustum.TestPoint(m_clipCentroid, m_clipRadius))
		return; // nothing below this patch is visible

	// only want to horizon cull patches that can actually be over the horizon!
	const vector3d camDir(campos - m_clipCentroid);
	const vector3d camDirNorm(camDir.Normalized());
	const vector3d cenDir(m_clipCentroid.Normalized());
	const double dotProd = camDirNorm.Dot(cenDir);

	if (dotProd < 0.25 && (camDir.LengthSqr() > (m_clipRadius * m_clipRadius))) {
		SSphere obj;
		obj.m_centre = m_clipCentroid;
		obj.m_radius = m_clipRadius;

		if (!s_sph.HorizonCulling(campos, obj)) {
			return; // nothing below this patch is visible
		}
	}

	if (m_kids[0]) {
		for (int i = 0; i < NUM_KIDS; i++)
			m_kids[i]->Render(campos, modelView, frustum);
	} else if (!m_heights.empty()) {
		RefCountedPtr<Graphics::Material> mat = m_geosphere->GetSurfaceMaterial();
		Graphics::RenderState *rs = m_geosphere->GetSurfRenderState();

		const vector3d relpos = m_clipCentroid - campos;
		RendererLocator::getRenderer()->SetTransform(modelView * matrix4x4d::Translation(relpos));

		RendererLocator::getRenderer()->GetStats().AddToStatCount(Graphics::Stats::STAT_PATCHES_TRIS, m_ctx->GetNumTris());

		// per-patch detail texture scaling value
		m_geosphere->GetMaterialParameters().patchDepth = m_depth;

		RendererLocator::getRenderer()->DrawBufferIndexed(m_vertexBuffer.get(), m_ctx->GetIndexBuffer(), rs, mat.Get());
#ifdef DEBUG_BOUNDING_SPHERES
		if (m_boundsphere.get()) {
			RendererLocator::getRenderer()->SetWireFrameMode(true);
			m_boundsphere->Draw();
			RendererLocator::getRenderer()->SetWireFrameMode(false);
		}
#endif
	}
}

void GeoPatch::LODUpdate(const vector3d &campos, const Graphics::Frustum &frustum)
{
	// there should be no LOD update when we have active split requests
	if (m_HasJobRequest)
		return;

	bool canSplit = true;
	bool canMerge = bool(m_kids[0]);

	// always split at first level
	double centroidDist = DBL_MAX;
	if (m_parent) {
		centroidDist = (campos - m_centroid).Length();
		const bool errorSplit = (centroidDist < m_roughLength);
		if (!(canSplit && (m_depth < std::min(GEOPATCH_MAX_DEPTH, m_geosphere->GetMaxDepth())) && errorSplit)) {
			canSplit = false;
		}
	}

	if (canSplit) {
		if (!m_kids[0]) {
			// Test if this patch is visible
			if (!frustum.TestPoint(m_clipCentroid, m_clipRadius))
				return; // nothing below this patch is visible

			// only want to horizon cull patches that can actually be over the horizon!
			const vector3d camDir(campos - m_clipCentroid);
			const vector3d camDirNorm(camDir.Normalized());
			const vector3d cenDir(m_clipCentroid.Normalized());
			const double dotProd = camDirNorm.Dot(cenDir);

			if (dotProd < 0.25 && (camDir.LengthSqr() > (m_clipRadius * m_clipRadius))) {
				SSphere obj;
				obj.m_centre = m_clipCentroid;
				obj.m_radius = m_clipRadius;

				if (!s_sph.HorizonCulling(campos, obj)) {
					return; // nothing below this patch is visible
				}
			}

			// we can see this patch so submit the jobs!
			assert(!m_HasJobRequest);
			m_HasJobRequest = true;

			SQuadSplitRequest *ssrd = new SQuadSplitRequest(m_v0, m_v1, m_v2, m_v3, m_centroid.Normalized(), m_depth,
				m_geosphere->GetSystemBodyPath(), m_PatchID, m_ctx->GetEdgeLen() - 2,
				m_ctx->GetFrac(), m_geosphere->GetTerrain());

			// add to the GeoSphere to be processed at end of all LODUpdate requests
			m_geosphere->AddQuadSplitRequest(centroidDist, ssrd, this);
		} else {
			for (int i = 0; i < NUM_KIDS; i++) {
				m_kids[i]->LODUpdate(campos, frustum);
			}
		}
	} else if (canMerge) {
		for (int i = 0; i < NUM_KIDS; i++) {
			canMerge &= m_kids[i]->canBeMerged();
		}
		if (canMerge) {
			for (int i = 0; i < NUM_KIDS; i++) {
				m_kids[i].reset();
			}
		}
	}
}

void GeoPatch::RequestSinglePatch()
{
	if (m_heights.empty()) {
		assert(!m_HasJobRequest);
		m_HasJobRequest = true;
		SSingleSplitRequest *ssrd = new SSingleSplitRequest(m_v0, m_v1, m_v2, m_v3, m_centroid.Normalized(), m_depth,
			m_geosphere->GetSystemBodyPath(), m_PatchID, m_ctx->GetEdgeLen() - 2, m_ctx->GetFrac(), m_geosphere->GetTerrain());
		m_job = Pi::GetAsyncJobQueue()->Queue(new SinglePatchJob(ssrd));
	}
}

void GeoPatch::ReceiveHeightmaps(SQuadSplitResult *psr)
{
	PROFILE_SCOPED()
	assert(NULL != psr);
	if (m_depth < psr->depth()) {
		// this should work because each depth should have a common history
		const uint32_t kidIdx = psr->data(0).patchID.GetPatchIdx(m_depth + 1);
		if (m_kids[kidIdx]) {
			m_kids[kidIdx]->ReceiveHeightmaps(psr);
		} else {
			psr->OnCancel();
		}
	} else {
		assert(m_HasJobRequest);
		const int newDepth = m_depth + 1;
		for (int i = 0; i < NUM_KIDS; i++) {
			assert(!m_kids[i]);
			const SQuadSplitResult::SSplitResultData &data = psr->data(i);
			assert(i == data.patchID.GetPatchIdx(newDepth));
			assert(0 == data.patchID.GetPatchIdx(newDepth + 1));
			m_kids[i].reset(new GeoPatch(m_ctx, m_geosphere,
				data.v0, data.v1, data.v2, data.v3,
				newDepth, data.patchID));
		}
		m_kids[0]->m_parent = m_kids[1]->m_parent = m_kids[2]->m_parent = m_kids[3]->m_parent = this;

		for (int i = 0; i < NUM_KIDS; i++) {
			const SQuadSplitResult::SSplitResultData &data = psr->data(i);
			m_kids[i]->m_heights = std::move(data.m_heights);
			m_kids[i]->m_normals = std::move(data.m_normals);
			m_kids[i]->m_colors = std::move(data.m_colors);
		}
		for (int i = 0; i < NUM_KIDS; i++) {
			m_kids[i]->NeedToUpdateVBOs();
		}
		m_HasJobRequest = false;
	}
}

void GeoPatch::ReceiveHeightmap(const SSingleSplitResult *psr)
{
	PROFILE_SCOPED()
	assert(nullptr == m_parent);
	assert(nullptr != psr);
	assert(m_HasJobRequest);
	{
		const SSingleSplitResult::SSplitResultData &data = psr->data();
		m_heights = std::move(data.m_heights);
		m_normals = std::move(data.m_normals);
		m_colors = std::move(data.m_colors);
	}
	m_HasJobRequest = false;
}

void GeoPatch::ReceiveJobHandle(Job::Handle job)
{
	assert(!m_job.HasJob());
	m_job = static_cast<Job::Handle &&>(job);
}
