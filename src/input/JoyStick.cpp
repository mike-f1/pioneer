#include "JoyStick.h"

#include "libs/utils.h"

JoyStick::JoyStick()
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

JoyStick::~JoyStick()
{
}

void JoyStick::InitGame()
{
	for (std::map<SDL_JoystickID, JoystickState>::iterator stick = m_joysticks.begin(); stick != m_joysticks.end(); ++stick) {
		JoystickState &state = stick->second;
		std::fill(state.buttons.begin(), state.buttons.end(), false);
		std::fill(state.hats.begin(), state.hats.end(), 0);
		std::fill(state.axes.begin(), state.axes.end(), 0.f);
	}
}

std::string JoyStick::JoystickName(int joystick)
{
	return std::string(SDL_JoystickName(m_joysticks[joystick].joystick));
}

std::string JoyStick::JoystickGUIDString(int joystick)
{
	const int guidBufferLen = 33; // as documented by SDL
	char guidBuffer[guidBufferLen];

	SDL_JoystickGetGUIDString(m_joysticks[joystick].guid, guidBuffer, guidBufferLen);
	return std::string(guidBuffer);
}

// conveniance version of JoystickFromGUID below that handles the string mangling.
int JoyStick::JoystickFromGUIDString(const std::string &guid)
{
	return JoystickFromGUIDString(guid.c_str());
}

// conveniance version of JoystickFromGUID below that handles the string mangling.
int JoyStick::JoystickFromGUIDString(const char *guid)
{
	return JoystickFromGUID(SDL_JoystickGetGUIDFromString(guid));
}

// return the internal ID of the stated joystick guid.
// returns -1 if we couldn't find the joystick in question.
int JoyStick::JoystickFromGUID(SDL_JoystickGUID guid)
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

SDL_JoystickGUID JoyStick::JoystickGUID(int joystick)
{
	return m_joysticks[joystick].guid;
}

int JoyStick::JoystickButtonState(int joystick, int button)
{
	if (joystick < 0 || joystick >= int(m_joysticks.size()))
		return 0;

	if (button < 0 || button >= int(m_joysticks[joystick].buttons.size()))
		return 0;

	return m_joysticks[joystick].buttons[button];
}

int JoyStick::JoystickHatState(int joystick, int hat)
{
	if (joystick < 0 || joystick >= int(m_joysticks.size()))
		return 0;

	if (hat < 0 || hat >= int(m_joysticks[joystick].hats.size()))
		return 0;

	return m_joysticks[joystick].hats[hat];
}

float JoyStick::JoystickAxisState(int joystick, int axis)
{
	if (joystick < 0 || joystick >= int(m_joysticks.size()))
		return 0;

	if (axis < 0 || axis >= int(m_joysticks[joystick].axes.size()))
		return 0;

	return m_joysticks[joystick].axes[axis];
}

void JoyStick::HandleSDLEvent(const SDL_Event &event)
{
	switch (event.type) {
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
}
