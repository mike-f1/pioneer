#include "BindingContainer.h"

#include "Input.h"
#include "KeyBindings.h"
#include "Pi.h"
#include "libs/utils.h"

BindingContainer::BindingContainer(const std::string &name) :
	m_name(name)
{
	Output("BindingContainer(%s)\n", m_name.c_str());
	m_actions.reserve(4);
	m_axes.reserve(4);
}

BindingContainer::~BindingContainer()
{
	Output("~BindingContainer() of %s\n", m_name.c_str());
	bool success = false;
	for (TAction &ab : m_actions) {
		success |= Pi::input->DeleteActionBinding(ab.name);
	}

	for (TAxis &ab : m_axes) {
		success |= Pi::input->DeleteAxisBinding(ab.name);
	}
	assert(success);
}

ActionId BindingContainer::AddActionBinding(std::string &id, BindingGroup &group, KeyBindings::ActionBinding binding)
{
	KeyBindings::ActionBinding *actionBind = Pi::input->AddActionBinding(id, group, binding);
	actionBind->Enable(true);
	m_actions.push_back({id, actionBind});
	return m_actions.size() - 1;
}

AxisId BindingContainer::AddAxisBinding(std::string &id, BindingGroup &group, KeyBindings::AxisBinding binding)
{
	KeyBindings::AxisBinding *axisBind = Pi::input->AddAxisBinding(id, group, binding);
	axisBind->Enable(true);
	m_axes.push_back({id, axisBind});
	return m_axes.size() - 1;
}

void BindingContainer::AddCallbackFunction(const std::string &id, const std::function<void(bool)> &fun)
{
	std::vector<TAction>::iterator ac = std::find_if(begin(m_actions), end(m_actions), [&id](TAction &action) {
		return (action.name == id);
	});
	if (ac != m_actions.end()) {
		if (ac->callback != nullptr) Output("WARNING: overwriting callback for '%s' in '%s'\n", ac->name.c_str(), m_name.c_str());
		ac->callback = fun;
		return;
	};
	std::vector<TAxis>::iterator ax = std::find_if(begin(m_axes), end(m_axes), [&id](TAxis &axis) {
		return (axis.name == id);
	});
	if (ax != m_axes.end()) {
		if (ax->callback != nullptr) Output("WARNING: overwriting callback for '%s' in '%s'\n", ax->name.c_str(), m_name.c_str());
		ax->callback = fun;
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

KeyBindings::InputResponse BindingContainer::ProcessSDLEvent(const SDL_Event &event)
{
	using namespace KeyBindings;

	// Filter non-(yet-)bindable events
	switch (event.type) {
	case SDL_MOUSEMOTION:
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
	case SDL_WINDOWEVENT:
	case SDL_DROPFILE:
	case SDL_DROPTEXT:
	case SDL_DROPBEGIN:
	case SDL_DROPCOMPLETE:
	case SDL_AUDIODEVICEADDED:
	case SDL_AUDIODEVICEREMOVED:
	case SDL_SYSWMEVENT:
		return InputResponse::NOMATCH;
	};

	bool matched = false;

	for (TAction &action : m_actions) {
		auto resp = action.binding_ptr->CheckSDLEventAndDispatch(event);
		//Output("\t\t%s -> %s\n", action.name.c_str(), resp == InputResponse::MATCHED ? "matched" : "no match");
		if (resp == InputResponse::MATCHED) {
			if (action.callback) {
				//Output("Callback for %s\n", action.name.c_str());
				action.callback(action.binding_ptr->GetIsUp());
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
