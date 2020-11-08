#ifndef INPUTFRAME_H
#define INPUTFRAME_H

#include <memory>
#include <string>
#include <utility>
#include <vector>

struct BindingGroup;
struct BindingPage;
union SDL_Event;
enum class MouseMotionBehaviour;
class InputFrameStatusTicket;

namespace KeyBindings {
	struct ActionBinding;
	struct AxisBinding;
	enum class InputResponse;
}

class InputFrame {
	friend class Input;
public:
	InputFrame() = delete;
	InputFrame(const InputFrame &) = delete;

	InputFrame(const std::string &name) ;
	virtual ~InputFrame();

	bool IsActive() const { return m_active; }
	void SetActive(bool is_active);

	KeyBindings::ActionBinding *AddActionBinding(std::string id, BindingGroup &group, KeyBindings::ActionBinding binding);
	KeyBindings::AxisBinding *AddAxisBinding(std::string id, BindingGroup &group, KeyBindings::AxisBinding binding);

	BindingPage &GetBindingPage(const std::string &id);

	// Call this at startup and register all the bindings associated with the frame.
	virtual void RegisterBindings() {};

	// Called when the frame is added to the stack.
	virtual void onFrameAdded() {};

	// Called when the frame is removed from the stack.
	virtual void onFrameRemoved() {};

private:
	std::string m_name;

	bool m_active;

	typedef std::pair<std::string, KeyBindings::ActionBinding *> TActionPair;
	typedef std::pair<std::string, KeyBindings::AxisBinding *> TAxisPair;
	std::vector<TActionPair> m_actions;
	std::vector<TAxisPair> m_axes;

	// Check the event against all the inputs in this frame.
	KeyBindings::InputResponse ProcessSDLEvent(const SDL_Event &event);
};

namespace InputFWD {
	// These functions are here to avoid direct inclusion of Pi::input

	float GetMoveSpeedShiftModifier();

	// Return true if the behaviour is the current one and if at least a value is != 0,
	// second and third parameters are mouse movement relative coordinates
	std::tuple<bool, int, int> GetMouseMotion(MouseMotionBehaviour mmb);

	bool IsMouseYInvert();

	std::unique_ptr<InputFrameStatusTicket> DisableAllInputFramesExcept(InputFrame *current);
} // namespace InputFWD

#endif // INPUTFRAME_H
