// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _FIXED_H
#define _FIXED_H

#include <cstdint>
#include <cassert>

template <int FRAC_BITS>
class fixedf {
public:
	static const int FRAC = FRAC_BITS;
	static const uint64_t MASK = (uint64_t(1UL) << FRAC_BITS) - 1;

	fixedf() :
		v(0) {}
	//	template <int bits>
	//	fixedf(fixedf<bits> f) { *this = f; }
	explicit fixedf(int64_t raw) :
		v(raw) {}
	fixedf(int64_t num, int64_t denom) :
		v((num << FRAC) / denom) {}
	// ^^ this is fucking shit

	fixedf Abs() const { return fixedf(v >= 0 ? v : -v); }
	friend fixedf operator+(const fixedf &a, const int64_t b) { return a + fixedf(b << FRAC); }
	friend fixedf operator-(const fixedf &a, const int64_t b) { return a - fixedf(b << FRAC); }
	friend fixedf operator*(const fixedf &a, const int64_t b) { return a * fixedf(b << FRAC); }
	friend fixedf operator/(const fixedf &a, const int64_t b) { return a / fixedf(b << FRAC); }
	friend fixedf operator+(const int64_t a, const fixedf &b) { return fixedf(a << FRAC) + b; }
	friend fixedf operator-(const int64_t a, const fixedf &b) { return fixedf(a << FRAC) - b; }
	friend fixedf operator*(const int64_t a, const fixedf &b) { return fixedf(a << FRAC) * b; }
	friend fixedf operator/(const int64_t a, const fixedf &b) { return fixedf(a << FRAC) / b; }
	friend bool operator==(const fixedf &a, const int64_t b) { return a == fixedf(b << FRAC); }
	friend bool operator==(const int64_t a, const fixedf &b) { return b == fixedf(a << FRAC); }
	friend bool operator!=(const fixedf &a, const int64_t b) { return a != fixedf(b << FRAC); }
	friend bool operator!=(const int64_t a, const fixedf &b) { return b != fixedf(a << FRAC); }
	friend bool operator>=(const fixedf &a, const int64_t b) { return a >= fixedf(b << FRAC); }
	friend bool operator>=(const int64_t a, const fixedf &b) { return b >= fixedf(a << FRAC); }
	friend bool operator<=(const fixedf &a, const int64_t b) { return a <= fixedf(b << FRAC); }
	friend bool operator<=(const int64_t a, const fixedf &b) { return b <= fixedf(a << FRAC); }
	friend bool operator>(const fixedf &a, const int64_t b) { return a > fixedf(b << FRAC); }
	friend bool operator>(const int64_t a, const fixedf &b) { return b > fixedf(a << FRAC); }
	friend bool operator<(const fixedf &a, const int64_t b) { return a < fixedf(b << FRAC); }
	friend bool operator<(const int64_t a, const fixedf &b) { return b < fixedf(a << FRAC); }
	friend fixedf operator>>(const fixedf &a, const int b) { return fixedf(a.v >> b); }
	friend fixedf operator<<(const fixedf &a, const int b) { return fixedf(a.v << b); }

	fixedf &operator*=(const fixedf &a)
	{
		(*this) = (*this) * a;
		return (*this);
	}
	fixedf &operator*=(const int64_t a)
	{
		(*this) = (*this) * a;
		return (*this);
	}
	fixedf &operator/=(const fixedf &a)
	{
		(*this) = (*this) / a;
		return (*this);
	}
	fixedf &operator/=(const int64_t a)
	{
		(*this) = (*this) / a;
		return (*this);
	}
	fixedf &operator+=(const fixedf &a)
	{
		(*this) = (*this) + a;
		return (*this);
	}
	fixedf &operator+=(const int64_t a)
	{
		(*this) = (*this) + a;
		return (*this);
	}
	fixedf &operator-=(const fixedf &a)
	{
		(*this) = (*this) - a;
		return (*this);
	}
	fixedf &operator-=(const int64_t a)
	{
		(*this) = (*this) - a;
		return (*this);
	}
	fixedf &operator>>=(const int a)
	{
		v >>= a;
		return (*this);
	}
	fixedf &operator<<=(const int a)
	{
		v <<= a;
		return (*this);
	}

	friend fixedf operator-(const fixedf &a) { return fixedf(-a.v); }
	friend fixedf operator+(const fixedf &a, const fixedf &b) { return fixedf(a.v + b.v); }
	friend fixedf operator-(const fixedf &a, const fixedf &b) { return fixedf(a.v - b.v); }
	friend fixedf operator*(const fixedf &a, const fixedf &b)
	{
		// 64*64 = (128bit>>FRAC) & ((1<<64)-1)
		//return fixedf(a.v*b.v >> FRAC);
		int64_t hi = 0;
		uint64_t a0, a1, b0, b1;
		uint64_t lo = 0;
		uint64_t oldlo;
		int isneg = 0;
		if (a.v < 0) {
			a0 = (-a.v) & 0xffffffff;
			a1 = (-a.v) >> 32;
			isneg = !isneg;
		} else {
			a0 = a.v & 0xffffffff;
			a1 = a.v >> 32;
		}
		if (b.v < 0) {
			b0 = (-b.v) & 0xffffffff;
			b1 = (-b.v) >> 32;
			isneg = !isneg;
		} else {
			b0 = b.v & 0xffffffff;
			b1 = b.v >> 32;
		}
		uint64_t x;
		// a0 * b;
		lo = a0 * b0;
		oldlo = lo;
		x = a0 * b1;
		lo += x << 32;
		if (lo < oldlo) hi++;
		oldlo = lo;
		hi += (x >> 32);

		// a1 * b;
		x = a1 * b0;
		lo += x << 32;
		if (lo < oldlo) hi++;
		hi += x >> 32;

		hi += a1 * b1;
		int64_t out = (lo >> FRAC) + ((hi & MASK) << (64 - FRAC));
		return isneg ? -static_cast<fixedf>(out) : static_cast<fixedf>(out);
	}
	friend fixedf operator/(const fixedf &a, const fixedf &b)
	{
		// 128-bit divided by 64-bit, to make sure high bits are not lost
		int64_t quotient_hi = a.v >> (64 - FRAC);
		uint64_t quotient_lo = a.v << FRAC;
		int64_t d = b.v;
		int isneg = 0;
		int64_t remainder = 0;

		if (d < 0) {
			d = -d;
			isneg = 1;
		}

		for (int i = 0; i < 128; i++) {
			uint64_t sbit = (uint64_t(1) << 63) & quotient_hi;
			remainder <<= 1;
			if (sbit) remainder |= 1;
			// shift quotient left 1
			{
				quotient_hi <<= 1;
				if (quotient_lo & (uint64_t(1) << 63)) quotient_hi |= 1;
				quotient_lo <<= 1;
			}
			if (remainder >= d) {
				remainder -= d;
				quotient_lo |= 1;
			}
		}
		return (isneg ? -static_cast<fixedf>(int64_t(quotient_lo)) : static_cast<fixedf>(quotient_lo));
	}
	friend bool operator==(const fixedf &a, const fixedf &b) { return a.v == b.v; }
	friend bool operator!=(const fixedf &a, const fixedf &b) { return a.v != b.v; }
	friend bool operator>(const fixedf &a, const fixedf &b) { return a.v > b.v; }
	friend bool operator<(const fixedf &a, const fixedf &b) { return a.v < b.v; }
	friend bool operator>=(const fixedf &a, const fixedf &b) { return a.v >= b.v; }
	friend bool operator<=(const fixedf &a, const fixedf &b) { return a.v <= b.v; }

	/* implicit operator float() bad */
	inline int ToInt32() const { return int(v >> FRAC); }
	inline int64_t ToInt64() const { return v >> FRAC; }
	inline float ToFloat() const { return v / float(int64_t(1) << FRAC); }
	inline double ToDouble() const { return v / double(int64_t(1) << FRAC); }

	static fixedf FromDouble(const double val) { return fixedf(int64_t(((val) * double(int64_t(1) << FRAC)))); }

	template <int NEW_FRAC_BITS>
	operator fixedf<NEW_FRAC_BITS>() const
	{
		int shift = NEW_FRAC_BITS - FRAC_BITS;
		if (shift > 0)
			return fixedf<NEW_FRAC_BITS>(v << shift);
		else
			return fixedf<NEW_FRAC_BITS>(v >> (-shift));
	}

	static fixedf SqrtOf(const fixedf &a)
	{
		/* only works on even-numbered fractional bits */
		assert(!(FRAC & 1));
		uint64_t root, remHi, remLo, count;
		root = 0;
		remHi = 0;
		remLo = a.v;
		count = 32 + (FRAC >> 1) - 1;
		do {
			remHi = (remHi << 2) | (remLo >> 62);
			remLo <<= 2;
			root <<= 1;
			uint64_t testDiv = (root << 1) + 1;
			if (remHi >= testDiv) {
				remHi -= testDiv;
				root++;
			}
		} while (count-- != 0);

		return (fixedf(root));
	}

	static fixedf CubeRootOf(const fixedf &a)
	{
		/* NR method. XXX very bad initial estimate (we get there in
		 * the end... XXX */
		fixedf x = a;
		for (int i = 0; i < 48; i++)
			x = fixedf(1, 3) * ((a / (x * x)) + 2 * x);
		return x;
	}

	int64_t v;
};

typedef fixedf<32> fixed;

/* Left here for reference: it was in "libs/utils.h" but seems unused.
Could be made part of fixedf class but it works for fixed (= fixedf<32>)
and I don't know if it's generic enought to be part of this library.
static inline int64_t isqrt(fixed v)
{
	int64_t ret = 0;
	int64_t s;
	int64_t ret_sq = -v.v - 1;
	for (s = 62; s >= 0; s -= 2) {
		int64_t b;
		ret += ret;
		b = ret_sq + ((2 * ret + 1) << s);
		if (b < 0) {
			ret_sq = b;
			ret++;
		}
	}
	return ret;
}
*/
#endif /* _FIXED_H */
