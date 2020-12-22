#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "PiState.h"

namespace MainState_ {

	class GameState final: public PiState {
	public:
		GameState();
		virtual ~GameState();

		PiState *Update() override final;

	private:
		MainState MainLoop();

		void SetMouseGrab(bool on);

		bool m_doingMouseGrab;
	};

} // namespace MainState_

#endif // PISTATE_H
