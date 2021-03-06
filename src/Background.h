// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _BACKGROUND_H
#define _BACKGROUND_H

#include "Color.h"
#include "libs/RefCounted.h"
#include "libs/matrix4x4.h"
#include "libs/vector3.h"
#include <memory>

class Random;

namespace Graphics {
	class Renderer;
	class Material;
	class RenderState;
	class Texture;
	class VertexBuffer;
	namespace Drawables {
		class PointSprites;
	} // namespace Drawables
} // namespace Graphics

/*
 * Classes to draw background stars and the milky way
 */

namespace Background {
	class BackgroundElement {
	public:
		BackgroundElement();
		~BackgroundElement();

		void SetIntensity(float intensity);

	protected:
		RefCountedPtr<Graphics::Material> m_material;
		RefCountedPtr<Graphics::Material> m_materialStreaks;

		float m_rMin;
		float m_rMax;
		float m_gMin;
		float m_gMax;
		float m_bMin;
		float m_bMax;
	};

	class UniverseBox : public BackgroundElement {
	public:
		UniverseBox();
		~UniverseBox();

		void Draw(Graphics::RenderState *);
		void LoadCubeMap(Random &rand);

	private:
		void Init();

		std::unique_ptr<Graphics::VertexBuffer> m_vertexBuffer;
		RefCountedPtr<Graphics::Texture> m_cubemap;

		uint32_t m_numCubemaps;
	};

	class Starfield : public BackgroundElement {
	public:
		//does not Fill the starfield
		Starfield(Random &rand, float amount);
		~Starfield();
		void Draw(Graphics::RenderState *);
		//create or recreate the starfield
		void Fill(Random &rand, float amount);

	private:
		void Init();

		std::unique_ptr<Graphics::Drawables::PointSprites> m_pointSprites;
		Graphics::RenderState *m_renderState; // NB: we don't own RenderState pointers, just borrow them

		//hyperspace animation vertex data
		std::unique_ptr<vector3f[]> m_hyperVtx; // BG_STAR_MAX * 3
		std::unique_ptr<Color[]> m_hyperCol; // BG_STAR_MAX * 3
		std::unique_ptr<Graphics::VertexBuffer> m_animBuffer;
	};

	class MilkyWay : public BackgroundElement {
	public:
		MilkyWay();
		void Draw(Graphics::RenderState *);

	private:
		std::unique_ptr<Graphics::VertexBuffer> m_vertexBuffer;
	};

	// contains starfield, milkyway, possibly other Background elements
	class Container {
	public:
		enum BackgroundDrawFlags {
			DRAW_STARS = 1 << 0,
			DRAW_MILKY = 1 << 1,
			DRAW_SKYBOX = 1 << 2
		};

		Container(Random &rand, float amountOfBackgroundStars);
		~Container();
		void Draw(const matrix4x4d &transform);

		void SetIntensity(float intensity);
		void SetDrawFlags(const uint32_t flags);

	private:
		void Refresh(Random &rand, float amountOfBackgroundStars);

		MilkyWay m_milkyWay;
		Starfield m_starField;
		UniverseBox m_universeBox;
		uint32_t m_drawFlags;
		Graphics::RenderState *m_renderState;
	};

} //namespace Background

#endif
