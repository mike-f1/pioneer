// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _COLOR_H
#define _COLOR_H

#include <cstdint>

struct lua_State;

struct Color4f {
	float r, g, b, a;
	Color4f() :
		r(0.f),
		g(0.f),
		b(0.f),
		a(1.f) {}
	explicit Color4f(float v_) :
		r(v_),
		g(v_),
		b(v_),
		a(v_) {}
	Color4f(float r_, float g_, float b_) :
		r(r_),
		g(g_),
		b(b_),
		a(1.f) {}
	Color4f(float r_, float g_, float b_, float a_) :
		r(r_),
		g(g_),
		b(b_),
		a(a_) {}
	operator float *() { return &r; }
	operator const float *() const { return &r; }
	Color4f &operator*=(const float v)
	{
		r *= v;
		g *= v;
		b *= v;
		a *= v;
		return *this;
	}
	friend Color4f operator*(const Color4f &c, const float v) { return Color4f(c.r * v, c.g * v, c.b * v, c.a * v); }

	void ToLuaTable(lua_State *l);
	static Color4f FromLuaTable(lua_State *l, int idx);

	float GetLuminance() const;

	static const Color4f BLACK;
	static const Color4f WHITE;
	static const Color4f RED;
	static const Color4f GREEN;
	static const Color4f BLUE;
	static const Color4f YELLOW;
	static const Color4f GRAY;
	static const Color4f STEELBLUE;
	static const Color4f BLANK;
};

namespace {
	static const float s_inv255 = 1.0f / 255.0f;
#define INV255(n) (uint8_t(float(n) * s_inv255))
} // namespace

struct Color4ub {

	uint8_t r, g, b, a;
	Color4ub() :
		r(0),
		g(0),
		b(0),
		a(255) {}
	Color4ub(uint8_t r_, uint8_t g_, uint8_t b_) :
		r(r_),
		g(g_),
		b(b_),
		a(255) {}
	Color4ub(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_) :
		r(r_),
		g(g_),
		b(b_),
		a(a_) {}
	Color4ub(const Color4ub &c) = default;

	explicit Color4ub(const Color4f &c) :
		r(uint8_t(c.r * 255.f)),
		g(uint8_t(c.g * 255.f)),
		b(uint8_t(c.b * 255.f)),
		a(uint8_t(c.a * 255.f)) {}
	explicit Color4ub(const uint32_t rgba) :
		r(uint8_t((rgba >> 24) & 0xff)),
		g(uint8_t((rgba >> 16) & 0xff)),
		b(uint8_t((rgba >> 8) & 0xff)),
		a(uint8_t(rgba & 0xff)) {}

	Color4ub &operator=(const Color4ub &) = default;
	operator unsigned char *() { return &r; }
	operator const unsigned char *() const { return &r; }
	Color4ub operator+(const Color4ub &c) const { return Color4ub(uint8_t(c.r + r), uint8_t(c.g + g), uint8_t(c.b + b), uint8_t(c.a + a)); }
	Color4ub &operator*=(const float f)
	{
		r = uint8_t(r * f);
		g = uint8_t(g * f);
		b = uint8_t(b * f);
		a = uint8_t(a * f);
		return *this;
	}
	Color4ub &operator*=(const Color4ub &c)
	{
		r = uint8_t(r * INV255(c.r));
		g = uint8_t(r * INV255(c.g));
		b = uint8_t(r * INV255(c.b));
		a = uint8_t(r * INV255(c.a));
		return *this;
	}
	Color4ub operator*(const float f) const { return Color4ub(uint8_t(f * r), uint8_t(f * g), uint8_t(f * b), uint8_t(f * a)); }
	Color4ub operator*(const Color4ub &c) const { return Color4ub(uint8_t(INV255(c.r) * r), uint8_t(INV255(c.g) * g), uint8_t(INV255(c.b) * b), uint8_t(INV255(c.a) * a)); }
	Color4ub operator/(const float f) const { return Color4ub(uint8_t(r / f), uint8_t(g / f), uint8_t(b / f), uint8_t(a / f)); }

	friend bool operator==(const Color4ub &aIn, const Color4ub &bIn) { return ((aIn.r == bIn.r) && (aIn.g == bIn.g) && (aIn.b == bIn.b) && (aIn.a == bIn.a)); }
	friend bool operator!=(const Color4ub &aIn, const Color4ub &bIn) { return ((aIn.r != bIn.r) || (aIn.g != bIn.g) || (aIn.b != bIn.b) || (aIn.a != bIn.a)); }

	Color4f ToColor4f() const { return Color4f(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f); }

	void ToLuaTable(lua_State *l);
	static Color4ub FromLuaTable(lua_State *l, int idx);

	uint8_t GetLuminance() const;
	void Shade(float factor)
	{
		r = static_cast<uint8_t>(r * (1.0f - factor));
		g = static_cast<uint8_t>(g * (1.0f - factor));
		b = static_cast<uint8_t>(b * (1.0f - factor));
	}
	void Tint(float factor)
	{
		r = static_cast<uint8_t>(r + (255.0f - r) * factor);
		g = static_cast<uint8_t>(g + (255.0f - g) * factor);
		b = static_cast<uint8_t>(b + (255.0f - b) * factor);
	}

	static const Color4ub BLACK;
	static const Color4ub WHITE;
	static const Color4ub RED;
	static const Color4ub GREEN;
	static const Color4ub BLUE;
	static const Color4ub YELLOW;
	static const Color4ub GRAY;
	static const Color4ub STEELBLUE;
	static const Color4ub BLANK;
	static const Color4ub PINK;
};

struct Color3ub {
	uint8_t r, g, b;
	Color3ub() :
		r(0),
		g(0),
		b(0) {}
	explicit Color3ub(uint8_t v_) :
		r(v_),
		g(v_),
		b(v_) {}
	Color3ub(uint8_t r_, uint8_t g_, uint8_t b_) :
		r(r_),
		g(g_),
		b(b_) {}
	explicit Color3ub(const Color4f &c) :
		r(uint8_t(c.r * 255.f)),
		g(uint8_t(c.g * 255.f)),
		b(uint8_t(c.b * 255.f)) {}

	operator unsigned char *() { return &r; }
	operator const unsigned char *() const { return &r; }
	Color3ub &operator*=(const Color3ub &c)
	{
		r = uint8_t(r * INV255(c.r));
		g = uint8_t(r * INV255(c.g));
		b = uint8_t(r * INV255(c.b));
		return *this;
	}
	Color3ub operator+(const Color3ub &c) const { return Color3ub(uint8_t(c.r + r), uint8_t(c.g + g), uint8_t(c.b + b)); }
	Color3ub operator*(const float f) const { return Color3ub(uint8_t(f * r), uint8_t(f * g), uint8_t(f * b)); }
	Color3ub operator*(const Color3ub &c) const { return Color3ub(uint8_t(INV255(c.r) * r), uint8_t(INV255(c.g) * g), uint8_t(INV255(c.b) * b)); }
	Color3ub operator/(const float f) const { return Color3ub(uint8_t(r / f), uint8_t(g / f), uint8_t(b / f)); }

	Color4f ToColor4f() const { return Color4f(r / 255.0f, g / 255.0f, b / 255.0f); }

	static const Color3ub BLACK;
	static const Color3ub WHITE;
	static const Color3ub RED;
	static const Color3ub GREEN;
	static const Color3ub BLUE;
	static const Color3ub YELLOW;
	static const Color3ub STEELBLUE;
	static const Color3ub BLANK;
};

typedef Color4ub Color;

#endif /* _COLOR_H */
