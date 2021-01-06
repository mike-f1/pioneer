#include "BindingContainer.h"

#include "Input.h"
#include "InputLocator.h"
#include "KeyBindings.h"
#include "libs/utils.h"

#include <exception>

BindingContainer::BindingContainer(const std::string &name) :
	m_name(name)
{
	m_actions.reserve(4);
	m_axes.reserve(4);
}

BindingContainer::~BindingContainer()
{
	bool success = false;
	for (TAction &ab : m_actions) {
		success |= InputLocator::getInput()->DeleteActionBinding(ab.name);
	}

	for (TAxis &ab : m_axes) {
		success |= InputLocator::getInput()->DeleteAxisBinding(ab.name);
	}
	assert(success);
}

ActionId BindingContainer::AddActionBinding(std::string &id, BindingGroup &group, KeyBindings::ActionBinding binding)
{
	for (int i = 0; i < m_actions.size(); i++) {
		if (id == m_actions[i].name) throw std::runtime_error{"AddActionBinding of '" + id + "' is already in '"  + m_name + "'!\n"};
	}
	KeyBindings::ActionBinding *actionBind = InputLocator::getInput()->AddActionBinding(id, group, binding);
	actionBind->Enable(true);
	m_actions.push_back({id, actionBind});
	return m_actions.size() - 1;
}

AxisId BindingContainer::AddAxisBinding(std::string &id, BindingGroup &group, KeyBindings::AxisBinding binding)
{
	for (int i = 0; i < m_axes.size(); i++) {
		if (id == m_axes[i].name) throw std::runtime_error{"AddAxisBinding of '" + id + "' is already in '"  + m_name + "'!\n"};
	}
	KeyBindings::AxisBinding *axisBind = InputLocator::getInput()->AddAxisBinding(id, group, binding);
	axisBind->Enable(true);
	m_axes.push_back({id, axisBind});
	return m_axes.size() - 1;
}

ActionId BindingContainer::GetActionBinding(const std::string &id)
{
	for (int i = 0; i < m_actions.size(); i++) {
		if (id == m_actions[i].name) return i;
	}
	throw std::runtime_error{"GetActionBinding of '" + id + "' isn't present in '" + m_name + "'!\n"};
}

AxisId BindingContainer::GetAxisBinding(const std::string &id)
{
	for (int i = 0; i < m_axes.size(); i++) {
		if (id == m_axes[i].name) return i;
	}
	throw std::runtime_error{"GetAxisBinding of '" + id + "' isn't in '" + m_name + "'!\n"};
}

void BindingContainer::AddCallbackFunction(const std::string &id, const std::function<void(bool)> &fun)
{
	std::vector<TAction>::iterator ac = std::find_if(begin(m_actions), end(m_actions), [&id](TAction &action) {
		return (action.name == id);
	});
	if (ac != m_actions.end()) {
		if (ac->callback || ac->lua_callback.IsValid()) Output("WARNING: overwriting callback for '%s' in '%s'\n", ac->name.c_str(), m_name.c_str());
		if (ac->lua_callback.IsValid()) ac->lua_callback.Unref();
		ac->callback = fun;
		return;
	};
	std::vector<TAxis>::iterator ax = std::find_if(begin(m_axes), end(m_axes), [&id](TAxis &axis) {
		return (axis.name == id);
	});
	if (ax != m_axes.end()) {
		if (ax->callback || ax->lua_callback.IsValid()) Output("WARNING: overwriting callback for '%s' in '%s'\n", ax->name.c_str(), m_name.c_str());
		if (ax->lua_callback.IsValid()) ax->lua_callback.Unref();
		ax->callback = fun;
		return;
	};
}

void BindingContainer::AddCallbackFunction(const std::string &id, LuaRef &fun)
{
	lua_State *l = fun.GetLua();
	if (l) {
		fun.PushCopyToStack();
		if (!lua_isfunction(l, -1)) {
			Output("WARNING: Invalid function as callback for '%s' in '%s'!\n", id.c_str(), m_name.c_str());
			return;
		}
	}
	std::vector<TAction>::iterator ac = std::find_if(begin(m_actions), end(m_actions), [&id](TAction &action) {
		return (action.name == id);
	});
	if (ac != m_actions.end()) {
		if (ac->callback  || ac->lua_callback.IsValid()) Output("WARNING: overwriting callback for '%s' in '%s'\n", ac->name.c_str(), m_name.c_str());
		if (ac->callback) ac->callback = nullptr;
		ac->lua_callback = fun;
		return;
	};
	std::vector<TAxis>::iterator ax = std::find_if(begin(m_axes), end(m_axes), [&id](TAxis &axis) {
		return (axis.name == id);
	});
	if (ax != m_axes.end()) {
		if (ax->callback || ax->lua_callback.IsValid()) Output("WARNING: overwriting callback for '%s' in '%s'\n", ax->name.c_str(), m_name.c_str());
		if (ax->callback) ax->callback = nullptr;
		ax->lua_callback = fun;
		return;
	};
}

void BindingContainer::SetBTrait(const std::string &id, const KeyBindings::BehaviourMod &bm)
{
	std::vector<TAction>::iterator ac = std::find_if(begin(m_actions), end(m_actions), [&id](TAction &action) {
		return (action.name == id);
	});
	if (ac != m_actions.end()) {
		ac->binding_ptr->SetBTrait(bm);
		return;
	};
	std::vector<TAxis>::iterator ax = std::find_if(begin(m_axes), end(m_axes), [&id](TAxis &axis) {
		return (axis.name == id);
	});
	if (ax != m_axes.end()) {
		ac->binding_ptr->SetBTrait(bm);
		return;
	};
}

void BindingContainer::RemoveCallbacks()
{
	std::for_each(m_actions.begin(), m_actions.end(), [](TAction &action) {
		action.callback = nullptr;
	});
	std::for_each(m_axes.begin(), m_axes.end(), [](TAxis &axis) {
		axis.callback = nullptr;
	});
}

KeyBindings::InputResponse BindingContainer::ProcessSDLEvent(const SDL_Event &event)
{
	using namespace KeyBindings;

	bool matched = false;

	for (TAction &action : m_actions) {
		auto resp = action.binding_ptr->CheckSDLEventAndDispatch(event);
		//Output("\t\t%s -> %s\n", action.name.c_str(), resp == InputResponse::MATCHED ? "matched" : "no match");
		if (resp == InputResponse::MATCHED) {
			if (action.callback) {
				//Output("Callback for %s\n", action.name.c_str());
				action.callback(action.binding_ptr->GetIsUp());
			}
			if (action.lua_callback.IsValid()) {
				//Output("LuaCallback for %s\n", action.name.c_str());
				lua_State *l = action.lua_callback.GetLua();
				action.lua_callback.PushCopyToStack();
				lua_pushboolean(l, action.binding_ptr->GetIsUp());
				lua_call(l, 1, 0);
			}
			return resp;
		}
		matched = matched || resp != InputResponse::NOMATCH;
	}

	for (TAxis &axis : m_axes) {
		auto resp = axis.binding_ptr->CheckSDLEventAndDispatch(event);
		if (resp == InputResponse::MATCHED) {
			if (axis.callback) axis.callback(axis.binding_ptr->GetValue());
			return resp;
		}
		matched = matched || resp != InputResponse::NOMATCH;
	}

	return matched ? InputResponse::PASSTHROUGH : InputResponse::NOMATCH;
}
