// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef INPUT_H
#define INPUT_H

#include "InputFwd.h"
#include "KeyBindings.h"
#include "BindingContainer.h"

#include "libs/RefCounted.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>
#include <vector>

class BindingContainer;
class InputFrame;
class InputFrameStatusTicket;
class JoyStick;

class Input {
public:
	Input();
	void InitGame();
	void TerminateGame();

	// Calling functions which initializes static InputFrame at Init time
	// NOTE: Can't be merged  in ctor due to InputFrames calling InputLocator
	void InitializeInputBindings(std::vector<std::function<void(void)>> &bindings_registerer);

	BindingPage &GetBindingPage(const std::string &id) { return m_bindingPages[id]; }
	const std::map<std::string, BindingPage> &GetBindingPages() { return m_bindingPages; };

	RefCountedPtr<BindingContainer> CreateOrShareBindContainer(const std::string &name, InputFrame *iframe);

	// Remove an arbitrary BindingContainer from the input stack.
	// return true if it was such frame
	bool RemoveBindingContainer(InputFrame *iframe);

	std::unique_ptr<InputFrameStatusTicket> DisableAllInputFrameExcept(InputFrame *current);

	// Creates a new action binding, copying the provided binding.
	// The returned binding pointer points to the actual binding.
	// NOTE: 'id' will change if the same string is already in use
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
	// PS: 'id' will change if the same string is already in use
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

	bool IsAnyKeyJustPressed() { return m_keyJustPressed != 0; }
	bool KeyState(SDL_Keycode k) { return m_keyState[k]; }

	SDL_Keymod KeyModStateUnified() { return m_keyModStateUnified; }

	// Get the default speed modifier to apply to movement (scrolling, zooming...), depending on the "shift" keys.
	// This is a default value only, centralized here to promote uniform user experience.
	float GetMoveSpeedShiftModifier() const;

	auto GetJoystick() const {return m_joystick.get(); }
	bool IsJoystickEnabled() const { return m_joystickEnabled; }
	void SetJoystickEnabled(bool state);

	void SetMouseYInvert(bool state) { m_mouseYInvert = state; }
	bool IsMouseYInvert() { return m_mouseYInvert; }

	int MouseButtonState(int button) { return m_mouseButton.at(button); } // <- Still used for Pi::SetMouseGrab
	void SetMouseButtonState(int button, bool state) { m_mouseButton.at(button) = state; }

	// Return true if the behaviour is the current one and if at least a value is != 0,
	// second and third parameters are mouse movement relative coordinates
	std::tuple<bool, int, int> GetMouseMotion(MouseMotionBehaviour mmb);

	KeyBindings::WheelDirection GetWheelState() const { return m_wheelState; }

	void ResetFrameInput()
	{
		m_keyJustPressed = 0;
		m_mouseMotion.fill(0);
		m_wheelState = KeyBindings::WheelDirection::NONE;
	}

	void HandleSDLEvent(const SDL_Event &ev);

#ifdef DEBUG_DUMP_PAGES
	void DebugDumpPage(const std::string &pageId);
#endif // DEBUG_DUMP_PAGES

private:
	unsigned m_keyJustPressed;

	void RegisterInputBindings();

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

	// TODO: finish decoupling these classes as actually, because of how Keybindings works, they always needs
	//  an instance of JoyStick up and running. This means we never can reset m_joystick or we incur in segfaults,
	// and there's a need for below bool :P
	bool m_joystickEnabled;
	std::unique_ptr<JoyStick> m_joystick;

	bool m_mouseYInvert;

	std::map<std::string, BindingPage> m_bindingPages;
	std::map<std::string, KeyBindings::ActionBinding> m_actionBindings;
	std::map<std::string, KeyBindings::AxisBinding> m_axisBindings;

	void PurgeBindingContainers();

	std::vector<RefCountedPtr<BindingContainer>> m_bindingContainers;
	std::vector<InputFrame *> m_inputFrames;

	std::unique_ptr<InputFrame> m_generalPanRotateZoom;
};

#endif
