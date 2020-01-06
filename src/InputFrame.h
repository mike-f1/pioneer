#ifndef INPUTFRAME_H
#define INPUTFRAME_H

#include <vector>
#include <SDL_events.h>
#include <sigc++/sigc++.h>

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

class InputFrame {
	friend class Input;
public:
	//InputFrame() = delete;

	// Call this at startup and register all the bindings associated with the frame.
	virtual void RegisterBindings(){};

	// Called when the frame is added to the stack.
	virtual void onFrameAdded(){};

	// Called when the frame is removed from the stack.
	virtual void onFrameRemoved(){};

	std::vector<KeyBindings::ActionBinding *> actions;
	std::vector<KeyBindings::AxisBinding *> axes;
	KeyBindings::WheelBinding *wheel = nullptr;

	// TODO: Find a way to store pointers to function on each binding
	// (lambda, functor or whatever but they must be on a single function)
	//void AddActionBinding(...blahblahyaddayaddadu... pointer to function for each signal);
	//std::vector<sigc::connection> signals;
	// Basically with the above we achieve a better encapsulation, thus each
	// InputFrame can calls whatever functions, but having InputFrame made static
	// because of initialization is an obstacle :P

	bool active;

private:
	// Check the event against all the inputs in this frame.
	InputResponse ProcessSDLEvent(const SDL_Event &event);

};

#endif // INPUTFRAME_H
