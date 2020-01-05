#include "InputFrame.h"

#include "KeyBindings.h"

InputResponse InputFrame::ProcessSDLEvent(const SDL_Event &event)
{
	bool matched = false;

	for (KeyBindings::ActionBinding *action : actions) {
		auto resp = action->CheckSDLEventAndDispatch(event);
		if (resp == RESPONSE_MATCHED) return resp;
		matched = matched || resp > RESPONSE_NOMATCH;
	}

	for (KeyBindings::AxisBinding *axis : axes) {
		auto resp = axis->CheckSDLEventAndDispatch(event);
		if (resp == RESPONSE_MATCHED) return resp;
		matched = matched || resp > RESPONSE_NOMATCH;
	}

	if (wheel != nullptr) {
		auto resp = wheel->CheckSDLEventAndDispatch(event);
		if (resp == RESPONSE_MATCHED) return resp;
		matched = matched || resp > RESPONSE_NOMATCH;
	}
	return matched ? RESPONSE_PASSTHROUGH : RESPONSE_NOMATCH;
}
