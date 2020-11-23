// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Input.h"

#include "GameConfig.h"
#include "GameConfSingleton.h"
#include "InputFrame.h"
#include "BindingContainer.h"

#include "libs/utils.h"
#include "profiler/Profiler.h"

std::string speedModifier = "SpeedModifier";

InputFrameStatusTicket::InputFrameStatusTicket(const std::vector<InputFrame *> &inputFrames)
{
	std::for_each(begin(inputFrames), end(inputFrames), [this](InputFrame *iframe) {
		m_statuses[iframe] = iframe->IsActive();
	});
}

InputFrameStatusTicket::~InputFrameStatusTicket()
{
	std::for_each(begin(m_statuses), end(m_statuses), [](std::pair<InputFrame *, bool> old_status) {
		old_status.first->SetActive(old_status.second);
	});
}

Input::Input() :
	m_keyJustPressed(0),
	m_keyModStateUnified(SDL_Keymod::KMOD_NONE),
	m_wheelState(KeyBindings::WheelDirection::NONE)
{
	GameConfig &config = GameConfSingleton::getInstance();

	m_joystickEnabled = (config.Int("EnableJoystick")) ? true : false;
	m_mouseYInvert = (config.Int("InvertMouseY")) ? true : false;

	//m_bindingContainers.reserve(8);

	InitJoysticks();

	RegisterInputBindings();
}

void Input::InitGame()
{
	//reset input states
	m_keyState.clear();
	m_keyModStateUnified = KMOD_NONE;
	m_mouseButton.fill(false);
	m_mouseMotion.fill(0);

	for (std::map<SDL_JoystickID, JoystickState>::iterator stick = m_joysticks.begin(); stick != m_joysticks.end(); ++stick) {
		JoystickState &state = stick->second;
		std::fill(state.buttons.begin(), state.buttons.end(), false);
		std::fill(state.hats.begin(), state.hats.end(), 0);
		std::fill(state.axes.begin(), state.axes.end(), 0.f);
	}
}

void Input::TerminateGame()
{
}

void Input::RegisterInputBindings()
{
	using namespace KeyBindings;

	auto &page = GetBindingPage("General");
	auto &group = page.GetBindingGroup("Miscellaneous");

	m_speedModifier = GetActionBinding(speedModifier);
	if (!m_speedModifier) {
		m_speedModifier = AddActionBinding(speedModifier, group, ActionBinding(SDLK_CAPSLOCK));
	}
	m_speedModifier->SetBTrait(BehaviourMod::DISALLOW_MODIFIER | BehaviourMod::ALLOW_KEYBOARD_ONLY);
}

#ifdef DEBUG_DUMP_PAGES
void Input::DebugDumpPage(const std::string &pageId)
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
#endif // DEBUG_DUMP_PAGES

RefCountedPtr<BindingContainer> Input::CreateOrShareBindContainer(const std::string &name, InputFrame *iframe)
{
	Output("Input::CreateOrShareBindContainer(const std::string &%s) ...", name.c_str());
	m_inputFrames.push_back(iframe);
	auto iter = std::find_if(begin(m_bindingContainers), end(m_bindingContainers), [&name](RefCountedPtr<BindingContainer> &bindCont) {
		return (name == bindCont->GetName());
	});
	if (iter != m_bindingContainers.end()) {
		Output("Found, share it (num = %i!)\n", (*iter)->GetRefCount());
		return *iter;
	} else {
		Output("NOT found, create it!\n");
		auto new_bindcont = RefCountedPtr<BindingContainer>(new BindingContainer(name));
		m_bindingContainers.push_back(new_bindcont);
		Output("[Ref count is: %i\n", m_bindingContainers.back()->GetRefCount());
		return new_bindcont;
	}
}

bool Input::RemoveBindingContainer(InputFrame *iframe)
{
	Output("Input::RemoveBindingContainer() ...\n");
	bool removed = false;
	auto it = std::find(m_inputFrames.begin(), m_inputFrames.end(), iframe);
	if (it != m_inputFrames.end()) {
		m_inputFrames.erase(it);
		removed = true;
	}
	Output("%s (m_bindingContainers.size() = %lu)\n", removed ? "found" : "not found", m_bindingContainers.size());
	std::for_each(m_bindingContainers.begin(), m_bindingContainers.end(), [&](RefCountedPtr<BindingContainer> &bindCont) {
		Output("\t%s => %i\n", bindCont->GetName().c_str(), bindCont->GetRefCount());
	});
	/*

		std::deque<RefCountedPtr<Widget>>::iterator i;
		for (i = m_widgets.begin(); i != m_widgets.end(); ++i)
			if ((*i).Get() == widget) break;
		if (i == m_widgets.end())
			return;

		widget->Detach();
		m_widgets.erase(i);
*/

	Output("During:\n");
	for (auto iter = m_bindingContainers.begin(); iter != m_bindingContainers.end(); ++iter) {
		//Output("\t%s => %i\n", (*iter)->GetName().c_str(), (*iter)->GetRefCount());
		if ((*iter)->GetRefCount() == 1) {
			Output("\t%s is unique, deleting...\n", (*iter)->GetName().c_str());
			(*iter)->DecRefCount();
			iter = m_bindingContainers.erase(iter);
		}
	}

/*
	m_bindingContainers.erase(std::remove(m_bindingContainers.begin(), m_bindingContainers.end(), [](RefCountedPtr<BindingContainer> &bindCont) {
		return (bindCont->GetRefCount() == 1);
	}));
*/
/*
	Output("After:\n");
	std::for_each(m_bindingContainers.begin(), m_bindingContainers.end(), [&](RefCountedPtr<BindingContainer> &bindCont) {
		Output("\t%s => %i\n", bindCont->GetName().c_str(), bindCont->GetRefCount());
	});
*/
	Output("...exiting\n");
	return removed;
}

bool  Input::HasBindingContainer(std::string &name)
{
	for (RefCountedPtr<BindingContainer> &bindCont : m_bindingContainers) {
		if (bindCont->GetName() == name) return true;
	}
	return false;
}

std::unique_ptr<InputFrameStatusTicket> Input::DisableAllInputFrameExcept(InputFrame *current)
{
	std::unique_ptr<InputFrameStatusTicket> ifst(new InputFrameStatusTicket(m_inputFrames));

	std::for_each(begin(m_inputFrames), end(m_inputFrames), [&current](InputFrame *iframe) {
		if (iframe != current) iframe->SetActive(false);
	});
	return ifst;
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
		++iter;
	}
	return matches;
}

KeyBindings::ActionBinding *Input::AddActionBinding(std::string &id, BindingGroup &group, KeyBindings::ActionBinding binding)
{
	// Load from the config
	std::string config_str = GameConfSingleton::getInstance().String(id.c_str());
	if (config_str.length() > 0) binding.SetFromString(config_str);

	int occurences = countPrefix<KeyBindings::ActionBinding>(m_actionBindings, id);

	if (occurences != 0) {
		#ifndef NDEBUG
			Output("HINT: Binding '%s' is used more than once\n", id.c_str());
		#endif // NDEBUG
		id += "_" + std::to_string(occurences);
	}

	// throw an error if we attempt to bind an action onto an already-bound axis in the same group.
	if (group.bindings.count(id) && group.bindings[id] != BindingGroup::EntryType::ACTION) {
		Error("Attempt to bind already-registered axis '%s' as an action on the same group.\n", id.c_str());
	}

	group.bindings[id] = BindingGroup::EntryType::ACTION;

	m_actionBindings[id] = binding;

	return &m_actionBindings[id];
}

KeyBindings::AxisBinding *Input::AddAxisBinding(std::string &id, BindingGroup &group, KeyBindings::AxisBinding binding)
{
	// Load from the config
	std::string config_str = GameConfSingleton::getInstance().String(id.c_str());
	if (config_str.length() > 0) binding.SetFromString(config_str);

	int occurences = countPrefix<KeyBindings::AxisBinding>(m_axisBindings, id);

	if (occurences != 0) {
		#ifndef NDEBUG
			Output("HINT: Binding '%s' is used more than once\n", id.c_str());
		#endif // NDEBUG
		id += "_" + std::to_string(occurences);
	}

	// throw an error if we attempt to bind an axis onto an already-bound action in the same group.
	if (group.bindings.count(id) && group.bindings[id] != BindingGroup::EntryType::AXIS) {
		Error("Attempt to bind already-registered action '%s' as an axis on the same group.\n", id.c_str());
	}

	group.bindings[id] = BindingGroup::EntryType::AXIS;

	m_axisBindings[id] = binding;

	return &m_axisBindings[id];
}

std::tuple<bool, int, int> Input::GetMouseMotion(MouseMotionBehaviour mmb)
{
	switch (mmb) {
		case MouseMotionBehaviour::Rotate:
			if (m_mouseButton[SDL_BUTTON_MIDDLE]) return std::make_tuple(true, m_mouseMotion[0], m_mouseMotion[1]);
		break;
		case MouseMotionBehaviour::DriveShip:
			if (m_mouseButton[SDL_BUTTON_RIGHT]) return std::make_tuple(true, m_mouseMotion[0], m_mouseMotion[1]);
		break;
		case MouseMotionBehaviour::Fire:
			if (m_mouseButton[SDL_BUTTON_LEFT] && m_mouseButton[SDL_BUTTON_RIGHT]) return std::make_tuple(true, m_mouseMotion[0], m_mouseMotion[1]);
		break;
		case MouseMotionBehaviour::Select:
			if (m_mouseButton[SDL_BUTTON_LEFT]) return std::make_tuple(true, m_mouseMotion[0], m_mouseMotion[1]);
		break;
	}
	return std::make_tuple(false, 0, 0);
}

float Input::GetMoveSpeedShiftModifier() const
{
	float speed = 1.0f;
	if (m_speedModifier->GetBinding(0).IsActive()) speed *= 5.f;
	if (m_speedModifier->GetBinding(1).IsActive()) speed *= 50.f;
	return speed;
}

void Input::HandleSDLEvent(const SDL_Event &event)
{
	PROFILE_SCOPED()
	switch (event.type) {
	case SDL_KEYDOWN:
		m_keyJustPressed++;
		m_keyState[event.key.keysym.sym] = true;
		m_keyModStateUnified = KeyBindings::KeymodUnifyLR(SDL_Keymod(event.key.keysym.mod));
		break;
	case SDL_KEYUP:
		m_keyState[event.key.keysym.sym] = false;
		m_keyModStateUnified = KeyBindings::KeymodUnifyLR(SDL_Keymod(event.key.keysym.mod));
		break;
	case SDL_MOUSEBUTTONDOWN:
		if (event.button.button < m_mouseButton.size()) {
			m_mouseButton[event.button.button] = true;
		}
		break;
	case SDL_MOUSEBUTTONUP:
		if (event.button.button < m_mouseButton.size()) {
			m_mouseButton[event.button.button] = false;
		}
		break;
	case SDL_MOUSEWHEEL: {
		if (event.wheel.x < 0) {
			m_wheelState = KeyBindings::WheelDirection::LEFT;
		} else if (event.wheel.x > 0) {
			m_wheelState = KeyBindings::WheelDirection::RIGHT;
		}
		// This is "prioritizing" up/down over left/right
		if (event.wheel.y < 0) {
			m_wheelState = KeyBindings::WheelDirection::DOWN;
		} else if (event.wheel.y > 0) {
			m_wheelState = KeyBindings::WheelDirection::UP;
		}
	}
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

	if (m_speedModifier) m_speedModifier->CheckSDLEventAndDispatch(event);

	//Output("ProcessSDLEvent of InputFrames\n");
	for (auto it = m_inputFrames.rbegin(); it != m_inputFrames.rend(); it++) {
		auto *inputFrame = (*it);
		//Output("\t%s\n", bindingCont->m_name.c_str());
		auto resp = inputFrame->ProcessSDLEvent(event);
		if (resp == KeyBindings::InputResponse::MATCHED) break;
	}
}

void Input::FindAndEraseEntryInPagesAndGroups(const std::string &id)
{
	for (auto &page : m_bindingPages) {
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
