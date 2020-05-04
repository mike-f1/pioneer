// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef INPUT_H
#define INPUT_H

#include "KeyBindings.h"
#include "utils.h"

#include <algorithm>
#include <utility>

class InputFrame;

enum class MouseMotionBehaviour {
	Select,
	Rotate,
	Fire,
	DriveShip,
};

// When this get deleted, automatically restore previous state
// TODO: would be good to catch if InputFrames are changed (e.g. size is changed) in the means
class InputFrameStatusTicket {
	friend class Input;
public:
	~InputFrameStatusTicket();
	InputFrameStatusTicket &operator =(InputFrameStatusTicket &) = delete;
	InputFrameStatusTicket(const InputFrameStatusTicket &) = delete;
private:
	InputFrameStatusTicket(const std::vector<InputFrame *> &inputFrames);
	std::map<InputFrame *, bool> m_statuses;
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
	void TerminateGame();

	BindingPage &GetBindingPage(const std::string &id) { return m_bindingPages[id]; }
	const std::map<std::string, BindingPage> &GetBindingPages() { return m_bindingPages; };

	// Pushes an InputFrame onto the input stack, return true if
	// correctly pushed
	bool PushInputFrame(InputFrame *frame);

	// Get a read-only list of input frames.
	const std::vector<InputFrame *> &GetInputFrames() { return m_inputFrames; }

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
			FindAndEraseEntryInPagesAndGroups(id);
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
			FindAndEraseEntryInPagesAndGroups(id);
			return true;
		}
		return false;
	}

	bool KeyState(SDL_Keycode k) { return m_keyState[k]; }

	SDL_Keymod KeyModStateUnified() { return m_keyModStateUnified; }

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

	int MouseButtonState(int button) { return m_mouseButton.at(button); } // <- Still used for Pi::SetMouseGrab
	void SetMouseButtonState(int button, bool state) { m_mouseButton.at(button) = state; }

	// Return true if the behaviour is the current one and if at least a value is != 0,
	// second and third parameters are mouse movement relative coordinates
	std::tuple<bool, int, int> GetMouseMotion(MouseMotionBehaviour mmb);

	KeyBindings::WheelDirection GetWheelState() const { return m_wheelState; }

	sigc::signal<void, const SDL_Keysym &> onKeyPress; // <- Here only for 'ctrl-functions' in Pi.cpp
	sigc::signal<void, const SDL_Keysym &> onKeyRelease;

	void ResetMouseMotion()
	{
		m_mouseMotion.fill(0);
		m_wheelState = KeyBindings::WheelDirection::NONE;
	}

	void HandleSDLEvent(const SDL_Event &ev);

#ifdef DEBUG_DUMP_PAGES
	void DebugDumpPage(const std::string &pageId);
#endif // 0

private:
	void InitJoysticks();
	void RegisterInputBindings();

	// Check if a specific input frame is currently on the stack.
	bool HasInputFrame(InputFrame *frame)
	{
		return std::count(m_inputFrames.begin(), m_inputFrames.end(), frame) > 0;
	}

	// The only current "action": this is a general binding
	// used to speed up scroll/rotation/... of various bindings
	// TODO:
	// * when game ends it should be removed
	// * allow customization of speed values
	KeyBindings::ActionBinding *m_speedModifier;

	// Ok, wanna free pages and groups when an Axis or an Action
	// binding is deleted (e.g. deleting InputFrames).
	// (...and this means an ~O(n^3) time :P )
	void FindAndEraseEntryInPagesAndGroups(const std::string &id);

	std::map<SDL_Keycode, bool> m_keyState;
	SDL_Keymod m_keyModStateUnified;

	KeyBindings::WheelDirection m_wheelState; // Store last wheel position (must reset every frame)
	std::array<int, 2> m_mouseMotion; // Store last frame relative mouse motion (must reset every frame)
	std::array<bool, 6> m_mouseButton; // Store mouse button state

	bool m_joystickEnabled;
	bool m_mouseYInvert;
	std::map<SDL_JoystickID, JoystickState> m_joysticks;

	std::map<std::string, BindingPage> m_bindingPages;
	std::map<std::string, KeyBindings::ActionBinding> m_actionBindings;
	std::map<std::string, KeyBindings::AxisBinding> m_axisBindings;

	std::vector<InputFrame *> m_inputFrames;
};

#endif
