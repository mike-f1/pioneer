// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _TEXTUREBUILDER_H
#define _TEXTUREBUILDER_H

#include "SDLWrappers.h"
#include "Texture.h"
#include <SDL.h>
#include <string>
#include <vector>

#include "PicoDDS/PicoDDS.h"

namespace Graphics {
	class Renderer;

	class TextureBuilder {
	public:
		TextureBuilder(const SDLSurfacePtr &surface, TextureSampleMode sampleMode = TextureSampleMode::LINEAR_CLAMP, bool generateMipmaps = false, bool potExtend = false, bool forceRGBA = true, bool compressTextures = true, bool anisoFiltering = true);
		TextureBuilder(const std::string &filename, TextureSampleMode sampleMode = TextureSampleMode::LINEAR_CLAMP, bool generateMipmaps = false, bool potExtend = false, bool forceRGBA = true, bool compressTextures = true, bool anisoFiltering = true, TextureType textureType = TextureType::T_2D);
		~TextureBuilder();

		static void Init();

		// convenience constructors for common texture types
		static TextureBuilder Model(const std::string &filename)
		{
			return TextureBuilder(filename, TextureSampleMode::LINEAR_REPEAT, true, false, false, true, true);
		}
		static TextureBuilder Normal(const std::string &filename)
		{
			return TextureBuilder(filename, TextureSampleMode::LINEAR_REPEAT, true, false, false, false, true);
		}
		static TextureBuilder Billboard(const std::string &filename)
		{
			return TextureBuilder(filename, TextureSampleMode::LINEAR_CLAMP, true, false, false, true, false);
		}
		static TextureBuilder UI(const std::string &filename)
		{
			return TextureBuilder(filename, TextureSampleMode::LINEAR_CLAMP, false, true, true, false, false);
		}
		static TextureBuilder Decal(const std::string &filename)
		{
			return TextureBuilder(filename, TextureSampleMode::LINEAR_CLAMP, true, true, false, true, true);
		}
		static TextureBuilder Raw(const std::string &filename)
		{
			return TextureBuilder(filename, TextureSampleMode::NEAREST_REPEAT, false, false, false, false, false);
		}
		static TextureBuilder Cube(const std::string &filename)
		{
			return TextureBuilder(filename, TextureSampleMode::LINEAR_CLAMP, true, true, false, true, false, TextureType::T_CUBE_MAP);
		}

		const TextureDescriptor &GetDescriptor()
		{
			PrepareSurface();
			return m_descriptor;
		}

		Texture *GetOrCreateTexture(Renderer *r, const std::string &type);

		//commonly used dummy textures
		static Texture *GetWhiteTexture(Renderer *);
		static Texture *GetTransparentTexture(Renderer *);

	private:
		SDLSurfacePtr m_surface;
		std::vector<SDLSurfacePtr> m_cubemap;
		PicoDDS::DDSImage m_dds;
		std::string m_filename;

		TextureSampleMode m_sampleMode;
		bool m_generateMipmaps;

		bool m_potExtend;
		bool m_forceRGBA;
		bool m_compressTextures;
		bool m_anisotropicFiltering;
		TextureType m_textureType;

		TextureDescriptor m_descriptor;

		Texture *CreateTexture(Renderer *r);
		void UpdateTexture(Texture *texture); // XXX pass src/dest rectangles
		void PrepareSurface();
		bool m_prepared;

		void LoadSurface();
		void LoadDDS();

		static SDL_mutex *m_textureLock;
	};

} // namespace Graphics

#endif
