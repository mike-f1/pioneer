// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef INPUT_H
#define INPUT_H

#include "KeyBindings.h"
#include "utils.h"

#include <algorithm>

class InputFrame;

enum class MouseButtonBehaviour {
	Select,
	Rotate,
	DriveShip,
};

// The Page->Group->Binding system serves as a thin veneer for the UI to make
// sane reasonings about how to structure the Options dialog.
struct BindingGroup {
	BindingGroup() = default;
	BindingGroup(const BindingGroup &) = delete;
	BindingGroup &operator=(const BindingGroup &) = delete;

	enum class EntryType {
		ACTION,
		AXIS,
		WHEEL
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

class Input {
public:
	Input() {};
	void Init();
	void InitGame();

	BindingPage &GetBindingPage(const std::string &id) { return m_bindingPages[id]; }
	const std::map<std::string, BindingPage> &GetBindingPages() { return m_bindingPages; };

	void CheckPage(const std::string &pageId);

	// Pushes an InputFrame onto the input stack, return true if
	// correctly pushed
	bool PushInputFrame(InputFrame *frame);

	// Get a read-only list of input frames.
	const std::vector<InputFrame *> &GetInputFrames() { return m_inputFrames; }

	// Check if a specific input frame is currently on the stack.
	bool HasInputFrame(InputFrame *frame)
	{
		return std::count(m_inputFrames.begin(), m_inputFrames.end(), frame) > 0;
	}

	// Remove an arbitrary input frame from the input stack.
	// return true if it was such frame
	bool RemoveInputFrame(InputFrame *frame);

	// Creates a new action binding, copying the provided binding.
	// The returned binding pointer points to the actual binding.
	// PS: 'id' may change if the same string is already in use
	KeyBindings::ActionBinding *AddActionBinding(std::string &id, BindingGroup &group, KeyBindings::ActionBinding binding);
	KeyBindings::ActionBinding *GetActionBinding(const std::string &id)
	{
		return m_actionBindings.count(id) ? &m_actionBindings.at(id) : nullptr;
	}
	bool DeleteActionBinding(const std::string &id)
	{
		if (m_actionBindings.erase(id) != 0) {
			FindAndEraseEntryInGroups(id);
			return true;
		}
		return false;
	}

	// Creates a new axis binding, copying the provided binding.
	// The returned binding pointer points to the actual binding.
	// PS: 'id' may change if the same string is already in use
	KeyBindings::AxisBinding *AddAxisBinding(std::string &id, BindingGroup &group, KeyBindings::AxisBinding binding);
	KeyBindings::AxisBinding *GetAxisBinding(const std::string &id)
	{
		return m_axisBindings.count(id) ? &m_axisBindings.at(id) : nullptr;
	}
	bool DeleteAxisBinding(const std::string &id)
	{
		if (m_axisBindings.erase(id) != 0) {
			FindAndEraseEntryInGroups(id);
			return true;
		}
		return false;
	}

	// PS: 'id' may change if the same string is already in use
	KeyBindings::WheelBinding *AddWheelBinding(std::string &id, BindingGroup &group, KeyBindings::WheelBinding binding);
	bool DeleteWheelBinding(const std::string &id)
	{
		if (m_wheelBindings.erase(id) != 0) {
			FindAndEraseEntryInGroups(id);
			return true;
		}
		return false;
	}

	bool KeyState(SDL_Keycode k) { return m_keyState[k]; }
	int KeyModState() { return m_keyModState; }

	// Get the default speed modifier to apply to movement (scrolling, zooming...), depending on the "shift" keys.
	// This is a default value only, centralized here to promote uniform user experience.
	float GetMoveSpeedShiftModifier();

	int JoystickButtonState(int joystick, int button);
	int JoystickHatState(int joystick, int hat);
	float JoystickAxisState(int joystick, int axis);

	bool IsJoystickEnabled() { return m_joystickEnabled; }
	void SetJoystickEnabled(bool state) { m_joystickEnabled = state; }

	struct JoystickState {
		SDL_Joystick *joystick;
		SDL_JoystickGUID guid;
		std::vector<bool> buttons;
		std::vector<int> hats;
		std::vector<float> axes;
	};
	std::map<SDL_JoystickID, JoystickState> GetJoysticksState() { return m_joysticks; }

	// User display name for the joystick from the API/OS.
	std::string JoystickName(int joystick);
	// fetch the GUID for the named joystick
	SDL_JoystickGUID JoystickGUID(int joystick);
	std::string JoystickGUIDString(int joystick);

	// reverse map a JoystickGUID to the actual internal ID.
	int JoystickFromGUIDString(const std::string &guid);
	int JoystickFromGUIDString(const char *guid);
	int JoystickFromGUID(SDL_JoystickGUID guid);

	void SetMouseYInvert(bool state) { m_mouseYInvert = state; }
	bool IsMouseYInvert() { return m_mouseYInvert; }

	int MouseButtonState(int button) { return m_mouseButton[button]; }
	void SetMouseButtonState(int button, bool state) { m_mouseButton[button] = state; }

	void GetMouseMotion(int motion[2])
	{
		memcpy(motion, m_mouseMotion, sizeof(int) * 2);
	}

	sigc::signal<void, const SDL_Keysym &> onKeyPress;
	sigc::signal<void, const SDL_Keysym &> onKeyRelease;
	sigc::signal<void, int, int, int> onMouseButtonUp;
	sigc::signal<void, int, int, int> onMouseButtonDown;
	sigc::signal<void, bool> onMouseWheel;

	void ResetMouseMotion()
	{
		m_mouseMotion[0] = m_mouseMotion[1] = 0;
	}

	void HandleSDLEvent(const SDL_Event &ev);
private:
	void InitJoysticks();

	void FindAndEraseEntryInGroups(const std::string &id);

	std::map<SDL_Keycode, bool> m_keyState;
	int m_keyModState;
	char m_mouseButton[6];
	int m_mouseMotion[2];

	bool m_joystickEnabled;
	bool m_mouseYInvert;
	std::map<SDL_JoystickID, JoystickState> m_joysticks;

	std::map<std::string, BindingPage> m_bindingPages;
	std::map<std::string, KeyBindings::ActionBinding> m_actionBindings;
	std::map<std::string, KeyBindings::AxisBinding> m_axisBindings;
	std::map<std::string, KeyBindings::WheelBinding> m_wheelBindings;

	std::vector<InputFrame *> m_inputFrames;
};

#endif
