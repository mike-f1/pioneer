#include "InputFrame.h"

#include "BindingContainer.h"
#include "Input.h"
#include "KeyBindings.h"
#include "Pi.h"
#include "libs/utils.h"

#include <SDL_events.h>

InputFrame::InputFrame(const std::string &name) :
		m_active(false)
{
	if (!Pi::input) {
		Output("InputFrame '%s' instantiation needs 'Pi::input'!\n", name.c_str());
		abort();
	}
	m_bindingContainer = Pi::input->CreateOrShareBindContainer(name, this);
	Output("In input frame, instead... %i\n", m_bindingContainer->GetRefCount());
}

InputFrame::~InputFrame()
{
	Pi::input->RemoveBindingContainer(this);
}

void InputFrame::SetActive(bool is_active)
{
	m_active = is_active;
}

ActionId InputFrame::AddActionBinding(std::string id, BindingGroup &group, KeyBindings::ActionBinding binding)
{
	return m_bindingContainer->AddActionBinding(id, group, binding);
}

AxisId InputFrame::AddAxisBinding(std::string id, BindingGroup &group, KeyBindings::AxisBinding binding)
{
	return m_bindingContainer->AddAxisBinding(id, group, binding);
}

void InputFrame::AddCallbackFunction(const std::string &id, const std::function<void(bool)> &fun)
{
	m_bindingContainer->AddCallbackFunction(id, fun);
}

void InputFrame::SetBTrait(const std::string &id, const KeyBindings::BehaviourMod &bm)
{
	m_bindingContainer->SetBTrait(id, bm);
}

bool InputFrame::IsActive(ActionId id)
{
	if (!m_active) return false;
	return m_bindingContainer->m_actions.at(id).binding_ptr->IsActive();
}

bool InputFrame::IsActive(AxisId id)
{
	if (!m_active) return false;
	return m_bindingContainer->m_axes.at(id).binding_ptr->IsActive();
}
float InputFrame::GetValue(AxisId id)
{
	if (!m_active) return 0.f;
	return m_bindingContainer->m_axes.at(id).binding_ptr->GetValue();
}

void InputFrame::CheckSDLEventAndDispatch(ActionId id, const SDL_Event &event)
{
	if (!m_active) return;
	m_bindingContainer->m_actions.at(id).binding_ptr->CheckSDLEventAndDispatch(event);
}

BindingPage &InputFrame::GetBindingPage(const std::string &id)
{
	return Pi::input->GetBindingPage(id);
}

KeyBindings::InputResponse InputFrame::ProcessSDLEvent(const SDL_Event &event)
{
	if (!m_active) return KeyBindings::InputResponse::NOMATCH;

	return m_bindingContainer->ProcessSDLEvent(event);
}

namespace InputFWD {
	float GetMoveSpeedShiftModifier()
	{
		return Pi::input->GetMoveSpeedShiftModifier();
	}

	std::tuple<bool, int, int> GetMouseMotion(MouseMotionBehaviour mmb)
	{
		return Pi::input->GetMouseMotion(mmb);
	}

	bool IsMouseYInvert()
	{
		return Pi::input->IsMouseYInvert();
	}

	std::unique_ptr<InputFrameStatusTicket> DisableAllInputFrameExcept(InputFrame *current)
	{
		return Pi::input->DisableAllInputFrameExcept(current);
	}
} // namespace InputFWD
