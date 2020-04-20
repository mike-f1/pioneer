#include "InputFrame.h"

#include "Input.h"
#include "KeyBindings.h"
#include "Pi.h"

InputFrame::~InputFrame()
{
	for (TActionPair &ab : m_actions) {
		bool success = Pi::input.DeleteActionBinding(ab.first);
	}

	for (TAxisPair &ab : m_axes) {
		bool success = Pi::input.DeleteAxisBinding(ab.first);
	}
}

KeyBindings::ActionBinding *InputFrame::AddActionBinding(std::string id, BindingGroup &group, KeyBindings::ActionBinding binding)
{
	KeyBindings::ActionBinding *actionBind = Pi::input.AddActionBinding(id, group, binding);
	m_actions.push_back({id, actionBind});
	return actionBind;
}

KeyBindings::AxisBinding *InputFrame::AddAxisBinding(std::string id, BindingGroup &group, KeyBindings::AxisBinding binding)
{
	KeyBindings::AxisBinding *axisBind = Pi::input.AddAxisBinding(id, group, binding);
	m_axes.push_back({id, axisBind});
	return axisBind;
}

InputResponse InputFrame::ProcessSDLEvent(const SDL_Event &event)
{
	if (!m_active) return InputResponse::NOMATCH;

	//Output("InputFrame::ProcessSDLEvent for %s\n", m_name.c_str());
	bool matched = false;

	for (TActionPair &action : m_actions) {
		auto resp = action.second->CheckSDLEventAndDispatch(event);
		if (resp == InputResponse::MATCHED) return resp;
		matched = matched || resp != InputResponse::NOMATCH;
	}

	for (TAxisPair &axis : m_axes) {
		auto resp = axis.second->CheckSDLEventAndDispatch(event);
		if (resp == InputResponse::MATCHED) return resp;
		matched = matched || resp != InputResponse::NOMATCH;
	}

	return matched ? InputResponse::PASSTHROUGH : InputResponse::NOMATCH;
}
