// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef PIGUI_FACE_H
#define PIGUI_FACE_H

#include "FaceParts.h"
#include "libs/RefCounted.h"
#include "libs/vector2.h"

#include <memory>

namespace Graphics {
	class Material;
	class Texture;
	namespace Drawables {
		class TexturedQuad;
	}
};

namespace PiGUI {

	class Face : public RefCounted {
	public:
		Face(FaceParts::FaceDescriptor& face, uint32_t seed = 0);
		~Face();

		uint32_t GetTextureId();
		vector2f GetTextureSize();

		enum Flags { // <enum scope='GameUI::Face' name=GameUIFaceFlags public>
			RAND = 0,
			MALE = (1 << 0),
			FEMALE = (1 << 1),
			GENDER_MASK = 0x03, // <enum skip>

			ARMOUR = (1 << 2),
		};

	private:
		uint32_t m_seed;

		static RefCountedPtr<Graphics::Material> s_material;

		RefCountedPtr<Graphics::Texture> m_texture;
		std::unique_ptr<Graphics::Drawables::TexturedQuad> m_quad;
	};

} // namespace PiGUI

#endif
