#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "PiState.h"

namespace MainState_ {

	class GameState: public PiState {
	public:
		GameState();
		~GameState();

		PiState *Update() override final;

	private:
		MainState MainLoop();

		void SetMouseGrab(bool on);

		bool m_doingMouseGrab;
	};

} // namespace MainState
#endif // PISTATE_H
