// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _SCENEGRAPH_COLORMAP_H
#define _SCENEGRAPH_COLORMAP_H
/*
 * Color look-up texture generator for newmodel pattern system
 */

#include <cstdint>
#include <vector>
#include "Color.h"
#include "libs/RefCounted.h"

namespace Graphics {
	class Texture;
	class Renderer;
}

namespace SceneGraph {

	class ColorMap {
	public:
		ColorMap();
		Graphics::Texture *GetTexture();
		void Generate(Graphics::Renderer *r, const Color &a, const Color &b, const Color &c);
		void SetSmooth(bool);

	private:
		void AddColor(int width, const Color &c, std::vector<uint8_t> &out);

		bool m_smooth;
		RefCountedPtr<Graphics::Texture> m_texture;
	};

} // namespace SceneGraph

#endif
