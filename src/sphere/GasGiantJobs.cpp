// Copyright © 2008-2015 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "GasGiantJobs.h"

#include "GasGiant.h"
#include "graphics/Frustum.h"
#include "graphics/Material.h"
#include "graphics/Renderer.h"
#include "graphics/RendererLocator.h"
#include "graphics/Texture.h"
#include "graphics/TextureBuilder.h"
#include "graphics/Types.h"
#include "graphics/VertexArray.h"
#include "graphics/VertexBuffer.h"
#include "perlin.h"
#include <algorithm>
#include <deque>

namespace GasGiantJobs {
	static const vector3d s_patchFaces[NUM_PATCHES][4] = {
		{ p5, p1, p4, p8 }, // +x
		{ p2, p6, p7, p3 }, // -x

		{ p2, p1, p5, p6 }, // +y
		{ p7, p8, p4, p3 }, // -y

		{ p6, p5, p8, p7 }, // +z - NB: these are actually reversed!
		{ p1, p2, p3, p4 } // -z
	};
	const vector3d &GetPatchFaces(const uint32_t patch, const uint32_t face) { return s_patchFaces[patch][face]; }

	STextureFaceRequest::STextureFaceRequest(const vector3d *v_, const SystemPath &sysPath_, const int32_t face_, const int32_t uvDIMs_, Terrain *pTerrain_) :
		corners(v_),
		sysPath(sysPath_),
		face(face_),
		uvDIMs(uvDIMs_),
		pTerrain(pTerrain_)
	{
		colors = new Color[NumTexels()];
	}

	// RUNS IN ANOTHER THREAD!! MUST BE THREAD SAFE!
	// Use only data local to this object
	void STextureFaceRequest::OnRun()
	{
		PROFILE_SCOPED()
		//MsgTimer timey;

		assert(corners != nullptr);
		double fracStep = 1.0 / double(UVDims() - 1);
		for (int32_t v = 0; v < UVDims(); v++) {
			for (int32_t u = 0; u < UVDims(); u++) {
				// where in this row & colum are we now.
				const double ustep = double(u) * fracStep;
				const double vstep = double(v) * fracStep;

				// get point on the surface of the sphere
				const vector3d p = GetSpherePoint(ustep, vstep);
				// get colour using `p`
				const vector3d colour = pTerrain->GetColor(p, 0.0, p);

				// convert to ubyte and store
				Color *col = colors + (u + (v * UVDims()));
				col[0].r = Uint8(colour.x * 255.0);
				col[0].g = Uint8(colour.y * 255.0);
				col[0].b = Uint8(colour.z * 255.0);
				col[0].a = 255;
			}
		}

		//timey.Mark("SingleTextureFaceCPUJob::OnRun");
	}

	// ********************************************************************************
	// Overloaded PureJob class to handle generating the mesh for each patch
	// ********************************************************************************
	SingleTextureFaceJob::~SingleTextureFaceJob()
	{
		PROFILE_SCOPED()
		if (mpResults) {
			mpResults->OnCancel();
			delete mpResults;
			mpResults = nullptr;
		}
	}

	void SingleTextureFaceJob::OnRun() // RUNS IN ANOTHER THREAD!! MUST BE THREAD SAFE!
	{
		PROFILE_SCOPED()
		mData->OnRun();

		// add this patches data
		STextureFaceResult *sr = new STextureFaceResult(mData->Face());
		sr->addResult(mData->Colors(), mData->UVDims());

		// store the result
		mpResults = sr;
	}

	void SingleTextureFaceJob::OnFinish() // runs in primary thread of the context
	{
		PROFILE_SCOPED()
		GasGiant::OnAddTextureFaceResult(mData->SysPath(), mpResults);
		mpResults = nullptr;
	}

	// ********************************************************************************
	GenFaceQuad::GenFaceQuad(const vector2f &size, Graphics::RenderState *state, const uint32_t GGQuality)
	{
		PROFILE_SCOPED()
		assert(state);
		m_renderState = state;

		Graphics::MaterialDescriptor desc;
		desc.effect = Graphics::EffectType::GEN_GASGIANT_TEXTURE;
		desc.quality = GGQuality;
		desc.textures = 3;
		m_material.reset(RendererLocator::getRenderer()->CreateMaterial(desc));

		// setup noise textures
		m_material->texture0 = Graphics::TextureBuilder::Raw("textures/permTexture.png").GetOrCreateTexture(RendererLocator::getRenderer(), "noise");
		m_material->texture1 = Graphics::TextureBuilder::Raw("textures/gradTexture.png").GetOrCreateTexture(RendererLocator::getRenderer(), "noise");

		// pick the correct colour basis texture for the planet
		switch (0x0000FFFF & GGQuality) {
		case Graphics::OGL::GEN_JUPITER_TEXTURE:
			m_material->texture2 = Graphics::TextureBuilder::Raw("textures/gasgiants/jupiterramp.png").GetOrCreateTexture(RendererLocator::getRenderer(), "gasgiant");
			break;
		case Graphics::OGL::GEN_SATURN_TEXTURE:
			m_material->texture2 = Graphics::TextureBuilder::Raw("textures/gasgiants/saturnramp.png").GetOrCreateTexture(RendererLocator::getRenderer(), "gasgiant");
			break;
		case Graphics::OGL::GEN_SATURN2_TEXTURE:
			m_material->texture2 = Graphics::TextureBuilder::Raw("textures/gasgiants/saturn2ramp.png").GetOrCreateTexture(RendererLocator::getRenderer(), "gasgiant");
			break;
		case Graphics::OGL::GEN_NEPTUNE_TEXTURE:
			m_material->texture2 = Graphics::TextureBuilder::Raw("textures/gasgiants/neptuneramp.png").GetOrCreateTexture(RendererLocator::getRenderer(), "gasgiant");
			break;
		case Graphics::OGL::GEN_NEPTUNE2_TEXTURE:
			m_material->texture2 = Graphics::TextureBuilder::Raw("textures/gasgiants/neptune2ramp.png").GetOrCreateTexture(RendererLocator::getRenderer(), "gasgiant");
			break;
		case Graphics::OGL::GEN_URANUS_TEXTURE:
			m_material->texture2 = Graphics::TextureBuilder::Raw("textures/gasgiants/uranusramp.png").GetOrCreateTexture(RendererLocator::getRenderer(), "gasgiant");
			break;
		}

		// these might need to be reversed
		const vector2f &texSize(size);

		Graphics::VertexArray vertices(Graphics::ATTRIB_POSITION | Graphics::ATTRIB_UV0);

		vertices.Add(vector3f(0.0f, 0.0f, 0.0f), vector2f(0.0f, texSize.y));
		vertices.Add(vector3f(0.0f, size.y, 0.0f), vector2f(0.0f, 0.0f));
		vertices.Add(vector3f(size.x, 0.0f, 0.0f), vector2f(texSize.x, texSize.y));
		vertices.Add(vector3f(size.x, size.y, 0.0f), vector2f(texSize.x, 0.0f));

		//Create vtx & index buffers and copy data
		Graphics::VertexBufferDesc vbd;
		vbd.attrib[0].semantic = Graphics::ATTRIB_POSITION;
		vbd.attrib[0].format = Graphics::ATTRIB_FORMAT_FLOAT3;
		vbd.attrib[1].semantic = Graphics::ATTRIB_UV0;
		vbd.attrib[1].format = Graphics::ATTRIB_FORMAT_FLOAT2;
		vbd.numVertices = vertices.GetNumVerts();
		vbd.usage = Graphics::BUFFER_USAGE_STATIC;
		m_vertexBuffer.reset(RendererLocator::getRenderer()->CreateVertexBuffer(vbd));
		m_vertexBuffer->Populate(vertices);
	}

	void GenFaceQuad::Draw()
	{
		PROFILE_SCOPED()
		RendererLocator::getRenderer()->DrawBuffer(m_vertexBuffer.get(), m_renderState, m_material.get(), Graphics::TRIANGLE_STRIP);
	}

	// ********************************************************************************
	SGPUGenRequest::SGPUGenRequest(const SystemPath &sysPath_, const int32_t uvDIMs_, Terrain *pTerrain_, const float planetRadius_, const float hueAdjust_, GenFaceQuad *pQuad_, Graphics::Texture *pTex_) :
		m_texture(pTex_),
		sysPath(sysPath_),
		uvDIMs(uvDIMs_),
		pTerrain(pTerrain_),
		planetRadius(planetRadius_),
		hueAdjust(hueAdjust_),
		pQuad(pQuad_)
	{
		PROFILE_SCOPED()
		assert(m_texture.Valid());
	}

	void SGPUGenRequest::SetupMaterialParams(const int face)
	{
		PROFILE_SCOPED()
		m_specialParams.v = &GetPatchFaces(face, 0);
		m_specialParams.fracStep = 1.0f / float(uvDIMs);
		m_specialParams.planetRadius = planetRadius;
		m_specialParams.time = 0.0f;

		for (unsigned i = 0; i < 3; i++) {
			m_specialParams.frequency[i] = float(pTerrain->GetFracDef(i).frequency);
		}

		m_specialParams.hueAdjust = hueAdjust;

		pQuad->GetMaterial()->specialParameter0 = &m_specialParams;
	}

	// ********************************************************************************
	void SGPUGenResult::addResult(Graphics::Texture *t_, int32_t uvDims_)
	{
		PROFILE_SCOPED()
		mData = SGPUGenData(t_, uvDims_);
	}

	void SGPUGenResult::OnCancel()
	{
		if (mData.texture) {
			mData.texture.Reset();
		}
	}

	// ********************************************************************************
	// Overloaded JobGPU class to handle generating the mesh for each patch
	// ********************************************************************************
	SingleGPUGenJob::SingleGPUGenJob(SGPUGenRequest *data) :
		mData(data),
		mpResults(nullptr)
	{ /* empty */
	}
	SingleGPUGenJob::~SingleGPUGenJob()
	{
		PROFILE_SCOPED()
		if (mpResults) {
			mpResults->OnCancel();
			delete mpResults;
			mpResults = nullptr;
		}
	}

	void SingleGPUGenJob::OnRun() // Runs in the main thread, may trash the GPU state
	{
		PROFILE_SCOPED()
		//MsgTimer timey;

		RendererLocator::getRenderer()->SetViewport(0, 0, mData->UVDims(), mData->UVDims());
		RendererLocator::getRenderer()->SetTransform(matrix4x4f::Identity());

		// enter ortho
		{
			RendererLocator::getRenderer()->SetMatrixMode(Graphics::MatrixMode::PROJECTION);
			RendererLocator::getRenderer()->PushMatrix();
			RendererLocator::getRenderer()->SetOrthographicProjection(0, mData->UVDims(), mData->UVDims(), 0, -1, 1);
			RendererLocator::getRenderer()->SetMatrixMode(Graphics::MatrixMode::MODELVIEW);
			RendererLocator::getRenderer()->PushMatrix();
			RendererLocator::getRenderer()->LoadIdentity();
		}

		GasGiant::BeginRenderTarget();
		for (unsigned iFace = 0; iFace < NUM_PATCHES; iFace++) {
			// render the scene
			GasGiant::SetRenderTargetCubemap(iFace, mData->Texture());
			RendererLocator::getRenderer()->BeginFrame();

			// draw to the texture here
			mData->SetupMaterialParams(iFace);
			mData->Quad()->Draw();

			RendererLocator::getRenderer()->EndFrame();
			GasGiant::SetRenderTargetCubemap(iFace, nullptr);
		}
		GasGiant::EndRenderTarget();

		// leave ortho?
		{
			RendererLocator::getRenderer()->SetMatrixMode(Graphics::MatrixMode::PROJECTION);
			RendererLocator::getRenderer()->PopMatrix();
			RendererLocator::getRenderer()->SetMatrixMode(Graphics::MatrixMode::MODELVIEW);
			RendererLocator::getRenderer()->PopMatrix();
		}

		// add this patches data
		SGPUGenResult *sr = new SGPUGenResult();
		sr->addResult(mData->Texture(), mData->UVDims());

		// store the result
		mpResults = sr;

		//timey.Mark("SingleGPUGenJob::OnRun");
	}

	void SingleGPUGenJob::OnFinish() // runs in primary thread of the context
	{
		PROFILE_SCOPED()
		GasGiant::OnAddGPUGenResult(mData->SysPath(), mpResults);
		mpResults = nullptr;
	}
} // namespace GasGiantJobs
