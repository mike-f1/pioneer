#include "InputFrame.h"

#include "BindingContainer.h"
#include "Input.h"
#include "InputFrameStatusTicket.h"
#include "InputLocator.h"
#include "KeyBindings.h"

#include <SDL_events.h>
#include <exception>

InputFrame::InputFrame(const std::string &name) :
		m_active(false),
		m_lockInsertion(false)
{
	if (!InputLocator::getInput()) throw std::runtime_error{"InputFrame '" + name + "' instantiation needs 'Input'!\n"};

	m_bindingContainer = InputLocator::getInput()->CreateOrShareBindContainer(name, this);
}

InputFrame::~InputFrame()
{
	/* Need to reset m_bindingContainer _before_ call of RemoveBindingContainer
	 * which in turn call Purge to remove orphans
	 * TODO: a better way would be a free function?
	*/
	m_bindingContainer.Reset();
	InputLocator::getInput()->RemoveBindingContainer(this);
}

const std::string &InputFrame::GetName()
{
	return m_bindingContainer->GetName();
}

void InputFrame::SetActive(bool is_active)
{
	m_active = is_active;
}

ActionId InputFrame::AddActionBinding(std::string id, BindingGroup &group, KeyBindings::ActionBinding binding)
{
	if (m_lockInsertion) throw std::runtime_error { "Attemp to add an action on '" + m_bindingContainer->GetName() + "' which is locked\n" };
	return m_bindingContainer->AddActionBinding(id, group, binding);
}

AxisId InputFrame::AddAxisBinding(std::string id, BindingGroup &group, KeyBindings::AxisBinding binding)
{
	if (m_lockInsertion) throw std::runtime_error { "Attemp to add an axis on '" + m_bindingContainer->GetName() + "' which is locked\n" };
	return m_bindingContainer->AddAxisBinding(id, group, binding);
}

ActionId InputFrame::GetActionBinding(const std::string &id)
{
	return m_bindingContainer->GetActionBinding(id);
}

AxisId InputFrame::GetAxisBinding(const std::string &id)
{
	return m_bindingContainer->GetAxisBinding(id);
}

void InputFrame::AddCallbackFunction(const std::string &id, const std::function<void(bool)> &fun)
{
	if (m_lockInsertion) throw std::runtime_error { "Attemp to add a callback on '" + m_bindingContainer->GetName() + "' which is locked\n" };
	m_bindingContainer->AddCallbackFunction(id, fun);
}

void InputFrame::AddCallbackFunction(const std::string &id, LuaRef &fun)
{
	if (m_lockInsertion) throw std::runtime_error { "Attemp to add a callback on '" + m_bindingContainer->GetName() + "' which is locked\n" };
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

KeyBindings::InputResponse InputFrame::ProcessSDLEvent(const SDL_Event &event)
{
	if (!m_active) return KeyBindings::InputResponse::NOMATCH;

	return m_bindingContainer->ProcessSDLEvent(event);
}

namespace InputFWD {
	BindingPage &GetBindingPage(const std::string &id)
	{
		return InputLocator::getInput()->GetBindingPage(id);
	}

	float GetMoveSpeedShiftModifier()
	{
		return InputLocator::getInput()->GetMoveSpeedShiftModifier();
	}

	std::tuple<bool, int, int> GetMouseMotion(MouseMotionBehaviour mmb)
	{
		return InputLocator::getInput()->GetMouseMotion(mmb);
	}

	bool IsMouseYInvert()
	{
		return InputLocator::getInput()->IsMouseYInvert();
	}

	std::unique_ptr<InputFrameStatusTicket> DisableAllInputFrameExcept(InputFrame *current)
	{
		return InputLocator::getInput()->DisableAllInputFrameExcept(current);
	}
} // namespace InputFWD
