// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef KEYBINDINGS_H
#define KEYBINDINGS_H

#include <cassert>
#include <functional>
#include <iosfwd>
#include <limits>
#include <string>
#include <sstream>

#include <SDL_joystick.h>
#include <SDL_events.h>
#include <SDL_keyboard.h>

#include "libs/bitmask_op.h"

namespace KeyBindings {
	enum class BehaviourMod;
}

template<>
struct enable_bitmask_operators<KeyBindings::BehaviourMod> {
	static constexpr bool enable = true;
};

namespace KeyBindings {

	enum class InputResponse {
		// None of the inputs match the event.
		NOMATCH,
		// An input matched, but won't consume the event.
		PASSTHROUGH,
		// An input matched and consumed the event.
		MATCHED
	};

	enum class BindType {
		BINDING_DISABLED,
		KEYBOARD_KEY,
		JOYSTICK_BUTTON,
		JOYSTICK_HAT,
		MOUSE_WHEEL,
		MOUSE_MOTION, // TODO: implementme!
		MOUSE_BUTTON // TODO: implementme!
	};

	enum class WheelDirection {
		UP,
		DOWN,
		LEFT,
		RIGHT,
		NONE
	};

	enum class BehaviourMod {
		NONE = 0,
		DISALLOW_MODIFIER = 1,
		ALLOW_KEYBOARD_ONLY = 2,
	};

	class BehaviourTrait {
		friend struct ActionBinding;
	protected:
		BehaviourTrait() :
			m_bmTrait(BehaviourMod::NONE)
		{}

		bool HaveBTrait(BehaviourMod masked) const;
	private:
		BehaviourMod m_bmTrait;
	};

	// Take an SDL_Keymod, make modifiers not L/R and filter unused
	SDL_Keymod KeymodUnifyLR(SDL_Keymod mod);

	class Modifiers {
	public:
		Modifiers(SDL_Keymod kmod = KMOD_NONE);
		Modifiers(const std::string &str);

		std::string ToString() const; // for serialisation
		std::string Description() const; // for display to the user

		bool Matches(const SDL_Keymod &mod) const;
		bool IsActive() const;
	private:
		SDL_Keymod m_mod;
	};

	struct KeyBinding : public BehaviourTrait {
	public:
		KeyBinding() :
			BehaviourTrait(),
			m_mod(),
			type(BindType::BINDING_DISABLED)
		{
			u.keyboard.key = SDLK_UNKNOWN;
		}
		KeyBinding(SDL_Keycode key, SDL_Keymod mod = KMOD_NONE) :
			BehaviourTrait(),
			m_mod(mod),
			type(BindType::KEYBOARD_KEY)
		{
			u.keyboard.key = key;
		}
		KeyBinding(WheelDirection dir, SDL_Keymod mod = KMOD_NONE) :
			BehaviourTrait(),
			m_mod(mod),
			type(BindType::MOUSE_WHEEL)
		{
			assert(dir != WheelDirection::NONE);
			u.mouseWheel.dir = dir;
		}
		KeyBinding(const SDL_JoystickGUID &joystickGuid, uint8_t button, SDL_Keymod mod = KMOD_NONE);
		KeyBinding(const SDL_JoystickGUID &joystickGuid, uint8_t hat, uint8_t dir, SDL_Keymod mod = KMOD_NONE);

		// return true if all is ok, otherwise it return false
		static bool FromString(const char *str, KeyBinding &binding);

		std::string ToString() const; // for serialisation
		std::string Description() const; // for display to the user

		bool IsActive() const;
		bool Matches(const SDL_Keysym &sym) const;
		bool Matches(const SDL_JoyButtonEvent &joy) const;
		bool Matches(const SDL_JoyHatEvent &joy) const;
		bool Matches(const SDL_MouseWheelEvent &mwe) const;

		void Clear() { memset(this, 0, sizeof(*this)); type = BindType::BINDING_DISABLED; }

		bool Enabled() const { return (type != BindType::BINDING_DISABLED); }

	private:
		Modifiers m_mod;
		BindType type;
		union {
			struct {
				SDL_Keycode key;
			} keyboard;

			struct {
				uint8_t joystick;
				uint8_t button;
			} joystickButton;

			struct {
				uint8_t joystick;
				uint8_t hat;
				uint8_t direction;
			} joystickHat;

			struct {
				WheelDirection dir;
			} mouseWheel;
			/* TODO: implement binding mouse buttons.
				struct {
					uint8_t button;
					// TODO: implement binding multiple clicks as their own action.
					uint8_t clicks;
				} mouseButton;
				*/
		} u;
	};

	struct ActionBinding {
		ActionBinding() :
			m_disabled(false)
		{}
		ActionBinding(KeyBinding b1, KeyBinding b2 = KeyBinding()) :
			m_disabled(false),
			m_binding({b1, b2})
		{}
		// This constructor is just a programmer shortcut.
		ActionBinding(SDL_Keycode k1, SDL_Keycode k2 = SDLK_UNKNOWN) :
			m_disabled(false)
		{
			m_binding[0] = KeyBinding(k1);
			if (k2 != SDLK_UNKNOWN) m_binding[1] = KeyBinding(k2);
		}
		ActionBinding(WheelDirection dir, SDL_Keymod mod = KMOD_NONE) :
			m_disabled(false)
		{
			m_binding[0] = KeyBinding(dir, mod);
		}

		void SetFromBindings(KeyBinding &b1, KeyBinding &b2);
		void SetFromString(const std::string &str);

		void Enable(bool enable) { m_disabled = !enable; }

		std::string ToString() const;

		bool IsActive() const;

		void StoreOnActionCallback(const std::function<void(bool)> &fun);

		InputResponse CheckSDLEventAndDispatch(const SDL_Event &event);

		const KeyBinding &GetBinding(int i) const { return m_binding.at(i); }

		//bool HaveBTrait(BehaviourMod bm) { return m_binding[0].HaveBTrait(bm); }
		void SetBTrait(BehaviourMod bm) {
			m_binding[0].m_bmTrait = bm;
			m_binding[1].m_bmTrait = bm;
		}
	private:
		bool m_disabled;
		std::array<KeyBinding, 2> m_binding;

		std::function<void(bool)> m_fun;
	};

	struct WheelAxisBinding {
		WheelAxisBinding() :
			m_type(WheelAxisType::DISABLED),
			m_direction(WheelDirection::NONE),
			m_mod(KMOD_NONE)
		{}

		WheelAxisBinding(WheelDirection dir, SDL_Keymod mod = KMOD_NONE) :
			m_type(((dir == WheelDirection::UP) || (dir == WheelDirection::DOWN)) ? WheelAxisType::VERTICAL :
				((dir == WheelDirection::LEFT) || (dir == WheelDirection::RIGHT)) ? WheelAxisType::HORIZONTAL :
				WheelAxisType::DISABLED),
			m_direction(dir),
			m_mod(mod)
		{}

		enum class WheelAxisType {
			DISABLED,
			VERTICAL,
			HORIZONTAL
		};

		float GetValue() const;
		std::string Description() const;

		void Clear()
		{
			m_type = WheelAxisType::DISABLED;
			m_direction = WheelDirection::NONE;
			m_mod = Modifiers();
		}

		bool Enabled() const { return (m_type != WheelAxisType::DISABLED); }

		static bool FromString(const char *str, WheelAxisBinding &binding);
		std::string ToString() const;

		bool Matches(const SDL_MouseWheelEvent &mwe) const;
		bool IsActive() const;

	private:
		WheelAxisType m_type;
		WheelDirection m_direction;
		Modifiers m_mod;
	};

	enum class AxisDirection {
		POSITIVE,
		NEGATIVE
	};

	struct JoyAxisBinding {
	public:
		JoyAxisBinding() :
			m_joystick(JOYSTICK_DISABLED),
			m_axis(0),
			m_direction(AxisDirection::POSITIVE),
			m_deadzone(0.0f),
			m_sensitivity(1.0f),
			m_mod(KMOD_NONE)
		{}
		JoyAxisBinding(const SDL_JoystickGUID &joystickGuid, uint8_t axis_, SDL_Keymod mod_ = KMOD_NONE, AxisDirection direction_ = AxisDirection::POSITIVE, float deadzone_ = 0.0f, float sensitivity_ = 1.0f);

		float GetValue() const;
		std::string Description() const;

		void Clear()
		{
			m_joystick = JOYSTICK_DISABLED;
			m_axis = 0;
			m_direction = AxisDirection::POSITIVE;
			m_deadzone = 0.0f;
			m_sensitivity = 1.0f;
			m_mod = Modifiers();
		}

		bool Enabled() const { return (m_joystick != JOYSTICK_DISABLED); }

		static bool FromString(const char *str, JoyAxisBinding &binding);
		std::string ToString() const;

		bool Matches(const SDL_JoyAxisEvent &jax) const;
		bool IsActive() const;

	private:
		enum { JOYSTICK_DISABLED = std::numeric_limits<uint8_t>::max() };
		uint8_t m_joystick;
		uint8_t m_axis;
		AxisDirection m_direction;
		float m_deadzone;
		float m_sensitivity;
		Modifiers m_mod;
	};

	enum class KeyDirection { POS, NEG };

	struct AxisBinding {
		AxisBinding() :
			m_disabled(false)
		{}
		AxisBinding(JoyAxisBinding ax, WheelAxisBinding wheel = WheelAxisBinding(),
			KeyBinding pos = KeyBinding(), KeyBinding neg = KeyBinding()) :
			m_disabled(false),
			m_axis(ax),
			m_wheel(wheel),
			m_positive(pos),
			m_negative(neg)
		{}
		// These constructors are just programmer shortcuts.
		AxisBinding(SDL_Keycode k1, SDL_Keycode k2) :
			m_disabled(false),
			m_positive(KeyBinding(k1)),
			m_negative(KeyBinding(k2))
		{}
		AxisBinding(WheelDirection wd) :
			m_disabled(false),
			m_wheel(wd)
		{}

		void SetFromBindings(JoyAxisBinding &ax, WheelAxisBinding &wheel, KeyBinding &pos, KeyBinding &neg);
		void SetFromString(const std::string &str);

		void Enable(bool enable) { m_disabled = !enable; }
		bool IsEnabled() { return !m_disabled; }

		std::string ToString() const;

		void StoreOnAxisCallback(const std::function<void(float)> &fun);

		bool IsActive() const;
		float GetValue() const;
		InputResponse CheckSDLEventAndDispatch(const SDL_Event &event);

		const JoyAxisBinding &GetAxis() const { return m_axis; }
		const WheelAxisBinding &GetWheel() const { return m_wheel; }
		const KeyBinding &GetKey(KeyDirection k) const {
			if (k == KeyDirection::POS) return m_positive;
			else return m_negative;
		}
	private:
		bool m_disabled;
		JoyAxisBinding m_axis;
		WheelAxisBinding m_wheel;
		KeyBinding m_positive;
		KeyBinding m_negative;

		std::function<void(float)> m_fun;
	};

	void InitBindings();
	void EnableBindings();
	void DisableBindings();

} // namespace KeyBindings

#endif /* KEYBINDINGS_H */
