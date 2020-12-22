#include "PiState.h"

#include "Cutscene.h"
#include "DebugInfo.h"
#include "Game.h"
#include "GameLocator.h"
#include "GameLog.h"
#include "GameConfSingleton.h"
#include "GameConfig.h"
#include "GameSaveError.h"
#include "InGameViews.h"
#include "InGameViewsLocator.h"
#include "Intro.h"
#include "Lang.h"
#include "LuaConsole.h"
#include "Tombstone.h"
#include "VideoRecorder.h"
#include "graphics/RendererLocator.h"
#include "graphics/Renderer.h"
#include "input/InputLocator.h"
#include "input/Input.h"
#include "input/InputFrame.h"
#include "input/KeyBindings.h"

#include "gui/Gui.h"
#include "ui/Context.h"
#include "PiGui.h"

#include "GameState.h"
#include "../GameState.h"

#include "Pi.h"

#include <SDL_timer.h>
#include <SDL_events.h>

#if ENABLE_SERVER_AGENT
#include "ServerAgent.h"
#endif

#if !defined(_MSC_VER) && !defined(__MINGW32__)
#define _popen popen
#define _pclose pclose
#endif

namespace MainState_ {

	PiState::StateLessVar PiState::m_statelessVars = PiState::StateLessVar();

	PiState::PiState()
	{
#if WITH_DEVKEYS
		if (m_statelessVars.debugInfo)  m_statelessVars.debugInfo->NewCycle();
#endif
	}

	PiState::~PiState()
	{}

	void PiState::CutSceneLoop(double step, Cutscene *m_cutscene)
	{
		// XXX hack
		// if we hit our exit conditions then ignore further queued events
		// protects against eg double-click during game generation
		if (GameLocator::getGame()) {
			SDL_Event event;
			while (SDL_PollEvent(&event)) {
			}
		}
#if WITH_DEVKEYS
		if (m_statelessVars.debugInfo)  m_statelessVars.debugInfo->IncreaseFrame();
#endif

		Pi::BeginRenderTarget();
		RendererLocator::getRenderer()->BeginFrame();
		m_cutscene->Draw(step);
		RendererLocator::getRenderer()->EndFrame();

		RendererLocator::getRenderer()->ClearDepthBuffer();

		// Mainly for Console
		Pi::ui->Update();
		Pi::ui->Draw();

		HandleEvents();

		Gui::Draw();

		{
			PiGuiFrameHelper piFH(Pi::pigui.Get(), RendererLocator::getRenderer()->GetSDLWindow());
			bool active = true;
			if (Pi::m_luaConsole) active = !Pi::m_luaConsole->IsActive();

			if (active && dynamic_cast<Intro *>(m_cutscene) != nullptr)
				Pi::pigui->Render(step, "MAINMENU");

#if WITH_DEVKEYS
			if (m_statelessVars.debugInfo)  {
				m_statelessVars.debugInfo->Update();
				m_statelessVars.debugInfo->Print();
			}
#endif
		}

		Pi::EndRenderTarget();

		// render the rendertarget texture
		Pi::DrawRenderTarget();
		RendererLocator::getRenderer()->SwapBuffers();

	#ifdef ENABLE_SERVER_AGENT
		Pi::serverAgent->ProcessResponses();
	#endif
	}

	bool PiState::HandleEscKey()
	{
		if (Pi::m_luaConsole && Pi::m_luaConsole->IsActive()) {
			Pi::m_luaConsole->Deactivate();
			return false;
		}

		if (!InGameViewsLocator::getInGameViews()) return true;
		else return InGameViewsLocator::getInGameViews()->HandleEscKey();
	}

	void PiState::HandleEvents()
	{
		PROFILE_SCOPED()
		SDL_Event event;

		// XXX for most keypresses SDL will generate KEYUP/KEYDOWN and TEXTINPUT
		// events. keybindings run off KEYUP/KEYDOWN. the console is opened/closed
		// via keybinding. the console TextInput widget uses TEXTINPUT events. thus
		// after switching the console, the stray TEXTINPUT event causes the
		// console key (backtick) to appear in the text entry field. we hack around
		// this by setting this flag if the console was switched. if its set, we
		// swallow the TEXTINPUT event this hack must remain until we have a
		// unified input system
		bool skipTextInput = false;

		InputLocator::getInput()->ResetFrameInput();
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				Pi::RequestQuit();
			}

			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
				if (!HandleEscKey()) continue;
			}

			Pi::pigui->ProcessEvent(&event);

			if (Pi::pigui->WantCaptureMouse()) {
				// don't process mouse event any further, imgui already handled it
				switch (event.type) {
				case SDL_MOUSEBUTTONDOWN:
				case SDL_MOUSEBUTTONUP:
				case SDL_MOUSEWHEEL:
				case SDL_MOUSEMOTION:
					continue;
				default: break;
				}
			}
			if (Pi::pigui->WantCaptureKeyboard()) {
				// don't process keyboard event any further, imgui already handled it
				switch (event.type) {
				case SDL_KEYDOWN:
				case SDL_KEYUP:
				case SDL_TEXTINPUT:
					continue;
				default: break;
				}
			}
			if (skipTextInput && event.type == SDL_TEXTINPUT) {
				skipTextInput = false;
				continue;
			}
			if (Pi::ui->DispatchSDLEvent(event))
				continue;

			bool consoleActive = true;
			if (Pi::m_luaConsole) consoleActive = Pi::m_luaConsole->IsActive();

			Gui::HandleSDLEvent(&event);
			InputLocator::getInput()->HandleSDLEvent(event);

			if (consoleActive != Pi::m_luaConsole->IsActive()) {
				skipTextInput = true;
				continue;
			}
		}
	}
} // namespace MainState
