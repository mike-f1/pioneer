// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "ColorMap.h"
#include "graphics/Texture.h"
#include "graphics/Renderer.h"
#include <SDL_stdinc.h>

namespace SceneGraph {

	ColorMap::ColorMap() :
		m_smooth(true)
	{
	}

	Graphics::Texture *ColorMap::GetTexture()
	{
		assert(m_texture.Valid());
		return m_texture.Get();
	}

	void ColorMap::AddColor(int width, const Color &c, std::vector<Uint8> &out)
	{
		for (int i = 0; i < width; i++) {
			out.push_back(c.r);
			out.push_back(c.g);
			out.push_back(c.b);
		}
	}

	void ColorMap::Generate(Graphics::Renderer *r, const Color &a, const Color &b, const Color &c)
	{
		std::vector<Uint8> colors;
		const int w = 4;
		AddColor(w, Color(255, 255, 255), colors);
		AddColor(w, a, colors);
		AddColor(w, b, colors);
		AddColor(w, c, colors);
		vector2f size(colors.size() / 3, 1.f);

		const Graphics::TextureFormat format = Graphics::TextureFormat::RGB_888;

		if (!m_texture.Valid()) {
			const Graphics::TextureSampleMode sampleMode = m_smooth ? Graphics::TextureSampleMode::LINEAR_CLAMP : Graphics::TextureSampleMode::NEAREST_CLAMP;
			m_texture.Reset(r->CreateTexture(Graphics::TextureDescriptor(Graphics::TextureFormat::RGB_888, size, sampleMode, true, true, true, 0, Graphics::TextureType::T_2D)));
		}

		m_texture->Update(&colors[0], size, format);
	}

	void ColorMap::SetSmooth(bool smooth)
	{
		m_smooth = smooth;
		if (m_texture.Valid()) {
			m_texture->SetSampleMode(m_smooth ? Graphics::TextureSampleMode::LINEAR_CLAMP : Graphics::TextureSampleMode::NEAREST_CLAMP);
		}
	}

} // namespace SceneGraph
