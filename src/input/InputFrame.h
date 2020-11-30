#ifndef INPUTFRAME_H
#define INPUTFRAME_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "libs/RefCounted.h"

struct BindingGroup;
struct BindingPage;
union SDL_Event;
enum class MouseMotionBehaviour;
class InputFrameStatusTicket;
class BindingContainer;

namespace KeyBindings {
	struct ActionBinding;
	struct AxisBinding;
	enum class InputResponse;
	enum class BehaviourMod;
}

class ActionId;
class AxisId;

class InputFrame final {
	friend class Input;
public:
	InputFrame() = delete;
	InputFrame(const InputFrame &) = delete;

	InputFrame(const std::string &name) ;
	~InputFrame();

	bool IsActive() const { return m_active; }
	void SetActive(bool is_active);

	// Useful for shared InputFrames, when is set it throw if Add*Binding is called
	void LockInsertion() { m_lockInsertion = true; }

	ActionId AddActionBinding(std::string id, BindingGroup &group, KeyBindings::ActionBinding binding);
	AxisId AddAxisBinding(std::string id, BindingGroup &group, KeyBindings::AxisBinding binding);

	ActionId GetActionBinding(const std::string &id);
	AxisId GetAxisBinding(const std::string &id);

	void AddCallbackFunction(const std::string &id, const std::function<void(bool)> &fun);
	void SetBTrait(const std::string &id, const KeyBindings::BehaviourMod &bm);

	bool IsActive(ActionId id);
	bool IsActive(AxisId id);
	float GetValue(AxisId id);

private:
	// Check the event against all the inputs in this frame.
	KeyBindings::InputResponse ProcessSDLEvent(const SDL_Event &event);

	bool m_active;
	bool m_lockInsertion;

	RefCountedPtr<BindingContainer> m_bindingContainer;
};

namespace InputFWD {
	// These functions are here to avoid direct inclusion of InputLocator & Input
	BindingPage &GetBindingPage(const std::string &id);

	float GetMoveSpeedShiftModifier();

	// Return true if the behaviour is the current one and if at least a value is != 0,
	// second and third parameters are mouse movement relative coordinates
	std::tuple<bool, int, int> GetMouseMotion(MouseMotionBehaviour mmb);

	bool IsMouseYInvert();

	std::unique_ptr<InputFrameStatusTicket> DisableAllInputFrameExcept(InputFrame *current);
} // namespace InputFWD

#endif // INPUTFRAME_H
