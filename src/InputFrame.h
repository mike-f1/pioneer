#ifndef INPUTFRAME_H
#define INPUTFRAME_H

#include <string>
#include <utility>
#include <vector>
#include <SDL_events.h>

struct BindingGroup;

namespace KeyBindings {
	struct ActionBinding;
	struct AxisBinding;
};

enum class InputResponse {
	// None of the inputs match the event.
	NOMATCH,
	// An input matched, but won't consume the event.
	PASSTHROUGH,
	// An input matched and consumed the event.
	MATCHED
};

class InputFrame {
	friend class Input;
public:
	InputFrame() = delete;
	InputFrame(const InputFrame &) = delete;

	InputFrame(const std::string &name) :
		m_name(name),
		m_active(false)
	{}

	~InputFrame();

	bool IsActive() const { return m_active; }
	void SetActive(bool is_active) { m_active = is_active; }

	KeyBindings::ActionBinding *AddActionBinding(std::string id, BindingGroup &group, KeyBindings::ActionBinding binding);
	KeyBindings::AxisBinding *AddAxisBinding(std::string id, BindingGroup &group, KeyBindings::AxisBinding binding);

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
	InputResponse ProcessSDLEvent(const SDL_Event &event);
};

#endif // INPUTFRAME_H
