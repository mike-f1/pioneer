#include "PiState.h"

#include "Cutscene.h"
#include "DebugInfo.h"
#include "Game.h"
#include "GameLocator.h"
#include "GameLog.h"
#include "GameConfSingleton.h"
#include "GameConfig.h"
#include "GameSaveError.h"
#include "../GameState.h"  // :P ...change file name or... well, wait for a complete rip?
#include "InGameViews.h"
#include "InGameViewsLocator.h"
#include "Intro.h"
#include "Lang.h"
#include "LuaConsole.h"
#include "PngWriter.h"
#include "Tombstone.h"
#include "graphics/RendererLocator.h"
#include "graphics/Renderer.h"
#include "input/InputLocator.h"
#include "input/Input.h"
#include "input/InputFrame.h"
#include "input/KeyBindings.h"
#include "libs/StringF.h"

#include "gui/Gui.h"
#include "ui/Context.h"
#include "PiGui.h"

#include "GameState.h"
#include "../GameState.h"

#include "Pi.h"

#include <SDL_timer.h>
#include <SDL_events.h>

#if !defined(_MSC_VER) && !defined(__MINGW32__)
#define _popen popen
#define _pclose pclose
#endif

namespace MainState_ {

	PiState::PiState()
	{
		RegisterInputBindings();
#if WITH_DEVKEYS
		if (m_debugInfo)  m_debugInfo->NewCycle();
#endif
	}

	PiState::~PiState()
	{}

	void PiState::CutSceneLoop(MainState &current, double step, Cutscene *m_cutscene)
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
			if (m_debugInfo)  m_debugInfo->IncreaseFrame();
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

		if (dynamic_cast<Intro *>(m_cutscene) != nullptr) {
			PiGuiFrameHelper piFH(Pi::pigui.Get(), RendererLocator::getRenderer()->GetSDLWindow());
			bool active = true;
			if (Pi::m_luaConsole) active = !Pi::m_luaConsole->IsActive();

			if (active)
				Pi::pigui->Render(step, "MAINMENU");

#if WITH_DEVKEYS
			if (m_debugInfo)  {
				m_debugInfo->Update();
				m_debugInfo->Print();
			}
#endif
		}

		Pi::EndRenderTarget();

		// render the rendertarget texture
		Pi::DrawRenderTarget();
		RendererLocator::getRenderer()->SwapBuffers();

		Pi::HandleRequests(current);

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

	void PiState::RegisterInputBindings()
	{
		using namespace KeyBindings;
		using namespace std::placeholders;

		m_inputFrame = std::make_unique<InputFrame>("TweakAndSetting");

		auto &page = InputLocator::getInput()->GetBindingPage("TweakAndSetting");
		page.shouldBeTranslated = false;

		auto &group = page.GetBindingGroup("None");

		// NOTE: All these bindings must use a modifier! Prefer CTRL over ALT or SHIFT
		m_piBindings.quickSave = m_inputFrame->AddActionBinding("QuickSave", group, ActionBinding(KeyBinding(SDLK_F9, KMOD_LCTRL)));
		m_inputFrame->AddCallbackFunction("QuickSave", std::bind(&PiState::QuickSave, this, _1));

		m_piBindings.reqQuit = m_inputFrame->AddActionBinding("RequestQuit", group, ActionBinding(KeyBinding(SDLK_q, KMOD_LCTRL)));
		m_inputFrame->AddCallbackFunction("RequestQuit", [](bool down) { Pi::RequestQuit(); } );

		m_piBindings.screenShot = m_inputFrame->AddActionBinding("Screenshot", group, ActionBinding(KeyBinding(SDLK_a, KMOD_LCTRL)));
		m_inputFrame->AddCallbackFunction("Screenshot", std::bind(&PiState::ScreenShot, this, _1));

		m_piBindings.toggleVideoRec = m_inputFrame->AddActionBinding("ToggleVideoRec", group, ActionBinding(KeyBinding(SDLK_ASTERISK, KMOD_LCTRL)));
		m_inputFrame->AddCallbackFunction("ToggleVideoRec", std::bind(&PiState::ToggleVideoRecording, this, _1));

		#ifdef WITH_DEVKEYS
		m_piBindings.toggleDebugInfo = m_inputFrame->AddActionBinding("ToggleDebugInfo", group, ActionBinding(KeyBinding(SDLK_i, KMOD_LCTRL)));
		m_inputFrame->AddCallbackFunction("ToggleDebugInfo", [&](bool down) {
			if (down) return;
			if (m_debugInfo) {
				m_debugInfo.reset();
			} else {
				m_debugInfo = std::make_unique<DebugInfo>();
			}
		});

		m_piBindings.reloadShaders = m_inputFrame->AddActionBinding("ReloadShaders", group, ActionBinding(KeyBinding(SDLK_F11, KMOD_LCTRL)));
		m_inputFrame->AddCallbackFunction("ReloadShaders", [](bool down) { if (!down && RendererLocator::getRenderer()) RendererLocator::getRenderer()->ReloadShaders(); } );
		#endif // WITH_DEVKEYS

		#ifdef PIONEER_PROFILER
		m_piBindings.profilerBindOne = m_inputFrame->AddActionBinding("ProfilerOne", group, ActionBinding(KeyBinding(SDLK_p, KMOD_LCTRL)));
		m_inputFrame->AddCallbackFunction("ProfilerOne",  [](bool down) { if (!down) Pi::doProfileOne = true; } );
		m_piBindings.profilerBindSlow = m_inputFrame->AddActionBinding("ProfilerSlow", group, ActionBinding(KeyBinding(SDLK_p, SDL_Keymod(KMOD_LCTRL | KMOD_LSHIFT))));
		m_inputFrame->AddCallbackFunction("ProfilerSlow", [](bool down) {
			if (down) return;
			Pi::doProfileSlow = !Pi::doProfileSlow;
			Output("slow frame profiling %s\n", Pi::doProfileSlow ? "enabled" : "disabled");
		});
		#endif // PIONEER_PROFILER

		#ifdef WITH_OBJECTVIEWER
		m_piBindings.objectViewer = m_inputFrame->AddActionBinding("ObjectViewer", group, ActionBinding(KeyBinding(SDLK_F10, KMOD_LCTRL)));
		m_inputFrame->AddCallbackFunction("ObjectViewer", [](bool down) { if (!down && InGameViewsLocator::getInGameViews()) InGameViewsLocator::getInGameViews()->SetView(ViewType::OBJECT); });
		#endif // WITH_OBJECTVIEWER

		m_inputFrame->SetActive(true);
	}

	void PiState::QuickSave(bool down)
	{
		if (down) return;
		if (!GameLocator::getGame()) return;
		if (GameLocator::getGame()->IsHyperspace()) {
			GameLocator::getGame()->GetGameLog().Add(Lang::CANT_SAVE_IN_HYPERSPACE);
		} else {
			const std::string name = "_quicksave";
			const std::string path = FileSystem::JoinPath(GameConfSingleton::GetSaveDirFull(), name);
			try {
				GameStateStatic::SaveGame(name);
				Output("Quick save: %s\n", name.c_str());
				GameLocator::getGame()->GetGameLog().Add(Lang::GAME_SAVED_TO + path);
			} catch (CouldNotOpenFileException) {
				GameLocator::getGame()->GetGameLog().Add(stringf(Lang::COULD_NOT_OPEN_FILENAME, formatarg("path", path)));
			} catch (CouldNotWriteToFileException) {
				GameLocator::getGame()->GetGameLog().Add(Lang::GAME_SAVE_CANNOT_WRITE);
			}
		}
	}

	void PiState::ScreenShot(bool down)
	{
		if (down) return;
		char buf[256];
		const time_t t = time(0);
		struct tm *_tm = localtime(&t);
		strftime(buf, sizeof(buf), "screenshot-%Y%m%d-%H%M%S.png", _tm);
		Graphics::ScreendumpState sd;
		RendererLocator::getRenderer()->Screendump(sd);
		PngWriter::write_screenshot(sd, buf);
	}

	void PiState::ToggleVideoRecording(bool down)
	{
		if (down) return;
		Pi::isRecordingVideo = !Pi::isRecordingVideo;
		if (Pi::isRecordingVideo) {
			char videoName[256];
			const time_t t = time(0);
			struct tm *_tm = localtime(&t);
			strftime(videoName, sizeof(videoName), "pioneer-%Y%m%d-%H%M%S", _tm);
			const std::string dir = "videos";
			FileSystem::userFiles.MakeDirectory(dir);
			const std::string fname = FileSystem::JoinPathBelow(FileSystem::userFiles.GetRoot() + "/" + dir, videoName);
			Output("Video Recording started to %s.\n", fname.c_str());
			// start ffmpeg telling it to expect raw rgba 720p-60hz frames
			// -i - tells it to read frames from stdin
			// if given no frame rate (-r 60), it will just use vfr
			char cmd[256] = { 0 };
			snprintf(cmd, sizeof(cmd), "ffmpeg -f rawvideo -pix_fmt rgba -s %dx%d -i - -threads 0 -preset fast -y -pix_fmt yuv420p -crf 21 -vf vflip %s.mp4",
				GameConfSingleton::getInstance().Int("ScrWidth"),
				GameConfSingleton::getInstance().Int("ScrHeight"),
				fname.c_str()
				);

			// open pipe to ffmpeg's stdin in binary write mode
	#if defined(_MSC_VER) || defined(__MINGW32__)
			Pi::ffmpegFile = _popen(cmd, "wb");
	#else
			Pi::ffmpegFile = _popen(cmd, "w");
	#endif
		} else {
			Output("Video Recording ended.\n");
			if (Pi::ffmpegFile != nullptr) {
				_pclose(Pi::ffmpegFile);
				Pi::ffmpegFile = nullptr;
			}
		}
	}

} // namespace MainState
