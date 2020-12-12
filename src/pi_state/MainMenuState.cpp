#include "MainMenuState.h"

#include "Intro.h"
#include "Game.h"
#include "GameLocator.h"
#include "GameConfSingleton.h"
#include "graphics/RendererLocator.h"
#include "graphics/Renderer.h"

#include "GameState.h"

#include <SDL_timer.h>
#include <SDL_events.h>

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
		m_frameTime = 0.001f * (SDL_GetTicks() - m_last_time);
		m_last_time = SDL_GetTicks();

		MainState current = MainState::MAIN_MENU;
		CutSceneLoop(current, m_frameTime, m_cutScene.get());
		if (GameLocator::getGame() != nullptr) {
			delete this;
			return new GameState();
		}
		if (current == MainState::MAIN_MENU) return this;
		else if (current == MainState::QUITTING) {
			delete this;
			return new QuittingState();
		}
		return this;
	}

} // namespace MainState
