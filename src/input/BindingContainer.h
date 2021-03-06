#ifndef BINDINGCONTAINER_H
#define BINDINGCONTAINER_H

#include <functional>
#include <string>
#include <vector>

#include "input/InputFwd.h"
#include "libs/RefCounted.h"
#include "LuaRef.h"

struct BindingGroup;
union SDL_Event;
class BindingContainerStatusTicket;
class BindingContainer;

namespace KeyBindings {
	struct ActionBinding;
	struct AxisBinding;
	enum class InputResponse;
	enum class BehaviourMod;
}

class BindingContainer : public RefCounted
{
	friend class Input;
	friend class InputFrame;

	BindingContainer() = delete;
	BindingContainer(const std::string &name);
	~BindingContainer();

	const std::string &GetName() { return m_name; };

	ActionId AddActionBinding(std::string &id, BindingGroup &group, KeyBindings::ActionBinding binding);
	AxisId AddAxisBinding(std::string &id, BindingGroup &group, KeyBindings::AxisBinding binding);

	ActionId GetActionBinding(const std::string &id);
	AxisId GetAxisBinding(const std::string &id);

	void AddCallbackFunction(const std::string &id, const std::function<void(bool)> &fun);
	void AddCallbackFunction(const std::string &id, LuaRef &fun);
	void SetBTrait(const std::string &id, const KeyBindings::BehaviourMod &bm);

	void RemoveCallbacks();

//private:
	KeyBindings::InputResponse ProcessSDLEvent(const SDL_Event &event);

	std::string m_name;

	struct TAction {
		std::string name;
		KeyBindings::ActionBinding *binding_ptr;
		std::function<void(bool)> callback;
		LuaRef lua_callback;
	};
	struct TAxis {
		std::string name;
		KeyBindings::AxisBinding *binding_ptr;
		std::function<void(float)> callback;
		LuaRef lua_callback;
	};
	std::vector<TAction> m_actions;
	std::vector<TAxis> m_axes;
};

#endif // BINDINGCONTAINER_H
