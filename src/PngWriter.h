// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef PNGWRITER_H
#define PNGWRITER_H

#include <cstdint>
#include <string>

namespace FileSystem {
	class FileSourceFS;
}

namespace Graphics {
	class ScreendumpState;
}

namespace PngWriter {
	// stride is in bytes (bytes per row)
	void write_png(FileSystem::FileSourceFS &fs, const std::string &path, const uint8_t *bytes, int width, int height, int stride, int bytes_per_pixel);

	void write_screenshot(const Graphics::ScreendumpState &sd, const char *destFile);
}

#endif
