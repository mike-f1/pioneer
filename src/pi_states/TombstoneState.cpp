#include "TombstoneState.h"

#include "Intro.h"
#include "Game.h"
#include "GameLocator.h"
#include "Tombstone.h"
#include "input/InputLocator.h"
#include "input/Input.h"
#include "graphics/RendererLocator.h"
#include "graphics/Renderer.h"

#include "MainMenuState.h"
#include "QuitState.h"

#include <SDL_timer.h>
#include <SDL_events.h>

#include "Pi.h"

namespace MainState_ {
	TombstoneState::TombstoneState() :
		PiState(),
		m_time(0.0),
		m_last_time(SDL_GetTicks())
	{
		m_cutScene = std::make_unique<Tombstone>(Graphics::GetScreenWidth(),
			Graphics::GetScreenHeight()
			);
	}

	TombstoneState::~TombstoneState()
	{
	}

	PiState *TombstoneState::Update()
	{
		m_statelessVars.frameTime = 0.001f * (SDL_GetTicks() - m_last_time);
		m_last_time = SDL_GetTicks();

		m_time += m_statelessVars.frameTime;
		CutSceneLoop(m_statelessVars.frameTime, m_cutScene.get());
		MainState current = MainState::TOMBSTONE;
		Pi::HandleRequests(current);

		if (m_time > 5.0 && InputLocator::getInput()->IsAnyKeyJustPressed()) {
			delete this;
			return new MainMenuState();
		} else if (current == MainState::TOMBSTONE) {
			return this;
		} else if (current == MainState::QUITTING) {
			delete this;
			return new QuitState();
		}
		return this;
	}

} // namespace MainState
