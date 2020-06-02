// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _SMARTPTR_H
#define _SMARTPTR_H

#include <cassert>
#include <cstddef> // for ptrdiff_t
#include <cstdlib> // for free()

#ifdef __GNUC__
#define WARN_UNUSED_RESULT(ret, decl) ret decl __attribute__((warn_unused_result))
#else
#define WARN_UNUSED_RESULT(ret, decl) ret decl
#endif

template <typename Derived, typename T>
class SmartPtrBase {
	typedef SmartPtrBase<Derived, T> this_type;
	typedef T *this_type::*safe_bool;

public:
	// copy & swap idiom
	// see http://stackoverflow.com/questions/3279543/what-is-the-copy-and-swap-idiom
	void Reset(T *p = nullptr) { Derived(p).Swap(*static_cast<Derived *>(this)); }

	T &operator*() const
	{
		assert(m_ptr);
		return *m_ptr;
	}
	T *operator->() const
	{
		assert(m_ptr);
		return m_ptr;
	}
	T *Get() const { return m_ptr; }
	bool Valid() const { return (m_ptr != nullptr); }

	// safe bool idiom; see http://www.artima.com/cppsource/safebool.html
	operator safe_bool() const { return (m_ptr == nullptr) ? nullptr : &this_type::m_ptr; }
	bool operator!() const { return (m_ptr == nullptr); }

	friend void swap(this_type &a, this_type &b) { a.Swap(b); }

	void Swap(this_type &b)
	{
		T *p = m_ptr;
		m_ptr = b.m_ptr;
		b.m_ptr = p;
	}

	friend bool operator==(const T *a, const this_type &b) { return (a == b.Get()); }
	friend bool operator!=(const T *a, const this_type &b) { return (a != b.Get()); }
	friend bool operator<(const T *a, const this_type &b) { return (a < b.Get()); }
	friend bool operator<=(const T *a, const this_type &b) { return (a <= b.Get()); }
	friend bool operator>(const T *a, const this_type &b) { return (a > b.Get()); }
	friend bool operator>=(const T *a, const this_type &b) { return (a >= b.Get()); }

protected:
	SmartPtrBase() :
		m_ptr(nullptr) {}
	explicit SmartPtrBase(T *p) :
		m_ptr(p) {}

	// Release() doesn't make sense for all smart pointer types
	// (e.g., RefCountedPtr can't Release, it can only Reset)
	WARN_UNUSED_RESULT(T *, Release())
	{
		T *p = m_ptr;
		m_ptr = nullptr;
		return p;
	}

	T *m_ptr = nullptr;
};

#define DEF_SMARTPTR_COMPARISON(op)                                                       \
	template <typename D1, typename T1, typename D2, typename T2>                         \
	inline bool operator op(const SmartPtrBase<D1, T1> &a, const SmartPtrBase<D2, T2> &b) \
	{                                                                                     \
		return (a.Get() op b.Get());                                                      \
	}                                                                                     \
	template <typename D, typename T1, typename T2>                                       \
	inline bool operator op(const SmartPtrBase<D, T1> &a, const T2 *b)                    \
	{                                                                                     \
		return (a.Get() op b);                                                            \
	}                                                                                     \
	template <typename D, typename T1, typename T2>                                       \
	inline bool operator op(const T1 *a, const SmartPtrBase<D, T2> &b)                    \
	{                                                                                     \
		return (a op b.Get());                                                            \
	}
DEF_SMARTPTR_COMPARISON(==)
DEF_SMARTPTR_COMPARISON(!=)
DEF_SMARTPTR_COMPARISON(<)
DEF_SMARTPTR_COMPARISON(<=)
DEF_SMARTPTR_COMPARISON(>)
DEF_SMARTPTR_COMPARISON(>=)
#undef DEF_SMARTPTR_COMPARISON

// a deleter type for use with std::unique_ptr
struct FreeDeleter {
	void operator()(void *p) const { std::free(p); }
};

#endif
