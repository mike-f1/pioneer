// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "MatrixTransform.h"

#include "BaseLoader.h"
#include "NodeCopyCache.h"
#include "NodeVisitor.h"
#include "Serializer.h"
#include "graphics/Renderer.h"

#include "profiler/Profiler.h"

namespace SceneGraph {

	MatrixTransform::MatrixTransform(const matrix4x4f &m) :
		Group(),
		m_transform(m),
		m_is_animated(false)
	{}

	MatrixTransform::MatrixTransform(const MatrixTransform &mt, NodeCopyCache *cache) :
		Group(mt, cache),
		m_transform(mt.m_transform),
		m_is_animated(false)
	{}

	Node *MatrixTransform::Clone(NodeCopyCache *cache)
	{
		return cache->Copy<MatrixTransform>(this);
	}

	void MatrixTransform::Accept(NodeVisitor &nv)
	{
		nv.ApplyMatrixTransform(*this);
	}

	void MatrixTransform::Render(const matrix4x4f &trans, const RenderData *rd)
	{
		PROFILE_SCOPED();
		const matrix4x4f t = trans * m_transform;
		RenderChildren(t, rd);
	}

	static const matrix4x4f s_ident(matrix4x4f::Identity());
	void MatrixTransform::Render(const std::vector<matrix4x4f> &trans, const RenderData *rd)
	{
		PROFILE_SCOPED();
		if (0 == memcmp(&m_transform, &s_ident, sizeof(matrix4x4f))) {
			// m_transform is identity so avoid performing all multiplications
			RenderChildren(trans, rd);
		} else {
			// m_transform is valid, modify all positions by it
			const size_t transSize = trans.size();
			std::vector<matrix4x4f> t;
			t.resize(transSize);
			for (size_t tIdx = 0; tIdx < transSize; tIdx++) {
				t[tIdx] = trans[tIdx] * m_transform;
			}
			RenderChildren(t, rd);
		}
	}

	void MatrixTransform::Save(NodeDatabase &db)
	{
		Group::Save(db);
		for (uint32_t i = 0; i < 16; i++)
			db.wr->Float(m_transform[i]);
		db.wr->Bool(m_is_animated);
	}

	MatrixTransform *MatrixTransform::Load(NodeDatabase &db)
	{
		matrix4x4f matrix;
		for (uint32_t i = 0; i < 16; i++)
			matrix[i] = db.rd->Float();
		bool animated = db.rd->Bool();
		MatrixTransform *mt = new MatrixTransform(matrix);
		if (animated) mt->SetAnimated();
		return mt;
	}

} // namespace SceneGraph
