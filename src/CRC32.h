// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _CRC32_H
#define _CRC32_H

#include <cstdint>

class CRC32 {
public:
	CRC32();

	void AddData(const char *data, int length);
	uint32_t GetChecksum() const { return m_checksum; }

private:
	uint32_t m_checksum;

	static const uint32_t s_polynomial;
	static bool s_lookupTableGenerated;
	static uint32_t s_lookupTable[256];
};

#endif
