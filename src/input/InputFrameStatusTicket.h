#ifndef _INPUT_FRAME_STATUS_TICKET_H
#define _INPUT_FRAME_STATUS_TICKET_H

#include <map>
#include <vector>

class InputFrame;

// Simple wrapper used to store statuses of InputFrames at instantiation.
// When deleted, it automatically restore previous states
// TODO: would be good to catch if InputFrames are changed (e.g. size is changed) in the means
class InputFrameStatusTicket {
	friend class Input;
public:
	~InputFrameStatusTicket();
	InputFrameStatusTicket &operator =(InputFrameStatusTicket &) = delete;
	InputFrameStatusTicket(const InputFrameStatusTicket &) = delete;
private:
	InputFrameStatusTicket(const std::vector<InputFrame *> &inputFrames);
	std::map<InputFrame *, bool> m_statuses;
};

#endif /* _INPUT_FRAME_STATUS_TICKET_H */
