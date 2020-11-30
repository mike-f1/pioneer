#ifndef INPUTFWD_H
#define INPUTFWD_H

#include <map>
#include <string>

struct ActionId {
	static constexpr int Invalid = -1;
	constexpr ActionId() :
		m_id(Invalid) {}
	constexpr ActionId(int new_id) :
		m_id(new_id) {}

	constexpr operator bool() const { return m_id > Invalid; }
	constexpr operator int() const { return m_id; }
	constexpr operator size_t() const { return m_id; }

	constexpr bool operator==(ActionId rhs) const { return m_id == rhs.m_id; }
	constexpr bool operator!=(ActionId rhs) const { return m_id != rhs.m_id; }

	constexpr bool valid() const { return m_id > Invalid; }
	constexpr int id() const { return m_id; }

private:
	int m_id;
};

struct AxisId {
	static constexpr int Invalid = -1;
	constexpr AxisId() :
		m_id(Invalid) {}
	constexpr AxisId(int new_id) :
		m_id(new_id) {}

	constexpr operator bool() const { return m_id > Invalid; }
	constexpr operator int() const { return m_id; }
	constexpr operator size_t() const { return m_id; }

	constexpr bool operator==(AxisId rhs) const { return m_id == rhs.m_id; }
	constexpr bool operator!=(AxisId rhs) const { return m_id != rhs.m_id; }

	constexpr bool valid() const { return m_id > Invalid; }
	constexpr int id() const { return m_id; }

private:
	int m_id;
};

// The Page->Group->Binding system serves as a thin veneer for the UI to make
// sane reasonings about how to structure the Options dialog.
// TODO: Do a step more defining an 'hint' for ordering
struct BindingGroup {
	BindingGroup() = default;
	BindingGroup(const BindingGroup &) = delete;
	BindingGroup &operator=(const BindingGroup &) = delete;

	enum class EntryType {
		ACTION,
		AXIS,
	};

	std::map<std::string, EntryType> bindings;
};

struct BindingPage {
	BindingPage() = default;
	BindingPage(const BindingPage &) = delete;
	BindingPage &operator=(const BindingPage &) = delete;

	BindingGroup &GetBindingGroup(const std::string &id) { return groups[id]; }

	std::map<std::string, BindingGroup> groups;

	bool shouldBeTranslated = true;
};

enum class MouseMotionBehaviour {
	Select,
	Rotate,
	Fire,
	DriveShip,
};

#endif
