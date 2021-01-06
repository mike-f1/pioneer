// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _SCENEGRAPH_THRUSTER_H
#define _SCENEGRAPH_THRUSTER_H
/*
 * Spaceship thruster
 */
#include "Node.h"

#include "Color.h"
#include "libs/vector3.h"
#include "libs/matrix4x4.h"
#include "libs/RefCounted.h"

namespace Graphics {
	class VertexBuffer;
	class Material;
	class RenderState;
} // namespace Graphics

namespace SceneGraph {

	class Thruster : public Node {
	public:
		Thruster(bool linear, const vector3f &pos, const vector3f &dir);
		Thruster(const Thruster &, NodeCopyCache *cache = 0);
		Node *Clone(NodeCopyCache *cache = 0) override;
		virtual void Accept(NodeVisitor &nv) override;
		virtual const char *GetTypeName() const override { return "Thruster"; }
		virtual void Render(const matrix4x4f &trans, const RenderData *rd) override;
		virtual void Save(NodeDatabase &) override;
		static Thruster *Load(NodeDatabase &);
		void SetColor(const Color c) { currentColor = c; }
		const vector3f &GetDirection() { return m_dir; }

	private:
		static Graphics::VertexBuffer *CreateThrusterGeometry(Graphics::Material *);
		static Graphics::VertexBuffer *CreateGlowGeometry(Graphics::Material *);
		RefCountedPtr<Graphics::Material> m_tMat;
		RefCountedPtr<Graphics::Material> m_glowMat;
		RefCountedPtr<Graphics::VertexBuffer> m_tBuffer;
		RefCountedPtr<Graphics::VertexBuffer> m_glowBuffer;
		Graphics::RenderState *m_renderState;
		bool linearOnly;
		vector3f m_dir;
		vector3f m_pos;
		Color currentColor;
	};

} // namespace SceneGraph

#endif
