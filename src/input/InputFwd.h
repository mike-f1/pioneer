#ifndef INPUTFWD_H
#define INPUTFWD_H

#include <map>
#include <string>

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
