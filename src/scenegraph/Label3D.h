// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _SCENEGRAPH_LABEL_H
#define _SCENEGRAPH_LABEL_H
/*
 * Text geometry node, mostly for ship labels
 */
#include "Node.h"
#include "text/DistanceFieldFont.h"
#include <memory>
#include <string>

namespace Graphics {
	class Material;
	class Renderer;
	class RenderState;
	class VertexArray;
	class VertexBuffer;
}

namespace SceneGraph {

	class Label3D final: public Node {
	public:
		Label3D(RefCountedPtr<Text::DistanceFieldFont>);
		Label3D(const Label3D &, NodeCopyCache *cache = 0);
		Node *Clone(NodeCopyCache *cache = 0) override;
		const char *GetTypeName() const override { return "Label3D"; }
		void SetText(const std::string &);
		void Render(const matrix4x4f &trans, const RenderData *rd) override;
		void Accept(NodeVisitor &nv) override;

	private:
		RefCountedPtr<Graphics::Material> m_material;
		std::unique_ptr<Graphics::VertexArray> m_geometry;
		std::unique_ptr<Graphics::VertexBuffer> m_vbuffer;
		RefCountedPtr<Text::DistanceFieldFont> m_font;
		Graphics::RenderState *m_renderState;
	};

} // namespace SceneGraph

#endif
