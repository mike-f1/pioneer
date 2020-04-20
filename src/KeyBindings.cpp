// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "KeyBindings.h"
#include "GameConfig.h"
#include "GameConfSingleton.h"
#include "Lang.h"
#include "Pi.h"
#include "StringF.h"

#include <sstream>
#include <string>

#include "profiler/Profiler.h"

static bool m_disableBindings = 0;

namespace KeyBindings {

	SDL_Keymod KeymodUnifyLR(SDL_Keymod mod)
	{
		unsigned imod = mod;
		if (imod & KMOD_CTRL) {
			imod |= KMOD_CTRL;
		}
		if (imod & KMOD_SHIFT) {
			imod |= KMOD_SHIFT;
		}
		if (imod & KMOD_ALT) {
			imod |= KMOD_ALT;
		}
		if (imod & KMOD_GUI) {
			imod |= KMOD_GUI;
		}
		// Mask with used modifiers:
		imod &= (KMOD_CTRL | KMOD_SHIFT | KMOD_ALT | KMOD_GUI);

		return static_cast<SDL_Keymod>(imod);
	}

	Modifiers::Modifiers(SDL_Keymod kmod)
	{
		m_mod = KeymodUnifyLR(kmod);
	}

	Modifiers::Modifiers(const std::string &str)
	{
		size_t pos = str.find("Mod");
		if (pos != std::string::npos) {
			unsigned uns = std::stoul(str.substr(pos));
			m_mod = KeymodUnifyLR(SDL_Keymod(uns));
		} else {
			m_mod = KMOD_NONE;
		}
	}

	std::string Modifiers::ToString() const
	{
		return ("Mod" + std::to_string(m_mod));
	}

	std::string Modifiers::Description() const
	{
		std::ostringstream oss;
		if (m_mod & KMOD_SHIFT) oss << Lang::SHIFT << " + ";
		if (m_mod & KMOD_CTRL) oss << Lang::CTRL << " + ";
		if (m_mod & KMOD_ALT) oss << Lang::ALT << " + ";
		if (m_mod & KMOD_GUI) oss << Lang::META << " + ";

		return oss.str();
	}

	bool Modifiers::Matches(const SDL_Keymod &mod) const
	{
		SDL_Keymod mod_unified = KeymodUnifyLR(mod);
		return mod_unified == m_mod;
	}

	bool Modifiers::IsActive() const
	{
		return m_mod == Pi::input.KeyModStateUnified();
	}

	int WheelDirectionToInt(const WheelDirection wd)
	{
		switch (wd) {
		case WheelDirection::UP: return 0;
		case WheelDirection::DOWN: return 1;
		case WheelDirection::LEFT: return 2;
		case WheelDirection::RIGHT: return 3;
		}
		return -1000;
	}

	char WheelDirectionToChar(const WheelDirection wd)
	{
		switch (wd) {
		case WheelDirection::UP: return '0';
		case WheelDirection::DOWN: return '1';
		case WheelDirection::LEFT: return '2';
		case WheelDirection::RIGHT: return '3';
		}
		assert(0 && "This should not return a 'WheelDirection::NONE'...");
		return '0';
	}

	WheelDirection WheelDirectionFromChar(const char c)
	{
		switch (c) {
		case '0': return WheelDirection::UP;
		case '1': return WheelDirection::DOWN;
		case '2': return WheelDirection::LEFT;
		case '3': return WheelDirection::RIGHT;
		}
		return WheelDirection::NONE;
	}

	std::string GetMouseWheelDescription(WheelDirection dir)
	{
		std::ostringstream oss;
		oss << Lang::MOUSE_WHEEL;
		switch (dir) {
		case WheelDirection::UP: oss << " " << Lang::UP; break;
		case WheelDirection::DOWN: oss << " " << Lang::DOWN; break;
		case WheelDirection::LEFT: oss << " " << Lang::LEFT; break;
		case WheelDirection::RIGHT: oss << " " << Lang::RIGHT; break;
		default:
			assert(0 && "...what a wheel! :P");
		}
		return oss.str();
	}

	bool KeyBinding::IsActive() const
	{
		if (!m_mod.IsActive()) return false;
		switch (type) {
		case BindType::BINDING_DISABLED:
			return false;
		case BindType::KEYBOARD_KEY: {
			if (!Pi::input.KeyState(u.keyboard.key)) {
				return false;
			}
			return m_mod.IsActive();
		}
		case BindType::JOYSTICK_BUTTON: {
			return Pi::input.JoystickButtonState(u.joystickButton.joystick, u.joystickButton.button) != 0;
		}
		case BindType::JOYSTICK_HAT: {
			// SDL_HAT generates diagonal directions by ORing two cardinal directions.
			int hatState = Pi::input.JoystickHatState(u.joystickHat.joystick, u.joystickHat.hat);
			return (hatState & u.joystickHat.direction) == u.joystickHat.direction;
		}
		case BindType::MOUSE_WHEEL: {
			WheelDirection wheelDir = Pi::input.GetWheelState();
			if (wheelDir != u.mouseWheel.dir) return false;
			return true;
		}
		default:
			abort();
		}
	}

	bool KeyBinding::Matches(const SDL_Keysym &sym) const
	{
		return (type == BindType::KEYBOARD_KEY) &&
			(sym.sym == u.keyboard.key) &&
			(m_mod.Matches(SDL_Keymod(sym.mod)));
	}

	bool KeyBinding::Matches(const SDL_JoyButtonEvent &joy) const
	{
		return (type == BindType::JOYSTICK_BUTTON) &&
			(joy.which == u.joystickButton.joystick) &&
			(joy.button == u.joystickButton.button) &&
			(m_mod.IsActive());
	}

	bool KeyBinding::Matches(const SDL_JoyHatEvent &joy) const
	{
		return (type == BindType::JOYSTICK_HAT) &&
			(joy.which == u.joystickHat.joystick) &&
			(joy.hat == u.joystickHat.hat) &&
			(joy.value == u.joystickHat.direction) &&
			(m_mod.IsActive());
	}

	bool KeyBinding::Matches(const SDL_MouseWheelEvent &mwe) const
	{
		return (type == BindType::MOUSE_WHEEL) && (
			((mwe.y < 0) && (u.mouseWheel.dir == WheelDirection::DOWN)) ||
			((mwe.y > 0) && (u.mouseWheel.dir == WheelDirection::UP)) ||
			((mwe.x < 0) && (u.mouseWheel.dir == WheelDirection::LEFT)) ||
			((mwe.x > 0) && (u.mouseWheel.dir == WheelDirection::RIGHT))
			) && (m_mod.IsActive());
	}

	std::string KeyBinding::Description() const
	{
		std::ostringstream oss;

		oss << m_mod.Description();

		switch (type) {
		case BindType::BINDING_DISABLED:
		break;
		case BindType::KEYBOARD_KEY: {
			oss << SDL_GetKeyName(u.keyboard.key);
		};
		break;
		case BindType::JOYSTICK_BUTTON: {
			oss << Pi::input.JoystickName(u.joystickButton.joystick);
			oss << Lang::BUTTON << int(u.joystickButton.button);
		};
		break;
		case BindType::JOYSTICK_HAT: {
			oss << Pi::input.JoystickName(u.joystickHat.joystick);
			oss << Lang::HAT << int(u.joystickHat.hat);
			oss << Lang::DIRECTION << int(u.joystickHat.direction);
		}
		break;
		case BindType::MOUSE_WHEEL: {
			oss << GetMouseWheelDescription(u.mouseWheel.dir);
		}
		break;
		default:
			assert(0 && "invalid binding type");
		}
		return oss.str();
	}

/**
 * In a C string pointed to by the string pointer pointed to by p, scan for
 * the character token tok, copying the bytes on the way to bufOut which is at most buflen long.
 *
 * returns true on success, returns false if the end of the input string was reached,
 *   or the buffer would be overfilled without encountering the token.
 *
 * upon return, the pointer pointed to by p will refer to the character AFTER the tok.
 */
	static bool ReadToTok(char tok, const char **p, char *bufOut, size_t buflen)
	{
		unsigned int idx;
		for (idx = 0; idx < buflen; idx++) {
			if (**p == '\0' || **p == tok) {
				break;
			}
			bufOut[idx] = *((*p)++);
		}
		// if, after that, we're not pointing at the tok, we must have hit
		// the terminal or run out of buffer.
		if (**p != tok) {
			return false;
		}
		// otherwise, skip over the tok.
		(*p)++;
		// if there is sufficient space in the buffer, NUL terminate.
		if (idx < buflen) {
			bufOut[idx] = '\0';
		}
		return true;
	}

/**
 * Example strings:
 *   Key55
 *   Joy{uuid}/Button2
 *   Joy{uuid}/Hat0Dir3
 */
	bool KeyBinding::FromString(const char *str, KeyBinding &kb)
	{
		const char *digits = "1234567890";
		const char *p = str;

		if (strcmp(p, "disabled") == 0) {
			kb.Clear();
		} else if (strncmp(p, "Key", 3) == 0) {
			kb.type = BindType::KEYBOARD_KEY;
			p += 3;

			kb.u.keyboard.key = SDL_Keycode(atoi(p));
			p += strspn(p, digits);

			if (strncmp(p, "Mod", 3) == 0) {
				p += 3;
				kb.m_mod = Modifiers(SDL_Keymod(atoi(p)));
			} else {
				kb.m_mod = Modifiers();
			}
		} else if (strncmp(p, "Joy", 3) == 0) {
			p += 3;

			const int JoyUUIDLength = 33;
			char joyUUIDBuf[JoyUUIDLength];

			// read the UUID
			if (!ReadToTok('/', &p, joyUUIDBuf, JoyUUIDLength)) {
				return false;
			}
			// force terminate
			joyUUIDBuf[JoyUUIDLength - 1] = '\0';
			// now, locate the internal ID.
			int joy = Pi::input.JoystickFromGUIDString(joyUUIDBuf);
			if (joy == -1) {
				return false;
			}
			if (strncmp(p, "Button", 6) == 0) {
				p += 6;
				kb.type = BindType::JOYSTICK_BUTTON;
				kb.u.joystickButton.joystick = joy;
				kb.u.joystickButton.button = atoi(p);
			} else if (strncmp(p, "Hat", 3) == 0) {
				p += 3;
				kb.type = BindType::JOYSTICK_HAT;
				kb.u.joystickHat.joystick = joy;
				kb.u.joystickHat.hat = atoi(p);
				p += strspn(p, digits);

				if (strncmp(p, "Dir", 3) != 0)
					return false;

				p += 3;
				kb.u.joystickHat.direction = atoi(p);
			} else {
				return false;
			}

			p += strspn(p, digits);
			if (strncmp(p, "Mod", 3) == 0) {
				p += 3;
				kb.m_mod = Modifiers(SDL_Keymod(atoi(p)));
			} else {
				kb.m_mod = Modifiers();
			}
		} else if (strncmp(p, "MWh", 3) == 0) {
			kb.type = BindType::MOUSE_WHEEL;
			p += 3;

			kb.u.mouseWheel.dir = WheelDirectionFromChar(*p);
			if (kb.u.mouseWheel.dir == WheelDirection::NONE) {
				return false;
			}

			p +=1;
			if (strncmp(p, "Mod", 3) == 0) {
				p += 3;
				kb.m_mod = Modifiers(SDL_Keymod(atoi(p)));
			} else {
				kb.m_mod = Modifiers();
			}
		}
		return true;
	}

	KeyBinding KeyBinding::FromString(const char *str)
	{
		KeyBinding kb;
		if (!KeyBinding::FromString(str, kb))
			kb.Clear();
		return kb;
	}

	KeyBinding::KeyBinding(const SDL_JoystickGUID &joystickGuid, Uint8 button, SDL_Keymod mod) :
		type(BindType::JOYSTICK_BUTTON),
		m_mod(mod)
	{
		u.joystickButton.joystick = Pi::input.JoystickFromGUID(joystickGuid);
		u.joystickButton.button = button;
	}

	KeyBinding::KeyBinding(const SDL_JoystickGUID &joystickGuid, Uint8 hat, Uint8 dir, SDL_Keymod mod) :
		type(BindType::JOYSTICK_HAT),
		m_mod(mod)
	{
		u.joystickHat.joystick = Pi::input.JoystickFromGUID(joystickGuid);
		u.joystickHat.hat = hat;
		u.joystickHat.direction = dir;
	}

	std::string KeyBinding::ToString() const
	{
		std::ostringstream oss;

		switch (type) {
		case BindType::BINDING_DISABLED:
			oss << "disabled";
		break;
		case BindType::KEYBOARD_KEY: {
				oss << "Key" << int(u.keyboard.key);
			}
		break;
		case BindType::JOYSTICK_BUTTON: {
				oss << "Joy" << Pi::input.JoystickGUIDString(u.joystickButton.joystick);
				oss << "/Button" << int(u.joystickButton.button);
		}
		break;
		case BindType::JOYSTICK_HAT: {
				oss << "Joy" << Pi::input.JoystickGUIDString(u.joystickButton.joystick);
				oss << "/Hat" << int(u.joystickHat.hat);
				oss << "Dir" << int(u.joystickHat.direction);
		}
		break;
		case BindType::MOUSE_WHEEL: {
				oss << "MWh" << WheelDirectionToChar(u.mouseWheel.dir);
		}
		break;
		default:
			assert(0 && "KeyBinding type field is invalid");
		}

		oss << m_mod.ToString();
		return oss.str();
	}

	void ActionBinding::SetFromString(const char *str)
	{
		const size_t BUF_SIZE = 64;
		const size_t len = strlen(str);
		if (len >= BUF_SIZE) {
			Output("invalid ActionBinding string\n");
			m_binding[0] = KeyBinding::FromString(str);
			m_binding[1].Clear();
		} else {
			const char *sep = strchr(str, ',');
			if (sep) {
				char buf[BUF_SIZE];
				const size_t len1 = sep - str;
				const size_t len2 = len - len1 - 1;
				memcpy(buf, str, len1);
				buf[len1] = '\0';
				m_binding[0] = KeyBinding::FromString(buf);
				memcpy(buf, sep + 1, len2);
				buf[len2] = '\0';
				m_binding[1] = KeyBinding::FromString(buf);
			} else {
				m_binding[0] = KeyBinding::FromString(str);
				m_binding[1].Clear();
			}
		}
	}

	std::string ActionBinding::ToString() const
	{
		std::ostringstream oss;
		if (m_binding[0].Enabled() && m_binding[1].Enabled()) {
			oss << m_binding[0].ToString() << "," << m_binding[1].ToString();
		} else if (m_binding[0].Enabled()) {
			oss << m_binding[0].ToString();
		} else if (m_binding[1].Enabled()) {
			oss << m_binding[1].ToString();
		} else {
			oss << "disabled";
		}
		return oss.str();
	}

	bool ActionBinding::IsActive() const
	{
		return m_binding[0].IsActive() || m_binding[1].IsActive();
	}

	void ActionBinding::StoreOnActionCallback(std::function<void(bool)> fun)
	{
		if (m_fun != nullptr) Error("It seems that a 'OnActionCallback' is already stored!");
		m_fun = fun;
	}

	InputResponse ActionBinding::CheckSDLEventAndDispatch(const SDL_Event &event)
	{
		if (m_disableBindings) return InputResponse::NOMATCH;
		switch (event.type) {
		case SDL_KEYDOWN: {
			if (m_binding[0].Matches(event.key.keysym) || m_binding[1].Matches(event.key.keysym) ) {
				if (m_fun != nullptr) m_fun(true);
				return InputResponse::MATCHED;
			}
		}
		break;
		case SDL_KEYUP: {
			if (m_binding[0].Matches(event.key.keysym) || m_binding[1].Matches(event.key.keysym) ) {
				if (m_fun != nullptr) m_fun(false);
				return InputResponse::MATCHED;
			}
		}
		break;
		case SDL_JOYBUTTONDOWN:
		case SDL_JOYBUTTONUP: {
			if (m_binding[0].Matches(event.jbutton) || m_binding[1].Matches(event.jbutton)) {
				if (event.jbutton.state == SDL_PRESSED) {
					if (m_fun != nullptr) m_fun(true);
				} else if (event.jbutton.state == SDL_RELEASED) {
					if (m_fun != nullptr) m_fun(false);
				}
				return InputResponse::MATCHED;
			}
		}
		break;
		case SDL_JOYHATMOTION: {
			if (m_binding[0].Matches(event.jhat) || m_binding[1].Matches(event.jhat)) {
				if (m_fun != nullptr) m_fun(true);
				// XXX to emit onRelease, we need to have access to the state of the joystick hat prior to this event,
				// so that we can detect the case of switching from a direction that matches the binding to some other direction
				return InputResponse::MATCHED;
			} else {
			}
		}
		break;
		case SDL_MOUSEWHEEL: {
			if (m_binding[0].Matches(event.wheel) || m_binding[1].Matches(event.wheel)) {
				if (m_fun != nullptr) {
					// 'false' so it can be treated as a SDL_RELEASED
					m_fun(false);
				}
				return InputResponse::MATCHED;
			}
		}
		break;
		default:
		break;
		}
		return InputResponse::NOMATCH;
	}

	float WheelAxisBinding::GetValue() const
	{
		switch (m_type) {
		case WheelAxisType::DISABLED: {
			return 0.0f;
		}
		break;
		case WheelAxisType::VERTICAL: {
			WheelDirection actual = Pi::input.GetWheelState();
			if (!m_mod.IsActive()) return 0.0;
			if (actual == m_direction) return 1.0;
			if (((actual == WheelDirection::UP) && (m_direction == WheelDirection::DOWN)) ||
				((actual == WheelDirection::DOWN) && (m_direction == WheelDirection::UP))
				) return -1.0;
			else return 0.0;
		}
		break;
		case WheelAxisType::HORIZONTAL: {
			WheelDirection actual = Pi::input.GetWheelState();
			if (!m_mod.IsActive()) return 0.0;
			if (actual == m_direction) return 1.0;
			if (((actual == WheelDirection::LEFT) && (m_direction == WheelDirection::RIGHT)) ||
				((actual == WheelDirection::RIGHT) && (m_direction == WheelDirection::LEFT))
				) return -1.0;
			else return 0.0;
		}
		break;
		}
		return 0.0;
	}

	std::string WheelAxisBinding::Description() const
	{
		switch (m_type) {
		case WheelAxisType::DISABLED: {
			return std::string();
		}
		break;
		case WheelAxisType::VERTICAL:
		case WheelAxisType::HORIZONTAL: {
			std::string desc = m_mod.Description();
			desc += GetMouseWheelDescription(m_direction);
			return desc;
		}
		break;
		}
		return std::string();
	}

	bool WheelAxisBinding::FromString(const char *str, WheelAxisBinding &ab)
	{
		if (strcmp(str, "disabled") == 0) {
			ab.Clear();
			return true;
		}

		const char *p = str;

		if (strncmp(str, "MWh", 3) == 0) {
			// Case of Wheel used as axis
			p += 3;
			WheelDirection dir = WheelDirectionFromChar(*p);
			if (dir == WheelDirection::NONE) {
				ab.Clear();
				return false;
			}
			ab.m_type = (dir == WheelDirection::UP) || (dir == WheelDirection::DOWN) ? WheelAxisType::VERTICAL :WheelAxisType::HORIZONTAL;
			ab.m_direction = dir;

			p += 1;
			if (strncmp(p, "Mod", 3) == 0) {
				p += 3;
				ab.m_mod = Modifiers(SDL_Keymod(atoi(p)));
			} else {
				ab.m_mod = Modifiers();
			}
			return true;
		} else {
			ab.Clear();
			return false;
		}
	}

	WheelAxisBinding WheelAxisBinding::FromString(const char *str)
	{
		WheelAxisBinding ab;
		if (!WheelAxisBinding::FromString(str, ab))
			ab.Clear();
		return ab;
	}

	std::string WheelAxisBinding::ToString() const
	{
		std::ostringstream oss;
		switch (m_type) {
		case WheelAxisType::DISABLED: {
			oss << "disabled";
		}
		break;
		case WheelAxisType::VERTICAL:
		case WheelAxisType::HORIZONTAL: {
			oss << "MWh" << WheelDirectionToChar(m_direction);
			oss << m_mod.ToString();
		}
		break;
		}
		return oss.str();
	}

	bool WheelAxisBinding::Matches(const SDL_MouseWheelEvent &mwe) const
	{
		if (m_type == WheelAxisType::DISABLED) return false;
		if (!m_mod.IsActive()) return false;
		return ((mwe.y != 0) && ((m_direction == WheelDirection::DOWN) || (m_direction == WheelDirection::UP))) ||
			((mwe.x != 0) && ((m_direction == WheelDirection::LEFT) || (m_direction == WheelDirection::RIGHT)));
	}

	bool WheelAxisBinding::IsActive() const
	{
		switch (m_type) {
		case WheelAxisType::DISABLED: {
			return false;
		}
		break;
		case WheelAxisType::VERTICAL:
		case WheelAxisType::HORIZONTAL: {
			// Active when direction of actual (Pi::input->wheel) state is
			// equal to stored direction or the opposite:
			WheelDirection actual = Pi::input.GetWheelState();
			if (actual == WheelDirection::NONE) return false;
			if (!m_mod.IsActive()) return false;
			return (((m_direction == WheelDirection::UP) || (m_direction == WheelDirection::DOWN)) &&
				((actual == WheelDirection::UP) || (actual == WheelDirection::DOWN))) ||
				(((m_direction == WheelDirection::LEFT) || (m_direction == WheelDirection::RIGHT)) &&
				((actual == WheelDirection::LEFT) || (actual == WheelDirection::RIGHT)));
		}
		default:
			return false;
		}
	}

	JoyAxisBinding::JoyAxisBinding(const SDL_JoystickGUID &joystickGuid, Uint8 axis_, SDL_Keymod mod_, AxisDirection direction_, float deadzone_, float sensitivity_) :
		m_joystick(Pi::input.JoystickFromGUID(joystickGuid)),
		m_axis(axis_),
		m_direction(direction_),
		m_mod(mod_),
		m_deadzone(deadzone_),
		m_sensitivity(sensitivity_)
	{}

	bool JoyAxisBinding::IsActive() const
	{
		if (!Enabled()) return false;
		if (!m_mod.IsActive()) return false;
		// If the stick is within the deadzone, it's not active.
		return std::abs(Pi::input.JoystickAxisState(m_joystick, m_axis)) > m_deadzone;
	}

	float JoyAxisBinding::GetValue() const
	{
		if (!Enabled()) return 0.0f;
		if (!m_mod.IsActive()) return 0.0f;

		const float o_val = Pi::input.JoystickAxisState(m_joystick, m_axis);

		// Deadzone with normalisation
		float value = fabs(o_val);
		if (value < m_deadzone) {
			return 0.0f;
		} else {
			// subtract deadzone and re-normalise to full range
			value = (value - m_deadzone) / (1.0f - m_deadzone);
		}

		// Apply sensitivity scaling and clamp.
		value = std::max(std::min(value * m_sensitivity, 1.0f), 0.0f);

		value = copysign(value, o_val);

		// Invert as necessary.
		return (m_direction == AxisDirection::POSITIVE) ? value : 0.0f - value;
	}

	bool JoyAxisBinding::Matches(const SDL_JoyAxisEvent &jax) const
	{
		if (!Enabled()) return false;
		if (!m_mod.IsActive()) return false;

		return jax.which == m_joystick && jax.axis == m_axis;
	}

	std::string JoyAxisBinding::Description() const
	{
		if (!Enabled()) return std::string();

		const char *axis_names[] = { Lang::X, Lang::Y, Lang::Z };
		std::ostringstream ossaxisnum;
		ossaxisnum << int(m_axis);

		std::string ret = m_mod.Description();
		ret += stringf(Lang::JOY_AXIS,
			formatarg("sign", m_direction == AxisDirection::NEGATIVE ? "-" : ""), // no + sign if positive
			formatarg("signp", m_direction == AxisDirection::NEGATIVE ? "-" : "+"), // optional with + sign
			formatarg("joynum", m_joystick),
			formatarg("joyname", Pi::input.JoystickName(m_joystick)),
			formatarg("axis", m_axis >= 0 && m_axis < 3 ? axis_names[m_axis] : ossaxisnum.str()));
		return ret;
	}

	bool JoyAxisBinding::FromString(const char *str, JoyAxisBinding &ab)
	{
		if (strcmp(str, "disabled") == 0) {
			ab.Clear();
			return true;
		}

		const char *p = str;

		if (p[0] == '-') {
			ab.m_direction = AxisDirection::NEGATIVE;
			p++;
		} else
			ab.m_direction = AxisDirection::POSITIVE;

		if (strncmp(p, "Joy", 3) != 0)
			return false;
		p += 3;

		const int JoyUUIDLength = 33;
		char joyUUIDBuf[JoyUUIDLength];

		// read the UUID
		if (!ReadToTok('/', &p, joyUUIDBuf, JoyUUIDLength)) {
			return false;
		}
		// force terminate
		joyUUIDBuf[JoyUUIDLength - 1] = '\0';
		// now, map the GUID to a joystick number
		const int joystick = Pi::input.JoystickFromGUIDString(joyUUIDBuf);
		if (joystick == -1) {
			return false;
		}
		// found a joystick
		assert(joystick < 256);
		ab.m_joystick = Uint8(joystick);

		if (strncmp(p, "Axis", 4) != 0)
			return false;

		p += 4;
		ab.m_axis = atoi(p);

		// Skip past the axis integer
		if (!(p = strstr(p, "/DZ")))
			return true; // deadzone is optional

		p += 3;
		ab.m_deadzone = atof(p);

		// Skip past the deadzone float
		if (!(p = strstr(p, "/E")))
			return true; // sensitivity is optional

		p += 2;
		ab.m_sensitivity = atof(p);

		if (!(p = strstr(p, "Mod"))) {
			ab.m_mod = Modifiers();
			return true; // modifiers are optional
		}

		p += 3;
		ab.m_mod = Modifiers(SDL_Keymod(atoi(p)));

		return true;
	}

	JoyAxisBinding JoyAxisBinding::FromString(const char *str)
	{
		JoyAxisBinding ab;
		if (!JoyAxisBinding::FromString(str, ab))
			ab.Clear();
		return ab;
	}

	std::string JoyAxisBinding::ToString() const
	{
		if (!Enabled()) return std::string("disabled");

		std::ostringstream oss;

		if (m_direction == AxisDirection::NEGATIVE)
			oss << '-';

		oss << "Joy";
		oss << Pi::input.JoystickGUIDString(m_joystick);
		oss << "/Axis";
		oss << int(m_axis);
		oss << "/DZ" << m_deadzone;
		oss << "/E" << m_sensitivity;
		oss << m_mod.ToString();

		return oss.str();
	}

	void AxisBinding::SetFromString(const std::string str)
	{
		size_t ofs = 0;
		size_t nextpos = str.find(',');
		if (nextpos == std::string::npos) return;

		if (str.substr(ofs, 8) != "disabled")
			axis = JoyAxisBinding::FromString(str.substr(0, nextpos).c_str());

		ofs = nextpos + 1;
		nextpos = str.find(',', ofs);
		if (str.substr(ofs, 8) != "disabled")
			m_wheel = WheelAxisBinding::FromString(str.substr(ofs, nextpos - ofs).c_str());

		ofs = nextpos + 1;
		nextpos = str.find(',', ofs);
		if (str.substr(ofs, 8) != "disabled")
			positive = KeyBinding::FromString(str.substr(ofs, nextpos - ofs).c_str());

		ofs = nextpos + 1;
		if (str.substr(ofs, 8) != "disabled")
			negative = KeyBinding::FromString(str.substr(ofs).c_str());
	}

	std::string AxisBinding::ToString() const
	{
		std::ostringstream oss;
		oss << axis.ToString() << ',';
		oss << m_wheel.ToString() << ',';
		oss << positive.ToString() << ',';
		oss << negative.ToString();
		return oss.str();
	}

	void AxisBinding::StoreOnAxisCallback(std::function<void(float)> fun)
	{
		if (m_fun != nullptr) Error("It seems that a 'OnAxisCallback' is already stored!");
		m_fun = fun;
	}

	bool AxisBinding::IsActive() const
	{
		return axis.IsActive() || m_wheel.IsActive() || positive.IsActive() || negative.IsActive();
	}

	float AxisBinding::GetValue() const
	{
		// Holding the positive and negative keys cancel each other out,
		float value = 0.0f;
		value += positive.IsActive() ? 1.0 : 0.0;
		value -= negative.IsActive() ? 1.0 : 0.0;

		// And input on the axis device supercedes both of them.
		return axis.IsActive() ? axis.GetValue() : (m_wheel.IsActive() ? m_wheel.GetValue() : value);
	}

	InputResponse AxisBinding::CheckSDLEventAndDispatch(const SDL_Event &event)
	{
		if (m_disableBindings) return InputResponse::NOMATCH;
		float value = GetValue();
		switch (event.type) {
		case SDL_KEYDOWN:
		case SDL_KEYUP: {
			if (positive.Matches(event.key.keysym) && negative.Matches(event.key.keysym)) {
				if (m_fun != nullptr) m_fun(value);
				return InputResponse::MATCHED;
			}
		}
		break;
		case SDL_MOUSEWHEEL: {
			if (m_wheel.Matches(event.wheel)) {
				if (m_fun != nullptr) m_fun(value);
				return InputResponse::MATCHED;
			}
		}
		break;
		case SDL_JOYBUTTONDOWN:
		case SDL_JOYBUTTONUP: {
			if (positive.Matches(event.jbutton) || negative.Matches(event.jbutton)) {
				if (m_fun != nullptr) m_fun(value);
				return InputResponse::MATCHED;
			}
		}
		break;
		case SDL_JOYHATMOTION: {
			if (positive.Matches(event.jhat) || positive.Matches(event.jhat)) {
				if (m_fun != nullptr) m_fun(value);
				// XXX to emit onRelease, we need to have access to the state of the joystick hat prior to this event,
				// so that we can detect the case of switching from a direction that matches the binding to some other direction
				return InputResponse::MATCHED;
			}
		}
		break;
		case SDL_JOYAXISMOTION: {
			if (axis.Matches(event.jaxis)) {
				if (m_fun != nullptr) m_fun(value);
				return InputResponse::MATCHED;
			}
		}
		default:
		break;
		}
		return InputResponse::NOMATCH;
	}

	void InitBindings()
	{
		GameConfSingleton::getInstance().Save();
	}

	void EnableBindings()
	{
		m_disableBindings = 0;
	}

	void DisableBindings()
	{
		m_disableBindings = 1;
	}

} // namespace KeyBindings
