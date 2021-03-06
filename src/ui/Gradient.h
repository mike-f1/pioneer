// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef UI_GRADIENT_H
#define UI_GRADIENT_H

#include "Color.h"
#include "Single.h"
#include "graphics/Drawables.h"
#include "graphics/Material.h"

namespace UI {

	class Gradient : public Single {
	public:
		enum Direction { // <enum scope='UI::Gradient' name=UIGradientDirection public>
			HORIZONTAL,
			VERTICAL
		};

		virtual void Draw();

	protected:
		friend class Context;
		Gradient(Context *context, const Color &beginColor, const Color &endColor, Direction direction);

	private:
		Color m_beginColor;
		Color m_endColor;
		Direction m_direction;

		RefCountedPtr<Graphics::Material> m_material;
		std::unique_ptr<Graphics::Drawables::TexturedQuad> m_quad;
	};

} // namespace UI

#endif
