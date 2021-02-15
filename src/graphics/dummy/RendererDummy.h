// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#pragma once

#ifndef _RENDERER_DUMMY_H
#define _RENDERER_DUMMY_H

#include "graphics/Renderer.h"
#include "graphics/dummy/MaterialDummy.h"
#include "graphics/dummy/RenderStateDummy.h"
#include "graphics/dummy/RenderTargetDummy.h"
#include "graphics/dummy/TextureDummy.h"
#include "graphics/dummy/VertexBufferDummy.h"

namespace Graphics {

	class RendererDummy : public Renderer {
	public:
		static void RegisterRenderer();

		RendererDummy() :
			Renderer(0, 0, 0),
			m_identity(matrix4x4f::Identity())
		{}

		const char *GetName() const override final { return "Dummy"; }
		RendererType GetRendererType() const override final { return RENDERER_DUMMY; }
		bool SupportsInstancing() override final { return false; }
		int GetMaximumNumberAASamples() const override final { return 0; }
		bool GetNearFarRange(float &near_, float &far_) const override final { return true; }

		bool BeginFrame() override final { return true; }
		bool EndFrame() override final { return true; }
		bool SwapBuffers() override final { return true; }

		bool SetRenderState(RenderState *) override final { return true; }
		bool SetRenderTarget(RenderTarget *) override final { return true; }

		bool SetDepthRange(double znear, double zfar) override final { return true; }

		bool ClearScreen() override final { return true; }
		bool ClearDepthBuffer() override final { return true; }
		bool SetClearColor(const Color &c) override final { return true; }

		bool SetViewport(int x, int y, int width, int height) override final { return true; }

		bool SetTransform(const matrix4x4d &m) override final { return true; }
		bool SetTransform(const matrix4x4f &m) override final { return true; }
		bool SetPerspectiveProjection(float fov, float aspect, float near_, float far_) override final { return true; }
		bool SetOrthographicProjection(float xmin, float xmax, float ymin, float ymax, float zmin, float zmax) override final { return true; }
		bool SetProjection(const matrix4x4f &m) override final { return true; }

		bool SetWireFrameMode(bool enabled) override final { return true; }

		bool SetLights(uint32_t numlights, const Light *l) override final { return true; }
		uint32_t GetNumLights() const override final { return 1; }
		bool SetAmbientColor(const Color &c) override final { return true; }

		bool SetScissor(bool enabled, const vector2f &pos = vector2f(0.0f), const vector2f &size = vector2f(0.0f)) override final { return true; }

		bool DrawTriangles(const VertexArray *vertices, RenderState *state, Material *material, PrimitiveType type = TRIANGLES) override final { return true; }
		bool DrawPointSprites(const uint32_t count, const vector3f *positions, RenderState *rs, Material *material, float size) override final { return true; }
		bool DrawPointSprites(const uint32_t count, const vector3f *positions, const vector2f *offsets, const float *sizes, RenderState *rs, Material *material) override final { return true; }
		bool DrawBuffer(VertexBuffer *, RenderState *, Material *, PrimitiveType) override final { return true; }
		bool DrawBufferIndexed(VertexBuffer *, IndexBuffer *, RenderState *, Material *, PrimitiveType) override final { return true; }
		bool DrawBufferInstanced(VertexBuffer *, RenderState *, Material *, InstanceBuffer *, PrimitiveType type = TRIANGLES) override final { return true; }
		bool DrawBufferIndexedInstanced(VertexBuffer *, IndexBuffer *, RenderState *, Material *, InstanceBuffer *, PrimitiveType = TRIANGLES) override final { return true; }

		Material *CreateMaterial(const MaterialDescriptor &d) override final { return new Graphics::Dummy::Material(); }
		Texture *CreateTexture(const TextureDescriptor &d) override final { return new Graphics::TextureDummy(d); }
		RenderState *CreateRenderState(const RenderStateDesc &d) override final { return new Graphics::Dummy::RenderState(d); }
		RenderTarget *CreateRenderTarget(const RenderTargetDesc &d) override final { return new Graphics::Dummy::RenderTarget(d); }
		VertexBuffer *CreateVertexBuffer(const VertexBufferDesc &d) override final { return new Graphics::Dummy::VertexBuffer(d); }
		IndexBuffer *CreateIndexBuffer(uint32_t size, BufferUsage bu) override final { return new Graphics::Dummy::IndexBuffer(size, bu); }
		InstanceBuffer *CreateInstanceBuffer(uint32_t size, BufferUsage bu) override final { return new Graphics::Dummy::InstanceBuffer(size, bu); }

		bool ReloadShaders() override final { return true; }

		const matrix4x4f &GetCurrentModelView() const override final { return m_identity; }
		const matrix4x4f &GetCurrentProjection() const override final { return m_identity; }
		void GetCurrentViewport(int32_t *vp) const override final {}

		void SetMatrixMode(MatrixMode mm) override final {}
		void PushMatrix() override final {}
		void PopMatrix() override final {}
		void LoadIdentity() override final {}
		void LoadMatrix(const matrix4x4f &m) override final {}
		void Translate(const float x, const float y, const float z) override final {}
		void Scale(const float x, const float y, const float z) override final {}

	protected:
		void PushState() override final {}
		void PopState() override final {}

	private:
		const matrix4x4f m_identity;
	};

} // namespace Graphics

#endif
