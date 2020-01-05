#ifndef INPUTFRAME_H
#define INPUTFRAME_H

#include <vector>
#include <SDL_events.h>

namespace KeyBindings {
	struct ActionBinding;
	struct AxisBinding;
	struct WheelBinding;
}

enum InputResponse {
	// None of the inputs match the event.
	RESPONSE_NOMATCH = 0,
	// An input matched, but won't consume the event.
	RESPONSE_PASSTHROUGH,
	// An input matched and consumed the event.
	RESPONSE_MATCHED
};

struct InputFrame {
	std::vector<KeyBindings::ActionBinding *> actions;
	std::vector<KeyBindings::AxisBinding *> axes;
	KeyBindings::WheelBinding *wheel = nullptr;

	bool active;

	// Call this at startup and register all the bindings associated with the frame.
	virtual void RegisterBindings(){};

	// Called when the frame is added to the stack.
	virtual void onFrameAdded(){};

	// Called when the frame is removed from the stack.
	virtual void onFrameRemoved(){};

	// Check the event against all the inputs in this frame.
	InputResponse ProcessSDLEvent(const SDL_Event &event);
};

#endif // INPUTFRAME_H
