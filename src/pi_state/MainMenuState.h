#ifndef MAIN_MENU_STATE_H
#define MAIN_MENU_STATE_H

#include "PiState.h"

#include <cstdint>
#include <memory>

class Intro;

namespace MainState_ {

	class MainMenuState: public PiState {
	public:
		MainMenuState();
		~MainMenuState();

		PiState *Update() override final;

	private:
		std::unique_ptr<Intro> m_cutScene;
		uint32_t m_last_time;
	};

} // namespace MainState
#endif // MAIN_MENU_STATE_H
