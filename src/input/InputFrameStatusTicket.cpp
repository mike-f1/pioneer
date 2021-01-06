#include "InputFrameStatusTicket.h"

#include "InputFrame.h"

#include <algorithm>

InputFrameStatusTicket::InputFrameStatusTicket(const std::vector<InputFrame *> &inputFrames)
{
	std::for_each(begin(inputFrames), end(inputFrames), [this](InputFrame *iframe) {
		m_statuses[iframe] = iframe->IsActive();
	});
}

InputFrameStatusTicket::~InputFrameStatusTicket()
{
	std::for_each(begin(m_statuses), end(m_statuses), [](std::pair<InputFrame *, bool> old_status) {
		old_status.first->SetActive(old_status.second);
	});
}
