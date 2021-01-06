#include "MainMenuState.h"

#include "Intro.h"
#include "Game.h"
#include "GameLocator.h"
#include "GameConfSingleton.h"
#include "graphics/RendererLocator.h"
#include "graphics/Renderer.h"

#include "GameState.h"
#include "QuitState.h"

#include <SDL_timer.h>
#include <SDL_events.h>

#include "Pi.h"

namespace MainState_ {

	MainMenuState::MainMenuState():
		PiState()
	{
		m_cutScene = std::make_unique<Intro>(Graphics::GetScreenWidth(),
			Graphics::GetScreenHeight(),
			GameConfSingleton::GetAmountBackgroundStars()
		);
		m_last_time = SDL_GetTicks();
	}

	MainMenuState::~MainMenuState()
	{
	}

	PiState *MainMenuState::Update()
	{
		m_statelessVars.frameTime = 0.001f * (SDL_GetTicks() - m_last_time);
		m_last_time = SDL_GetTicks();

		CutSceneLoop(m_statelessVars.frameTime, m_cutScene.get());
		MainState current = MainState::MAIN_MENU;
		Pi::HandleRequests(current);
		if (GameLocator::getGame() != nullptr) {
			delete this;
			return new GameState();
		}
		if (current == MainState::MAIN_MENU) return this;
		else if (current == MainState::QUITTING) {
			delete this;
			return new QuitState();
		}
		return this;
	}

} // namespace MainState
