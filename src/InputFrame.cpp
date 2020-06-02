#include "InputFrame.h"

#include "Input.h"
#include "KeyBindings.h"
#include "Pi.h"

#include <SDL_events.h>

InputFrame::InputFrame(const std::string &name) :
		m_name(name),
		m_active(false)
{
	m_actions.reserve(4);
	m_axes.reserve(4);
	Pi::input->PushInputFrame(this);
}

InputFrame::~InputFrame()
{
	bool success = false;
	for (TActionPair &ab : m_actions) {
		success |= Pi::input->DeleteActionBinding(ab.first);
	}

	for (TAxisPair &ab : m_axes) {
		success |= Pi::input->DeleteAxisBinding(ab.first);
	}
	assert(success);

	Pi::input->RemoveInputFrame(this);
}

void InputFrame::SetActive(bool is_active)
{
	if (m_active == is_active) return;
	m_active = is_active;
	std::for_each(begin(m_actions), end(m_actions), [is_active](TActionPair &each) {
		each.second->Enable(is_active);
	});
	std::for_each(begin(m_axes), end(m_axes), [is_active](TAxisPair &each) {
		each.second->Enable(is_active);
	});
}

KeyBindings::ActionBinding *InputFrame::AddActionBinding(std::string id, BindingGroup &group, KeyBindings::ActionBinding binding)
{
	KeyBindings::ActionBinding *actionBind = Pi::input->AddActionBinding(id, group, binding);
	actionBind->Enable(m_active);
	m_actions.push_back({id, actionBind});
	return actionBind;
}

KeyBindings::AxisBinding *InputFrame::AddAxisBinding(std::string id, BindingGroup &group, KeyBindings::AxisBinding binding)
{
	KeyBindings::AxisBinding *axisBind = Pi::input->AddAxisBinding(id, group, binding);
	axisBind->Enable(m_active);
	m_axes.push_back({id, axisBind});
	return axisBind;
}

KeyBindings::InputResponse InputFrame::ProcessSDLEvent(const SDL_Event &event)
{
	using namespace KeyBindings;

	if (!m_active) return InputResponse::NOMATCH;

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
