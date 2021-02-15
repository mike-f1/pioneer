// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#pragma once

#ifndef _RENDERER_OGL_H
#define _RENDERER_OGL_H
/*
 * OpenGL 2.X renderer (2.0, GLSL 1.10 at the moment)
 *  - no fixed function support (shaders for everything)
 *  The plan is: make this more like GL3/ES2
 *  - try to stick to bufferobjects
 *  - use glvertexattribpointer instead of glvertexpointer etc
 *  - get rid of built-in glMaterial, glMatrix use
 */
#include "OpenGLLibs.h"
#include "graphics/Renderer.h"
#include <stack>
#include <unordered_map>
#include <SDL_video.h>

namespace Graphics {

	class Texture;
	struct Settings;

	namespace OGL {
		class GasGiantSurfaceMaterial;
		class GeoSphereSkyMaterial;
		class GeoSphereStarMaterial;
		class GeoSphereSurfaceMaterial;
		class GenGasGiantColourMaterial;
		class Material;
		class MultiMaterial;
		class LitMultiMaterial;
		class Program;
		class RenderState;
		class RenderTarget;
		class RingMaterial;
		class FresnelColourMaterial;
		class ShieldMaterial;
		class UIMaterial;
		class BillboardMaterial;
	} // namespace OGL

	class RendererOGL : public Renderer {
	public:
		static void RegisterRenderer();

		RendererOGL(SDL_Window *window, const Settings &vs, SDL_GLContext &glContext);
		~RendererOGL() override final;

		const char *GetName() const override final { return "OpenGL 3.1, with extensions, renderer"; }
		RendererType GetRendererType() const override final { return RENDERER_OPENGL_3x; }

		void WriteRendererInfo(std::ostream &out) const override final;

		void CheckRenderErrors(const char *func = nullptr, const int line = -1) const override final { CheckErrors(func, line); }
		static void CheckErrors(const char *func = nullptr, const int line = -1);

		bool SupportsInstancing() override final { return true; }

		int GetMaximumNumberAASamples() const override final;
		bool GetNearFarRange(float &near_, float &far_) const override final;

		bool BeginFrame() override final;
		bool EndFrame() override final;
		bool SwapBuffers() override final;

		bool SetRenderState(RenderState *) override final;
		bool SetRenderTarget(RenderTarget *) override final;

		bool SetDepthRange(double znear, double zfar) override final;

		bool ClearScreen() override final;
		bool ClearDepthBuffer() override final;
		bool SetClearColor(const Color &c) override final;

		bool SetViewport(int x, int y, int width, int height) override final;

		bool SetTransform(const matrix4x4d &m) override final;
		bool SetTransform(const matrix4x4f &m) override final;
		bool SetPerspectiveProjection(float fov, float aspect, float near_, float far_) override final;
		bool SetOrthographicProjection(float xmin, float xmax, float ymin, float ymax, float zmin, float zmax) override final;
		bool SetProjection(const matrix4x4f &m) override final;

		bool SetWireFrameMode(bool enabled) override final;

		bool SetLights(uint32_t numlights, const Light *l) override final;
		uint32_t GetNumLights() const override final { return m_numLights; }
		bool SetAmbientColor(const Color &c) override final;

		bool SetScissor(bool enabled, const vector2f &pos = vector2f(0.0f), const vector2f &size = vector2f(0.0f)) override final;

		bool DrawTriangles(const VertexArray *vertices, RenderState *state, Material *material, PrimitiveType type = TRIANGLES) override final;
		bool DrawPointSprites(const uint32_t count, const vector3f *positions, RenderState *rs, Material *material, float size) override final;
		bool DrawPointSprites(const uint32_t count, const vector3f *positions, const vector2f *offsets, const float *sizes, RenderState *rs, Material *material) override final;
		bool DrawBuffer(VertexBuffer *, RenderState *, Material *, PrimitiveType) override final;
		bool DrawBufferIndexed(VertexBuffer *, IndexBuffer *, RenderState *, Material *, PrimitiveType) override final;
		bool DrawBufferInstanced(VertexBuffer *, RenderState *, Material *, InstanceBuffer *, PrimitiveType type = TRIANGLES) override final;
		bool DrawBufferIndexedInstanced(VertexBuffer *, IndexBuffer *, RenderState *, Material *, InstanceBuffer *, PrimitiveType = TRIANGLES) override final;

		Material *CreateMaterial(const MaterialDescriptor &descriptor) override final;
		Texture *CreateTexture(const TextureDescriptor &descriptor) override final;
		RenderState *CreateRenderState(const RenderStateDesc &) override final;
		RenderTarget *CreateRenderTarget(const RenderTargetDesc &) override final;
		VertexBuffer *CreateVertexBuffer(const VertexBufferDesc &) override final;
		IndexBuffer *CreateIndexBuffer(uint32_t size, BufferUsage) override final;
		InstanceBuffer *CreateInstanceBuffer(uint32_t size, BufferUsage) override final;

		bool ReloadShaders() override final;

		const matrix4x4f &GetCurrentModelView() const override final { return m_modelViewStack.top(); }
		const matrix4x4f &GetCurrentProjection() const override final { return m_projectionStack.top(); }
		void GetCurrentViewport(int32_t *vp) const override final
		{
			const Viewport &cur = m_viewportStack.top();
			vp[0] = cur.x;
			vp[1] = cur.y;
			vp[2] = cur.w;
			vp[3] = cur.h;
		}

		void SetMatrixMode(MatrixMode mm) override final;
		void PushMatrix() override final;
		void PopMatrix() override final;
		void LoadIdentity() override final;
		void LoadMatrix(const matrix4x4f &m) override final;
		void Translate(const float x, const float y, const float z) override final;
		void Scale(const float x, const float y, const float z) override final;

		bool Screendump(ScreendumpState &sd) override final;

	protected:
		void PushState() override final;
		void PopState() override final;

		uint32_t m_numLights;
		uint32_t m_numDirLights;
		std::vector<GLuint> m_vertexAttribsSet;
		float m_minZNear;
		float m_maxZFar;
		bool m_useCompressedTextures;
		bool m_useAnisotropicFiltering;

		void SetMaterialShaderTransforms(Material *);

		matrix4x4f &GetCurrentTransform() { return m_currentTransform; }
		matrix4x4f m_currentTransform;

		OGL::Program *GetOrCreateProgram(OGL::Material *);
		friend class OGL::Material;
		friend class OGL::GasGiantSurfaceMaterial;
		friend class OGL::GeoSphereSurfaceMaterial;
		friend class OGL::GeoSphereSkyMaterial;
		friend class OGL::GeoSphereStarMaterial;
		friend class OGL::GenGasGiantColourMaterial;
		friend class OGL::MultiMaterial;
		friend class OGL::LitMultiMaterial;
		friend class OGL::RingMaterial;
		friend class OGL::FresnelColourMaterial;
		friend class OGL::ShieldMaterial;
		friend class OGL::BillboardMaterial;
		std::vector<std::pair<MaterialDescriptor, OGL::Program *>> m_programs;
		std::unordered_map<uint32_t, OGL::RenderState *> m_renderStates;
		float m_invLogZfarPlus1;
		OGL::RenderTarget *m_activeRenderTarget;
		RenderState *m_activeRenderState;

		MatrixMode m_matrixMode;
		std::stack<matrix4x4f> m_modelViewStack;
		std::stack<matrix4x4f> m_projectionStack;

		struct Viewport {
			Viewport() :
				x(0),
				y(0),
				w(0),
				h(0) {}
			int32_t x, y, w, h;
		};
		std::stack<Viewport> m_viewportStack;

	private:
		static bool initted;

		typedef std::map<std::pair<AttributeSet, size_t>, RefCountedPtr<VertexBuffer>> AttribBufferMap;
		typedef AttribBufferMap::iterator AttribBufferIter;
		static AttribBufferMap s_AttribBufferMap;

		SDL_GLContext m_glContext;
	};
#define CHECKERRORS() RendererOGL::CheckErrors(__FUNCTION__, __LINE__)

} // namespace Graphics

#endif
