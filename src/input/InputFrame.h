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

class InputFrame {
	friend class Input;
public:
	InputFrame() = delete;
	InputFrame(const InputFrame &) = delete;

	InputFrame(const std::string &name) ;
	virtual ~InputFrame();

	bool IsActive() const { return m_active; }
	void SetActive(bool is_active);

	ActionId AddActionBinding(std::string id, BindingGroup &group, KeyBindings::ActionBinding binding);
	AxisId AddAxisBinding(std::string id, BindingGroup &group, KeyBindings::AxisBinding binding);

	void AddCallbackFunction(const std::string &id, const std::function<void(bool)> &fun);
	void SetBTrait(const std::string &id, const KeyBindings::BehaviourMod &bm);

	bool IsActive(ActionId id);
	bool IsActive(AxisId id);
	float GetValue(AxisId id);
	void CheckSDLEventAndDispatch(ActionId id, const SDL_Event &event);

	BindingPage &GetBindingPage(const std::string &id);

	// Call this at startup and register all the bindings associated with the frame.
	virtual void RegisterBindings() {};

	// Called when the frame is added to the stack.
	virtual void onFrameAdded() {};

	// Called when the frame is removed from the stack.
	virtual void onFrameRemoved() {};

private:
	// Check the event against all the inputs in this frame.
	KeyBindings::InputResponse ProcessSDLEvent(const SDL_Event &event);

	bool m_active;

	RefCountedPtr<BindingContainer> m_bindingContainer;
};

namespace InputFWD {
	// These functions are here to avoid direct inclusion of Pi::input

	float GetMoveSpeedShiftModifier();

	// Return true if the behaviour is the current one and if at least a value is != 0,
	// second and third parameters are mouse movement relative coordinates
	std::tuple<bool, int, int> GetMouseMotion(MouseMotionBehaviour mmb);

	bool IsMouseYInvert();

	std::unique_ptr<InputFrameStatusTicket> DisableAllInputFrameExcept(InputFrame *current);
} // namespace InputFWD

#endif // INPUTFRAME_H
