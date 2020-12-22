// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Input.h"

#include "GameConfig.h"
#include "GameConfSingleton.h"
#include "InputFrame.h"
#include "InputFrameStatusTicket.h"
#include "BindingContainer.h"
#include "JoyStick.h"

#include "libs/utils.h"
#include "profiler/Profiler.h"

#include <stdexcept>

std::string speedModifier = "SpeedModifier";

Input::Input() :
	m_keyJustPressed(0),
	m_keyModStateUnified(SDL_Keymod::KMOD_NONE),
	m_wheelState(KeyBindings::WheelDirection::NONE)
{
	GameConfig &config = GameConfSingleton::getInstance();

	m_joystickEnabled = (config.Int("EnableJoystick")) ? true : false;
	m_mouseYInvert = (config.Int("InvertMouseY")) ? true : false;

	m_bindingContainers.reserve(10);

	m_joystick = std::make_unique<JoyStick>();

	RegisterInputBindings();
}

void Input::InitGame()
{
	Output("Input::InitGame()\n");
	//reset input states
	m_keyState.clear();
	m_keyModStateUnified = KMOD_NONE;
	m_mouseButton.fill(false);
	m_mouseMotion.fill(0);

	if (m_joystick) m_joystick->InitGame();
}

void Input::TerminateGame()
{
	Output("Input::TerminateGame()\n");
	m_generalPanRotateZoom.reset();
	PurgeBindingContainers();
}

void Input::InitializeInputBindings(std::vector<std::function<void(void)>> &bindings_registerer)
{
	static bool initialized = false;
	if (initialized) throw std::logic_error {"InitializeInputBindings should be called only once!"};
	// create a shared InputFrame for standard movement and initialize it
	// after Input ctor has been called due to InputFrame needs
	using namespace KeyBindings;
	using namespace std::placeholders;

	m_generalPanRotateZoom = std::make_unique<InputFrame>("GeneralPanRotateZoom");

	auto &page = GetBindingPage("General");
	auto &group = page.GetBindingGroup("GenViewControl");

	m_generalPanRotateZoom->AddAxisBinding("BindMapViewShiftForwardBackward", group, AxisBinding(SDLK_r, SDLK_f));
	m_generalPanRotateZoom->AddAxisBinding("BindMapViewShiftLeftRight", group, AxisBinding(SDLK_a, SDLK_d));
	m_generalPanRotateZoom->AddAxisBinding("BindMapViewShiftUpDown", group, AxisBinding(SDLK_w, SDLK_s));

	m_generalPanRotateZoom->AddAxisBinding("BindMapViewZoom", group, AxisBinding(SDLK_PLUS, SDLK_MINUS));

	m_generalPanRotateZoom->AddAxisBinding("BindMapViewRotateLeftRight", group, AxisBinding(SDLK_RIGHT, SDLK_LEFT));
	m_generalPanRotateZoom->AddAxisBinding("BindMapViewRotateUpDown", group, AxisBinding(SDLK_DOWN, SDLK_UP));

	m_generalPanRotateZoom->LockInsertion();

	std::for_each(begin(bindings_registerer), end(bindings_registerer), [](auto &fun) { fun(); });
	initialized = true;
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
	m_inputFrames.push_back(iframe);
	auto iter = std::find_if(begin(m_bindingContainers), end(m_bindingContainers), [&name](RefCountedPtr<BindingContainer> &bindCont) {
		return (name == bindCont->GetName());
	});
	if (iter != m_bindingContainers.end()) {
		return *iter;
	} else {
		auto new_bindcont = RefCountedPtr<BindingContainer>(new BindingContainer(name));
		m_bindingContainers.push_back(new_bindcont);
		return new_bindcont;
	}
}

bool Input::RemoveBindingContainer(InputFrame *iframe)
{
	bool removed = false;
	if (auto it = std::find(m_inputFrames.begin(), m_inputFrames.end(), iframe); it != m_inputFrames.end()) {
		m_inputFrames.erase(it);
		removed = true;
	}
	PurgeBindingContainers();
	return removed;
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

void Input::SetJoystickEnabled(bool state)
{
	m_joystickEnabled = state;
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
	case SDL_JOYBUTTONUP:
	case SDL_JOYBUTTONDOWN:
	case SDL_JOYHATMOTION:
		if (m_joystickEnabled) m_joystick->HandleSDLEvent(event);
	break;
	// Filter non-(yet-)bindable events
	case SDL_WINDOWEVENT:
	case SDL_DROPFILE:
	case SDL_DROPTEXT:
	case SDL_DROPBEGIN:
	case SDL_DROPCOMPLETE:
	case SDL_AUDIODEVICEADDED:
	case SDL_AUDIODEVICEREMOVED:
	case SDL_SYSWMEVENT:
		return;
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

void Input::PurgeBindingContainers()
{
	m_bindingContainers.erase(std::remove_if(m_bindingContainers.begin(), m_bindingContainers.end(), [](RefCountedPtr<BindingContainer> &bindCont) {
		return bindCont.Unique();
	}), m_bindingContainers.end());
}
