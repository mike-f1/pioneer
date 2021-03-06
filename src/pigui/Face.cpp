// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Face.h"

#include "FileSystem.h"
#include "SDLWrappers.h"
#include "graphics/Material.h"
#include "graphics/Drawables.h"
#include "graphics/Renderer.h"
#include "graphics/RendererLocator.h"
#include "graphics/Texture.h"
#include "graphics/TextureBuilder.h"

#include "profiler/Profiler.h"

namespace PiGUI {

	RefCountedPtr<Graphics::Material> Face::s_material;

	Face::Face(FaceParts::FaceDescriptor &face, uint32_t seed)
	{
		PROFILE_SCOPED()
		if (!seed) seed = time(0);

		m_seed = seed;

		SDLSurfacePtr faceim = SDLSurfacePtr::WrapNew(SDL_CreateRGBSurface(SDL_SWSURFACE, FaceParts::FACE_WIDTH, FaceParts::FACE_HEIGHT, 24, 0xff, 0xff00, 0xff0000, 0));

		FaceParts::PickFaceParts(face, m_seed);
		FaceParts::BuildFaceImage(faceim.Get(), face);

		m_texture.Reset(Graphics::TextureBuilder(faceim, Graphics::TextureSampleMode::LINEAR_CLAMP, true, true).GetOrCreateTexture(RendererLocator::getRenderer(), std::string("face")));

		if (!s_material) {
			Graphics::MaterialDescriptor matDesc;
			matDesc.textures = 1;
			s_material.Reset(RendererLocator::getRenderer()->CreateMaterial(matDesc));
		}
	}

	Face::~Face()
	{}

	uint32_t Face::GetTextureId()
	{
		return m_texture->GetTextureID();
	}

	vector2f Face::GetTextureSize()
	{
		return m_texture->GetDescriptor().texSize;
	}

} // namespace PiGUI
