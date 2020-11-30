#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <SDL_joystick.h>
#include <SDL_events.h>

#include <map>
#include <string>
#include <vector>

class JoyStick
{
public:
	JoyStick(const JoyStick& rhs) = delete;
	JoyStick& operator=(const JoyStick& rhs) = delete;

	JoyStick();
	~JoyStick();

	void InitGame();

	int JoystickButtonState(int joystick, int button);
	int JoystickHatState(int joystick, int hat);
	float JoystickAxisState(int joystick, int axis);

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

	void HandleSDLEvent(const SDL_Event &event);
private:
	std::map<SDL_JoystickID, JoystickState> m_joysticks;
};

#endif // JOYSTICK_H
