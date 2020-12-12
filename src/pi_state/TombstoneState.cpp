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

#include <SDL_timer.h>
#include <SDL_events.h>

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
		m_frameTime = 0.001f * (SDL_GetTicks() - m_last_time);
		m_last_time = SDL_GetTicks();

		m_time += m_frameTime;
		MainState current = MainState::TOMBSTONE;
		CutSceneLoop(current, m_frameTime, m_cutScene.get());

		if (m_time > 5.0 && InputLocator::getInput()->IsAnyKeyJustPressed()) {
			delete this;
			return new MainMenuState();
		} else if (current == MainState::TOMBSTONE) {
			return this;
		} else if (current == MainState::QUITTING) {
			delete this;
			return new QuittingState();
		}
		return this;
	}

} // namespace MainState
