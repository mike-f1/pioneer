// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "LOD.h"

#include "BaseLoader.h"
#include "NodeCopyCache.h"
#include "NodeVisitor.h"
#include "Serializer.h"
#include "graphics/Graphics.h"
#include "graphics/RendererLocator.h"
#include "graphics/VertexBuffer.h"
#include "libs/StringF.h"

#include "profiler/Profiler.h"

namespace SceneGraph {

	LOD::LOD() :
		Group()
	{}

	LOD::LOD(const LOD &lod, NodeCopyCache *cache) :
		Group(lod, cache),
		m_pixelSizes(lod.m_pixelSizes)
	{}

	Node *LOD::Clone(NodeCopyCache *cache)
	{
		return cache->Copy<LOD>(this);
	}

	void LOD::Accept(NodeVisitor &nv)
	{
		nv.ApplyLOD(*this);
	}

	void LOD::AddLevel(float pixelSize, Node *nod)
	{
		m_pixelSizes.push_back(pixelSize);
		if (nod->GetName().empty()) {
			nod->SetName(stringf("%0{f.0}", pixelSize));
		}
		AddChild(nod);
	}

	void LOD::Render(const matrix4x4f &trans, const RenderData *rd)
	{
		PROFILE_SCOPED()
		if (m_pixelSizes.empty() || !RendererLocator::getRenderer())
			return;

		//figure out approximate pixel size of object's bounding radius
		//on screen and pick a child to render
		const vector3f cameraPos(-trans[12], -trans[13], -trans[14]);
		//fov is vertical, so using screen height
		const float pixrad = Graphics::GetScreenHeight() * rd->boundingRadius / (cameraPos.Length() * Graphics::GetFovFactor());
		unsigned int lod = m_children.size() - 1;
		for (unsigned int i = m_pixelSizes.size(); i > 0; i--) {
			if (pixrad < m_pixelSizes[i - 1]) lod = i - 1;
		}
		m_children[lod]->Render(trans, rd);
	}

	void LOD::Render(const std::vector<matrix4x4f> &trans, const RenderData *rd)
	{
		PROFILE_SCOPED()
		// anything to draw?
		if (m_pixelSizes.empty() || !RendererLocator::getRenderer())
			return;

		// got something to draw with
		const size_t pixel_size = m_pixelSizes.size();
		const size_t tsize = trans.size();

		// transformation buffers
		std::vector<std::vector<matrix4x4f>> transform;
		transform.resize(pixel_size);
		for (uint32_t i = 0; i < pixel_size; i++) {
			transform[i].reserve(tsize);
		}

		// separate out the transformations basing on which lod they need
		for (const auto &mt : trans) {
			//figure out approximate pixel size of object's bounding radius
			//on screen and pick a child to render
			const vector3f cameraPos(-mt[12], -mt[13], -mt[14]);
			//fov is vertical, so using screen height
			const float pixrad = Graphics::GetScreenHeight() * rd->boundingRadius / (cameraPos.Length() * Graphics::GetFovFactor());
			unsigned int lod = m_children.size() - 1;
			for (unsigned int i = pixel_size; i > 0; i--) {
				if (pixrad < m_pixelSizes[i - 1]) {
					lod = i - 1;
				}
			}

			transform[lod].push_back(mt);
		}

		// now render each of the buffers for each of the lods
		for (uint32_t inst = 0; inst < transform.size(); inst++) {
			if (!transform[inst].empty()) {
				m_children[inst]->Render(transform[inst], rd);
			}
		}
	}

	void LOD::Save(NodeDatabase &db)
	{
		Group::Save(db);
		//same number as children
		db.wr->Int32(m_pixelSizes.size());
		for (auto i : m_pixelSizes)
			db.wr->Int32(i);
	}

	LOD *LOD::Load(NodeDatabase &db)
	{
		LOD *lod = new LOD();
		const uint32_t numLevels = db.rd->Int32();
		for (uint32_t i = 0; i < numLevels; i++)
			lod->m_pixelSizes.push_back(db.rd->Int32());
		return lod;
	}

} // namespace SceneGraph
