#ifndef QUIT_STATE_H
#define QUIT_STATE_H

#include "PiState.h"

namespace MainState_ {

	class QuitState: public PiState {
	public:
		QuitState() {};
		~QuitState() {};

		PiState *Update() override final;

	private:
	};
} // namespace MainState_

#endif // QUIT_STATE_H
