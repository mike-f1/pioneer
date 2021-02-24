// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _MATRIXTRANSFORM_H
#define _MATRIXTRANSFORM_H
/*
 * Applies a matrix transform to child nodes
 */
#include "Group.h"
#include "libs/matrix4x4.h"

namespace Graphics {
	class Renderer;
}

namespace SceneGraph {
	class MatrixTransform final : public Group {
	public:
		MatrixTransform(const matrix4x4f &m);
		MatrixTransform(const MatrixTransform &, NodeCopyCache *cache = 0);

		Node *Clone(NodeCopyCache *cache = 0) override;
		const char *GetTypeName() const override { return "MatrixTransform"; }
		void Accept(NodeVisitor &v) override;

		void Save(NodeDatabase &) override;
		static MatrixTransform *Load(NodeDatabase &);

		void Render(const matrix4x4f &trans, const RenderData *rd) override;
		void Render(const std::vector<matrix4x4f> &trans, const RenderData *rd) override;

		const matrix4x4f &GetTransform() const { return m_transform; }
		void SetTransform(const matrix4x4f &m) { m_transform = m; }

		bool GetIsAnimated() { return m_is_animated; }
		void SetAnimated() { m_is_animated = true; }
	protected:
		virtual ~MatrixTransform() {}

	private:
		matrix4x4f m_transform;
		bool m_is_animated; // <- set when animations are "reconnected" or initialized, _not when copied_
	};
} // namespace SceneGraph
#endif
