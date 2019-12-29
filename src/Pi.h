// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _PI_H
#define _PI_H

#include "Input.h"
#include "gameconsts.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

class Cutscene;
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
	static void Init(const std::map<std::string, std::string> &options, bool no_gui = false);
	static void InitGame();
	static void StartGame();
	static void RequestEndGame(); // request that the game is ended as soon as safely possible
	static void EndGame();
	static void Start(const SystemPath &startPath);
	static void RequestQuit();
	static void MainLoop();
	static void CutSceneLoop(double step, Cutscene *cutscene);
	static void OnChangeDetailLevel();
	static float GetFrameTime() { return frameTime; }
	static float GetGameTickAlpha() { return gameTickAlpha; }

	static bool IsConsoleActive();

	static void SetMouseGrab(bool on);
	static bool DoingMouseGrab() { return doingMouseGrab; }

	static void CreateRenderTarget(const Uint16 width, const Uint16 height);
	static void DrawRenderTarget();
	static void BeginRenderTarget();
	static void EndRenderTarget();

	static std::unique_ptr<LuaNameGen> luaNameGen;

#if ENABLE_SERVER_AGENT
	static ServerAgent *serverAgent;
#endif

	static RefCountedPtr<UI::Context> ui;
	static RefCountedPtr<PiGui> pigui;

	static int statSceneTris;
	static int statNumPatches;

	static void DrawPiGui(double delta, std::string handler);

	/* Only use #if WITH_DEVKEYS */
	static bool showDebugInfo;

#if PIONEER_PROFILER
	static std::string profilerPath;
	static bool doProfileSlow;
	static bool doProfileOne;
#endif

	static Input input;
	static std::unique_ptr<LuaConsole> luaConsole;

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
	static void HandleKeyDown(const SDL_Keysym &key);
	static void HandleEvents();
	static void HandleRequests();
	static void HandleEscKey();

	// private members
	static std::vector<InternalRequests> internalRequests;
	static const Uint32 SYNC_JOBS_PER_LOOP = 1;
	static std::unique_ptr<AsyncJobQueue> asyncJobQueue;
	static std::unique_ptr<SyncJobQueue> syncJobQueue;

	static bool menuDone;

	/** So, the game physics rate (50Hz) can run slower
	  * than the frame rate. gameTickAlpha is the interpolation
	  * factor between one physics tick and another [0.0-1.0]
	  */
	static float gameTickAlpha;
	static float frameTime;

	static std::unique_ptr<Cutscene> m_cutscene;

	static Graphics::RenderTarget *renderTarget;
	static RefCountedPtr<Graphics::Texture> renderTexture;
	static std::unique_ptr<Graphics::Drawables::TexturedQuad> renderQuad;
	static Graphics::RenderState *quadRenderState;

	static bool doingMouseGrab;

	static bool isRecordingVideo;
	static FILE *ffmpegFile;
};

#endif /* _PI_H */
