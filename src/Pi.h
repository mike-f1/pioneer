// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _PI_H
#define _PI_H

#include "buildopts.h"

#include "libs/RefCounted.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

class Cutscene;
class Input;
class InputFrame;
class LuaConsole;
class LuaNameGen;
class PiGui;
class SystemPath;

class AsyncJobQueue;
class SyncJobQueue;
class JobQueue;

#if ENABLE_SERVER_AGENT
class ServerAgent;
#endif

namespace KeyBindings {
	struct ActionBinding;
}

namespace Graphics {
	class Texture;
	class RenderState;
	class RenderTarget;
	namespace Drawables {
		class TexturedQuad;
	}
} // namespace Graphics

namespace UI {
	class Context;
}

class Pi {
public:
	static void CreateRenderTarget(const uint16_t width, const uint16_t height);
	static void DrawRenderTarget();
	static void BeginRenderTarget();
	static void EndRenderTarget();

	static void Init(const std::map<std::string, std::string> &options, bool no_gui = false);
	static void InitGame();
	static void TerminateGame();
	static void StartGame();
	static void RequestEndGame(); // request that the game is ended as soon as safely possible
	static void EndGame();
	static void Start(const SystemPath &startPath);
	static void RequestQuit();
	static void MainLoop();
	static void CutSceneLoop(double step, Cutscene *cutscene);
	static void OnChangeDetailLevel();
	static float GetFrameTime() { return m_frameTime; }
	static float GetGameTickAlpha() { return m_gameTickAlpha; }

	static bool IsConsoleActive();

	static void SetMouseGrab(bool on);
	static bool DoingMouseGrab() { return doingMouseGrab; }

	static std::unique_ptr<LuaNameGen> m_luaNameGen;

#if ENABLE_SERVER_AGENT
	static ServerAgent *serverAgent;
#endif

	static RefCountedPtr<UI::Context> ui;
	static RefCountedPtr<PiGui> pigui;

	static void DrawPiGui(double delta, std::string handler);

	/* Only use #if WITH_DEVKEYS */
	static bool showDebugInfo;

#if PIONEER_PROFILER
	static std::string profilerPath;
	static bool doProfileSlow;
	static bool doProfileOne;
#endif

	static std::unique_ptr<Input> input;
	static std::unique_ptr<LuaConsole> m_luaConsole;

	static JobQueue *GetAsyncJobQueue();
	static JobQueue *GetSyncJobQueue();

private:
	enum class MainState {
		MAIN_MENU,
		GAME_START,
		TOMBSTONE,
		TO_GAME_START,
		TO_MAIN_MENU,
		TO_TOMBSTONE,
	};
	static MainState m_mainState;

	// msgs/requests that can be posted which the game processes at the end of a game loop in HandleRequests
	enum InternalRequests {
		END_GAME = 0,
		QUIT_GAME,
	};
	static void Quit() __attribute((noreturn));

	// Return true if there'a a need of further checks
	// (basically it should return true if a windows (as settings)
	// needs to be displayed, thus the event should be passed to
	// PiGui...
	// TODO: Right now this can't be "informed" from Lua if there's
	// a need to perform any operation, so it can't work correctly
	static bool HandleEscKey();

	static void HandleEvents();
	static void HandleRequests();

	static void RegisterInputBindings();
	// Key-press action feedback functions
	static void QuickSave(bool down);
	static void ScreenShot(bool down);
	static void ToggleVideoRecording(bool down);
	#ifdef WITH_DEVKEYS
	static void ToggleDebug(bool down);
	static void ReloadShaders(bool down);
	#endif // WITH_DEVKEYS
	#ifdef PIONEER_PROFILER
	static void ProfilerCommandOne(bool down);
	static void ProfilerCommandSlow(bool down);
	#endif // PIONEER_PROFILER
	#ifdef WITH_OBJECTVIEWER
	static void ObjectViewer(bool down);
	#endif // WITH_OBJECTVIEWER

	static struct PiBinding {
	public:
		using Action = KeyBindings::ActionBinding;

		Action *quickSave;
		Action *reqQuit;
		Action *screenShot;
		Action *toggleVideoRec;
		#ifdef WITH_DEVKEYS
		Action *toggleDebugInfo;
		Action *reloadShaders;
		#endif // WITH_DEVKEYS
		#ifdef PIONEER_PROFILER
		Action *profilerBindSlow;
		Action *profilerBindOne;
		#endif
		#ifdef WITH_OBJECTVIEWER
		Action *objectViewer;
		#endif // WITH_OBJECTVIEWER

	} m_piBindings;

	static std::unique_ptr<InputFrame> m_inputFrame;
	// private members
	static std::vector<InternalRequests> internalRequests;
	static const uint32_t SYNC_JOBS_PER_LOOP = 1;
	static std::unique_ptr<AsyncJobQueue> asyncJobQueue;
	static std::unique_ptr<SyncJobQueue> syncJobQueue;

	/** So, the game physics rate (50Hz) can run slower
	  * than the frame rate. m_gameTickAlpha is the interpolation
	  * factor between one physics tick and another [0.0-1.0]
	  */
	static float m_gameTickAlpha;
	static float m_frameTime;

	static std::unique_ptr<Cutscene> m_cutscene;

	static Graphics::RenderTarget *m_renderTarget;
	static RefCountedPtr<Graphics::Texture> m_renderTexture;
	static std::unique_ptr<Graphics::Drawables::TexturedQuad> m_renderQuad;
	static Graphics::RenderState *m_quadRenderState;

	static bool doingMouseGrab;

	static bool isRecordingVideo;
	static FILE *ffmpegFile;
};

#endif /* _PI_H */
