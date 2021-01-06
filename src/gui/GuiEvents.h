// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _GUIEVENTS_H
#define _GUIEVENTS_H

#include <cstdint>

namespace Gui {
	struct MouseButtonEvent {
		uint8_t isdown;
		uint8_t button;
		float x, y; // widget coords
		float screenX, screenY; // screen coords
		enum {
			BUTTON_WHEELUP = 0xfe,
			BUTTON_WHEELDOWN = 0xff
		};
	};
	struct MouseMotionEvent {
		float x, y; // widget coords
		float screenX, screenY; // screen coords
	};
} // namespace Gui

#endif /* _GUIEVENTS_H */
