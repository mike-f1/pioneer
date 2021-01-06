// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef PIGUI_IMAGE_H
#define PIGUI_IMAGE_H

#include "libs/RefCounted.h"
#include "libs/vector2.h"
#include <cstdint>
#include <string>

namespace Graphics {
	class Texture;
}

namespace PiGUI {

	class Image : public RefCounted {
	public:
		explicit Image(const std::string &filename);

		uint32_t GetId();
		vector2f GetSize();
		vector2f GetUv();

	private:
		RefCountedPtr<Graphics::Texture> m_texture;
	};

} // namespace PiGUI

#endif
