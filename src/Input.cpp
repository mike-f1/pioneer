// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Input.h"

#include "GameConfig.h"
#include "GameConfSingleton.h"
#include "InputFrame.h"

#include "profiler/Profiler.h"

void Input::Init()
{
	GameConfig &config = GameConfSingleton::getInstance();

	m_joystickEnabled = (config.Int("EnableJoystick")) ? true : false;
	m_mouseYInvert = (config.Int("InvertMouseY")) ? true : false;

	InitJoysticks();
}

void Input::InitGame()
{
	//reset input states
	m_keyState.clear();
	m_keyModState = 0;
	std::fill(m_mouseButton, m_mouseButton + COUNTOF(m_mouseButton), 0);
	std::fill(m_mouseMotion, m_mouseMotion + COUNTOF(m_mouseMotion), 0);
	for (std::map<SDL_JoystickID, JoystickState>::iterator stick = m_joysticks.begin(); stick != m_joysticks.end(); ++stick) {
		JoystickState &state = stick->second;
		std::fill(state.buttons.begin(), state.buttons.end(), false);
		std::fill(state.hats.begin(), state.hats.end(), 0);
		std::fill(state.axes.begin(), state.axes.end(), 0.f);
	}
}

void Input::CheckPage(const std::string &pageId)
{
	Output("Check binding page '%s'\n", pageId.c_str());
	std::map<std::string, BindingPage>::const_iterator itPage = m_bindingPages.find(pageId);

	if (itPage == m_bindingPages.end()) {
		Output("The above page is not present!\nSKIP!!!!!!!!!\n");
		return;
	}

	const BindingPage &page = (*itPage).second;
	Output("Bindings Groups [%lu]:\n", page.groups.size());
	for (auto &groupsPair : page.groups) {
		Output("  Group name '%s' contains [%lu]\n", groupsPair.first.c_str(), groupsPair.second.bindings.size());
		const BindingGroup &group = groupsPair.second;
		for (auto &bindsPair : group.bindings) {
			Output("    %s\n", bindsPair.first.c_str());
		}
	}
}

bool Input::PushInputFrame(InputFrame *frame)
{
	if (HasInputFrame(frame)) {
		return false;
	}

	if (frame == nullptr) {
		Error("Pushing a 'null' InputFrame!\n");
	} else {
	}
	m_inputFrames.push_back(frame);
	frame->onFrameAdded();
	return true;
}

bool Input::RemoveInputFrame(InputFrame *frame)
{
	auto it = std::find(m_inputFrames.begin(), m_inputFrames.end(), frame);
	if (it != m_inputFrames.end()) {
		m_inputFrames.erase(it);
		frame->m_active = false;
		frame->onFrameRemoved();
		return true;
	}
	return false;
}

template <class MapValueType>
int countPrefix(const std::map<std::string, MapValueType> &map, const std::string &search_for) {
	typename std::map<std::string, MapValueType>::const_iterator iter = map.lower_bound(search_for);
	int matches = 0;
	while (iter != map.end()) {
		const std::string &key = iter->first;
		if (key.compare(0, search_for.size(), search_for) == 0) {
			matches++;
		}
		iter++;
	}
	return matches;
}

KeyBindings::ActionBinding *Input::AddActionBinding(std::string &id, BindingGroup &group, KeyBindings::ActionBinding binding)
{
	int count = countPrefix<KeyBindings::ActionBinding>(m_actionBindings, id);

	if (count != 0) {
		#ifndef NDEBUG
			Output("HINT: Binding '%s' is used more than once\n", id.c_str());
		#endif // NDEBUG
		id += "_" + std::to_string(count);
	}

	// throw an error if we attempt to bind an action onto an already-bound axis in the same group.
	if (group.bindings.count(id) && group.bindings[id] != BindingGroup::EntryType::ACTION) {
		Error("Attempt to bind already-registered axis '%s' as an action on the same group.\n", id.c_str());
	}

	group.bindings[id] = BindingGroup::EntryType::ACTION;

	// Load from the config
	std::string config_str = GameConfSingleton::getInstance().String(id.c_str());
	if (config_str.length() > 0) binding.SetFromString(config_str);

	m_actionBindings[id] = binding;

	return &m_actionBindings[id];
}

KeyBindings::AxisBinding *Input::AddAxisBinding(std::string &id, BindingGroup &group, KeyBindings::AxisBinding binding)
{
	int count = countPrefix<KeyBindings::AxisBinding>(m_axisBindings, id);

	if (count != 0) {
		#ifndef NDEBUG
			Output("HINT: Binding '%s' is used more than once\n", id.c_str());
		#endif // NDEBUG
		id += "_" + std::to_string(count);
	}

	// throw an error if we attempt to bind an axis onto an already-bound action in the same group.
	if (group.bindings.count(id) && group.bindings[id] != BindingGroup::EntryType::AXIS) {
		Error("Attempt to bind already-registered action '%s' as an axis on the same group.\n", id.c_str());
	}

	group.bindings[id] = BindingGroup::EntryType::AXIS;

	// Load from the config
	std::string config_str = GameConfSingleton::getInstance().String(id.c_str());
	if (config_str.length() > 0) binding.SetFromString(config_str);

	m_axisBindings[id] = binding;

	return &m_axisBindings[id];
}

KeyBindings::WheelBinding *Input::AddWheelBinding(std::string &id, BindingGroup &group, KeyBindings::WheelBinding binding)
{
	int count = countPrefix<KeyBindings::WheelBinding>(m_wheelBindings, id);

	if (count != 0) {
		#ifndef NDEBUG
			Output("HINT: Binding '%s' is used more than once\n", id.c_str());
		#endif // NDEBUG
		id += "_" + std::to_string(count);
	}

	// throw an error if we attempt to bind an axis onto an already-bound action in the same group.
	if (group.bindings.count(id) && group.bindings[id] != BindingGroup::EntryType::WHEEL) {
		Error("Attempt to bind already-registered action '%s' as a wheel on the same group.\n", id.c_str());
	}

	group.bindings[id] = BindingGroup::EntryType::WHEEL;

	m_wheelBindings[id] = binding;

	return &m_wheelBindings[id];
}

void Input::HandleSDLEvent(const SDL_Event &event)
{
	PROFILE_SCOPED()
	switch (event.type) {
	case SDL_KEYDOWN:
		m_keyState[event.key.keysym.sym] = true;
		m_keyModState = event.key.keysym.mod;
		onKeyPress.emit(event.key.keysym);
		break;
	case SDL_KEYUP:
		m_keyState[event.key.keysym.sym] = false;
		m_keyModState = event.key.keysym.mod;
		onKeyRelease.emit(event.key.keysym);
		break;
	case SDL_MOUSEBUTTONDOWN:
		if (event.button.button < COUNTOF(m_mouseButton)) {
			m_mouseButton[event.button.button] = 1;
			onMouseButtonDown.emit(event.button.button,
				event.button.x, event.button.y);
		}
		break;
	case SDL_MOUSEBUTTONUP:
		if (event.button.button < COUNTOF(m_mouseButton)) {
			m_mouseButton[event.button.button] = 0;
			onMouseButtonUp.emit(event.button.button,
				event.button.x, event.button.y);
		}
		break;
	case SDL_MOUSEWHEEL:
		onMouseWheel.emit(event.wheel.y > 0); // true = up
		break;
	case SDL_MOUSEMOTION:
		m_mouseMotion[0] += event.motion.xrel;
		m_mouseMotion[1] += event.motion.yrel;
		break;
	case SDL_JOYAXISMOTION:
		if (!m_joysticks[event.jaxis.which].joystick)
			break;
		if (event.jaxis.value == -32768)
			m_joysticks[event.jaxis.which].axes[event.jaxis.axis] = 1.f;
		else
			m_joysticks[event.jaxis.which].axes[event.jaxis.axis] = -event.jaxis.value / 32767.f;
		break;
	case SDL_JOYBUTTONUP:
	case SDL_JOYBUTTONDOWN:
		if (!m_joysticks[event.jaxis.which].joystick)
			break;
		m_joysticks[event.jbutton.which].buttons[event.jbutton.button] = event.jbutton.state != 0;
		break;
	case SDL_JOYHATMOTION:
		if (!m_joysticks[event.jaxis.which].joystick)
			break;
		m_joysticks[event.jhat.which].hats[event.jhat.hat] = event.jhat.value;
		break;
	}

	//Output("ProcessSDLEvent of InputFrames\n");
	for (auto it = m_inputFrames.rbegin(); it != m_inputFrames.rend(); it++) {
		auto *inputFrame = *it;
		auto resp = inputFrame->ProcessSDLEvent(event);
		if (resp == InputResponse::MATCHED) break;
	}
}

void Input::InitJoysticks()
{
	int joy_count = SDL_NumJoysticks();
	Output("Initializing joystick subsystem.\n");
	for (int n = 0; n < joy_count; n++) {
		JoystickState state;

		state.joystick = SDL_JoystickOpen(n);
		if (!state.joystick) {
			Warning("SDL_JoystickOpen(%i): %s\n", n, SDL_GetError());
			continue;
		}

		state.guid = SDL_JoystickGetGUID(state.joystick);
		state.axes.resize(SDL_JoystickNumAxes(state.joystick));
		state.buttons.resize(SDL_JoystickNumButtons(state.joystick));
		state.hats.resize(SDL_JoystickNumHats(state.joystick));

		std::array<char, 33> joystickGUIDName;
		SDL_JoystickGetGUIDString(state.guid, joystickGUIDName.data(), joystickGUIDName.size());
		Output("Found joystick '%s' (GUID: %s)\n", SDL_JoystickName(state.joystick), joystickGUIDName.data());
		Output("  - %ld axes, %ld buttons, %ld hats\n", state.axes.size(), state.buttons.size(), state.hats.size());

		SDL_JoystickID joyID = SDL_JoystickInstanceID(state.joystick);
		m_joysticks[joyID] = state;
	}
}

void Input::FindAndEraseEntryInGroups(const std::string &id)
{
	for (auto &page : m_bindingPages) {
		bool jumpOut = false;
		BindingPage &bindPage = page.second;
		for (auto &group : bindPage.groups) {
			BindingGroup &bindGroup = group.second;

			std::map<std::string, BindingGroup::EntryType>::iterator found = bindGroup.bindings.find(id);
			if (found != bindGroup.bindings.end()) {
				bindGroup.bindings.erase(found);
				if (bindGroup.bindings.empty()) {
					bindPage.groups.erase(group.first);
				}
				if (bindPage.groups.empty()) {
					m_bindingPages.erase(page.first);
				}
				return;
			}
		}
	}
}

std::string Input::JoystickName(int joystick)
{
	return std::string(SDL_JoystickName(m_joysticks[joystick].joystick));
}

std::string Input::JoystickGUIDString(int joystick)
{
	const int guidBufferLen = 33; // as documented by SDL
	char guidBuffer[guidBufferLen];

	SDL_JoystickGetGUIDString(m_joysticks[joystick].guid, guidBuffer, guidBufferLen);
	return std::string(guidBuffer);
}

// conveniance version of JoystickFromGUID below that handles the string mangling.
int Input::JoystickFromGUIDString(const std::string &guid)
{
	return JoystickFromGUIDString(guid.c_str());
}

// conveniance version of JoystickFromGUID below that handles the string mangling.
int Input::JoystickFromGUIDString(const char *guid)
{
	return JoystickFromGUID(SDL_JoystickGetGUIDFromString(guid));
}

// return the internal ID of the stated joystick guid.
// returns -1 if we couldn't find the joystick in question.
int Input::JoystickFromGUID(SDL_JoystickGUID guid)
{
	const int guidLength = 16; // as defined
	for (std::map<SDL_JoystickID, JoystickState>::iterator stick = m_joysticks.begin(); stick != m_joysticks.end(); ++stick) {
		JoystickState &state = stick->second;
		if (0 == memcmp(state.guid.data, guid.data, guidLength)) {
			return static_cast<int>(stick->first);
		}
	}
	return -1;
}

SDL_JoystickGUID Input::JoystickGUID(int joystick)
{
	return m_joysticks[joystick].guid;
}

float Input::GetMoveSpeedShiftModifier()
{
	// Suggestion: make x1000 speed on pressing both keys?
	if (KeyState(SDLK_LSHIFT)) return 100.f;
	if (KeyState(SDLK_RSHIFT)) return 10.f;
	return 1;
}

int Input::JoystickButtonState(int joystick, int button)
{
	if (!m_joystickEnabled) return 0;
	if (joystick < 0 || joystick >= int(m_joysticks.size()))
		return 0;

	if (button < 0 || button >= int(m_joysticks[joystick].buttons.size()))
		return 0;

	return m_joysticks[joystick].buttons[button];
}

int Input::JoystickHatState(int joystick, int hat)
{
	if (!m_joystickEnabled) return 0;
	if (joystick < 0 || joystick >= int(m_joysticks.size()))
		return 0;

	if (hat < 0 || hat >= int(m_joysticks[joystick].hats.size()))
		return 0;

	return m_joysticks[joystick].hats[hat];
}

float Input::JoystickAxisState(int joystick, int axis)
{
	if (!m_joystickEnabled) return 0;
	if (joystick < 0 || joystick >= int(m_joysticks.size()))
		return 0;

	if (axis < 0 || axis >= int(m_joysticks[joystick].axes.size()))
		return 0;

	return m_joysticks[joystick].axes[axis];
}
