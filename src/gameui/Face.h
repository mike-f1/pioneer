// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef GAMEUI_FACE_H
#define GAMEUI_FACE_H

#include "FaceParts.h"
#include "libs/SmartPtr.h"
#include "ui/Context.h"

namespace Graphics {
	namespace Drawables {
		class TexturedQuad;
	}
	class Material;
	class Texture;
}

namespace GameUI {

	class Face : public UI::Single {
	public:
		Face(UI::Context *context, FaceParts::FaceDescriptor& face, uint32_t seed = 0);

		virtual UI::Point PreferredSize();
		virtual void Layout();
		virtual void Draw();

		Face *SetHeightLines(uint32_t lines);

		enum Flags { // <enum scope='GameUI::Face' name=GameUIFaceFlags public>
			RAND = 0,
			MALE = (1 << 0),
			FEMALE = (1 << 1),
			GENDER_MASK = 0x03, // <enum skip>

			ARMOUR = (1 << 2),
		};

	private:
		UI::Point m_preferredSize;

		uint32_t m_seed;

		static RefCountedPtr<Graphics::Material> s_material;

		RefCountedPtr<Graphics::Texture> m_texture;
		std::unique_ptr<Graphics::Drawables::TexturedQuad> m_quad;
	};

} // namespace GameUI

#endif
