#include "buildopts.h"

#include "GameState.h"
#include "MainMenuState.h"
#include "TombstoneState.h"

#include "DeathView.h"
#include "DebugInfo.h"
#include "Frame.h"
#include "Game.h"
#include "GameLocator.h"
#include "GameConfSingleton.h"
#include "../GameState.h"  // :P ...change file name or... well, wait for a complete rip?
#include "InGameViews.h"
#include "InGameViewsLocator.h"
#include "LuaConsole.h"
#include "LuaEvent.h"
#include "Pi.h"
#include "Player.h"
#include "ShipCpanel.h"
#include "Space.h"
#include "WorldView.h"
#include "graphics/Renderer.h"
#include "graphics/RendererLocator.h"
#include "input/Input.h"
#include "input/InputLocator.h"
#include "sound/AmbientSounds.h"
#include "sound/SoundMusic.h"
#include "sphere/BaseSphere.h"

#include "PiGui.h"

#include "gui/Gui.h"
#include "ui/Context.h"

#include "profiler/Profiler.h"

#include <SDL_timer.h>
#include <SDL_events.h>

namespace MainState_ {

	constexpr uint32_t SYNC_JOBS_PER_LOOP = 1;

	static void OnPlayerDockOrUndock()
	{
		GameLocator::getGame()->RequestTimeAccel(Game::TIMEACCEL_1X);
		GameLocator::getGame()->SetTimeAccel(Game::TIMEACCEL_1X);
	}

	GameState::GameState():
		PiState(),
		m_doingMouseGrab(false)
	{
		// this is a bit brittle. skank may be forgotten and survive between
		// games

		if (!GameConfSingleton::getInstance().Int("DisableSound")) AmbientSounds::Init();

		LuaEvent::Clear();

		GameLocator::getGame()->GetPlayer()->onDock.connect(sigc::ptr_fun(&OnPlayerDockOrUndock));
		GameLocator::getGame()->GetPlayer()->onUndock.connect(sigc::ptr_fun(&OnPlayerDockOrUndock));
		GameLocator::getGame()->GetPlayer()->onLanded.connect(sigc::ptr_fun(&OnPlayerDockOrUndock));
		InGameViewsLocator::getInGameViews()->GetCpan()->ShowAll();
		InGameViewsLocator::getInGameViews()->SetView(ViewType::WORLD);

#ifdef REMOTE_LUA_REPL
#ifndef REMOTE_LUA_REPL_PORT
#define REMOTE_LUA_REPL_PORT 12345
#endif
		m_luaConsole->OpenTCPDebugConnection(REMOTE_LUA_REPL_PORT);
#endif

		// fire event before the first frame
		//TODO: onGameStart is for game load and for game start...
		LuaEvent::Queue("onGameStart");
		LuaEvent::Emit();
	}

	GameState::~GameState()
	{
		SetMouseGrab(false);

		Sound::MusicPlayer::Stop();
		Sound::DestroyAllEvents();

		// final event
		LuaEvent::Queue("onGameEnd");
		LuaEvent::Emit();

		if (!GameConfSingleton::getInstance().Int("DisableSound")) AmbientSounds::Uninit();
		Sound::DestroyAllEvents();

		assert(GameLocator::getGame());

		GameStateStatic::DestroyGame();

		Lua::manager->CollectGarbage();
	}

	PiState *GameState::Update()
	{
		MainState ret = MainLoop();
		if (ret == MainState::GAME_LOOP) return this;
		else if (ret == MainState::TOMBSTONE) {
			delete this;
			return new TombstoneState();
		} else if (ret == MainState::MAIN_MENU) {
			delete this;
			return new MainMenuState();
		} else if (ret == MainState::QUITTING) {
			delete this;
			return new QuittingState();
		}
		return this;
	}

	MainState GameState::MainLoop()
	{
		double time_player_died = 0;
#if WITH_DEVKEYS
		if (m_debugInfo)  m_debugInfo->NewCycle();
#endif

		int MAX_PHYSICS_TICKS = GameConfSingleton::getInstance().Int("MaxPhysicsCyclesPerRender");
		if (MAX_PHYSICS_TICKS <= 0)
			MAX_PHYSICS_TICKS = 4;

		double currentTime = 0.001 * double(SDL_GetTicks());
		double accumulator = GameLocator::getGame()->GetTimeStep();
		m_gameTickAlpha = 0;

#ifdef PIONEER_PROFILER
		Profiler::reset();
#endif

		while (GameLocator::getGame()) {
			PROFILE_SCOPED()

#ifdef ENABLE_SERVER_AGENT
			Pi::serverAgent->ProcessResponses();
#endif

			const uint32_t newTicks = SDL_GetTicks();
			double newTime = 0.001 * double(newTicks);
			m_frameTime = newTime - currentTime;
			if (m_frameTime > 0.25) m_frameTime = 0.25;
			currentTime = newTime;
			accumulator += m_frameTime * GameLocator::getGame()->GetTimeAccelRate();

			const float step = GameLocator::getGame()->GetTimeStep();
			if (step > 0.0f) {
				PROFILE_SCOPED_RAW("unpaused")
				int phys_ticks = 0;
				while (accumulator >= step) {
					if (++phys_ticks >= MAX_PHYSICS_TICKS) {
						accumulator = 0.0;
						break;
					}
					GameLocator::getGame()->TimeStep(step);
					InGameViewsLocator::getInGameViews()->GetCpan()->TimeStepUpdate(step);

					BaseSphere::UpdateAllBaseSphereDerivatives();

					accumulator -= step;
				}
				// rendering interpolation between frames: don't use when docked
				// TODO: Investigate the real needs of these lines...
				int pstate = GameLocator::getGame()->GetPlayer()->GetFlightState();
				if (pstate == Ship::DOCKED || pstate == Ship::DOCKING || pstate == Ship::UNDOCKING)
					m_gameTickAlpha = 1.0;
				else
					m_gameTickAlpha = accumulator / step;

#if WITH_DEVKEYS
				if (m_debugInfo)  m_debugInfo->IncreasePhys(phys_ticks);
#endif
			} else {
				// paused
				PROFILE_SCOPED_RAW("paused")
				BaseSphere::UpdateAllBaseSphereDerivatives();
			}
#if WITH_DEVKEYS
			if (m_debugInfo)  m_debugInfo->IncreaseFrame();
#endif

			// did the player die?
			if (GameLocator::getGame()->GetPlayer()->IsDead()) {
				if (time_player_died > 0.0) {
					if (GameLocator::getGame()->GetTime() - time_player_died > 8.0) {
						InGameViewsLocator::getInGameViews()->SetView(ViewType::NONE);
						return MainState::TOMBSTONE;
					}
				} else {
					GameLocator::getGame()->SetTimeAccel(Game::TIMEACCEL_1X);
					InGameViewsLocator::getInGameViews()->GetDeathView()->Init();
					InGameViewsLocator::getInGameViews()->SetView(ViewType::DEATH);
					time_player_died = GameLocator::getGame()->GetTime();
				}
			}

			Pi::BeginRenderTarget();
			RendererLocator::getRenderer()->SetViewport(0, 0, Graphics::GetScreenWidth(), Graphics::GetScreenHeight());
			RendererLocator::getRenderer()->BeginFrame();
			RendererLocator::getRenderer()->SetTransform(matrix4x4f::Identity());

			/* Calculate position for this rendered frame (interpolated between two physics ticks */
			// XXX should this be here? what is this anyway?
			for (Body *b : GameLocator::getGame()->GetSpace()->GetBodies()) {
				b->UpdateInterpTransform(m_gameTickAlpha);
			}

			Frame::GetRootFrame()->UpdateInterpTransform(m_gameTickAlpha);

			InGameViewsLocator::getInGameViews()->UpdateView(m_frameTime);
			InGameViewsLocator::getInGameViews()->Draw3DView();

			// hide cursor for ship control. Do this before imgui runs, to prevent the mouse pointer from jumping
			SetMouseGrab(InputLocator::getInput()->MouseButtonState(SDL_BUTTON_RIGHT) | InputLocator::getInput()->MouseButtonState(SDL_BUTTON_MIDDLE));

			// XXX HandleEvents at the moment must be after view->Draw3D and before
			// Gui::Draw so that labels drawn to screen can have mouse events correctly
			// detected. Gui::Draw wipes memory of label positions.
			HandleEvents();

#ifdef REMOTE_LUA_REPL
			m_luaConsole->HandleTCPDebugConnections();
#endif

			RendererLocator::getRenderer()->EndFrame();

			RendererLocator::getRenderer()->ClearDepthBuffer();

			if (InGameViewsLocator::getInGameViews()->ShouldDrawGui()) {
				Gui::Draw();
			}

			// XXX don't draw the UI during death obviously a hack, and still
			// wrong, because we shouldn't this when the HUD is disabled, but
			// probably sure draw it if they switch to eg infoview while the HUD is
			// disabled so we need much smarter control for all this rubbish
			if ((!GameLocator::getGame() || InGameViewsLocator::getInGameViews()->GetViewType() != ViewType::DEATH) && InGameViewsLocator::getInGameViews()->ShouldDrawGui()) {
				Pi::ui->Update();
				Pi::ui->Draw();
			}

			Pi::EndRenderTarget();
			Pi::DrawRenderTarget();
			if (GameLocator::getGame() && !GameLocator::getGame()->GetPlayer()->IsDead()) {
				// FIXME: Always begin a camera frame because WorldSpaceToScreenSpace
				// requires it and is exposed to pigui.
				InGameViewsLocator::getInGameViews()->GetWorldView()->BeginCameraFrame();
				bool active = true;
				if (Pi::m_luaConsole) active = !Pi::m_luaConsole->IsActive();
				if (active) {
					PiGuiFrameHelper pifh(Pi::pigui.Get(), RendererLocator::getRenderer()->GetSDLWindow(), InGameViewsLocator::getInGameViews()->ShouldDrawGui());

					InGameViewsLocator::getInGameViews()->DrawUI(m_frameTime);
					Pi::pigui->Render(m_frameTime, "GAME");
#if WITH_DEVKEYS
					if (m_debugInfo)  {
						m_debugInfo->Update();
						m_debugInfo->Print();
					}
#endif
					Pi::pigui->EndFrame();
				}
				InGameViewsLocator::getInGameViews()->GetWorldView()->EndCameraFrame();
			}

			RendererLocator::getRenderer()->SwapBuffers();

			// game exit will have cleared GameLocator::getGame(). we can't continue.
			if (!GameLocator::getGame()) {
				return MainState::MAIN_MENU;
			}

			if (GameLocator::getGame()->UpdateTimeAccel())
				accumulator = 0; // fix for huge pauses 10000x -> 1x

			if (!GameLocator::getGame()->GetPlayer()->IsDead()) {
				// XXX should this really be limited to while the player is alive?
				// this is something we need not do every turn...
				if (!GameConfSingleton::getInstance().Int("DisableSound")) AmbientSounds::Update();
			}
			InGameViewsLocator::getInGameViews()->GetCpan()->Update();
			Sound::MusicPlayer::Update();

			Pi::syncJobQueue->RunJobs(SYNC_JOBS_PER_LOOP);
			Pi::asyncJobQueue->FinishJobs();
			Pi::syncJobQueue->FinishJobs();

			MainState have_new_state = MainState::GAME_LOOP;
			Pi::HandleRequests(have_new_state);
			if (have_new_state != MainState::GAME_LOOP) return have_new_state;

#ifdef PIONEER_PROFILER
			const uint32_t profTicks = SDL_GetTicks();
			if (Pi::doProfileOne || (Pi::doProfileSlow && (profTicks - newTicks) > 100)) { // slow: < ~10fps
				Output("dumping profile data\n");
				Profiler::dumphtml(Pi::profilerPath.c_str());
				Pi::doProfileOne = false;
			}
#endif // PIONEER_PROFILER

			if (Pi::isRecordingVideo && (Pi::ffmpegFile != nullptr)) {
				Graphics::ScreendumpState sd;
				RendererLocator::getRenderer()->Screendump(sd);
				fwrite(sd.pixels.get(), sizeof(uint32_t) * RendererLocator::getRenderer()->GetWindowWidth() * RendererLocator::getRenderer()->GetWindowHeight(), 1, Pi::ffmpegFile);
			}

#ifdef PIONEER_PROFILER
			Profiler::reset();
#endif
		}
		return MainState::MAIN_MENU;
	}

	void GameState::SetMouseGrab(bool on)
	{
		if (!m_doingMouseGrab && on) {
			RendererLocator::getRenderer()->SetGrab(true);
			Pi::ui->SetMousePointerEnabled(false);
			Pi::pigui->DoMouseGrab(true);
			m_doingMouseGrab = true;
		} else if (m_doingMouseGrab && !on) {
			RendererLocator::getRenderer()->SetGrab(false);
			Pi::ui->SetMousePointerEnabled(true);
			Pi::pigui->DoMouseGrab(false);
			m_doingMouseGrab = false;
		}
	}

} // namespace MainState
