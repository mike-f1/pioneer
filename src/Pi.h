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

class LuaConsole;
class LuaNameGen;
class PiGui;
class SystemPath;

namespace MainState_ {
	class PiState;
	class GameState;
}

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

enum class MainState;

class Pi {
	friend class MainState_::PiState;
	friend class MainState_::GameState;
public:
	static void CreateRenderTarget(const uint16_t width, const uint16_t height);
	static void DrawRenderTarget();
	static void BeginRenderTarget();
	static void EndRenderTarget();

	static void Init(const std::map<std::string, std::string> &options, bool no_gui = false);
	static void RequestEndGame(); // request that the game is ended as soon as safely possible
	static void Start(const SystemPath &startPath);
	static void RequestQuit();
	static void OnChangeDetailLevel();

	static std::unique_ptr<LuaNameGen> m_luaNameGen;

#if ENABLE_SERVER_AGENT
	static ServerAgent *serverAgent;
#endif

	static RefCountedPtr<UI::Context> ui;
	static RefCountedPtr<PiGui> pigui;

	static void DrawPiGui(double delta, std::string handler);

#if PIONEER_PROFILER
	static std::string profilerPath;
	static bool doProfileSlow;
	static bool doProfileOne;
#endif

	static std::unique_ptr<LuaConsole> m_luaConsole;

	static JobQueue *GetAsyncJobQueue();
	static JobQueue *GetSyncJobQueue();

private:
	// msgs/requests that can be posted which the game processes at the end of a game loop in HandleRequests
	enum InternalRequests {
		END_GAME = 0,
		QUIT_GAME,
	};
	static void Quit() __attribute((noreturn));

	static void HandleRequests(MainState &current);

	static void RegisterInputBindings();
	// private members
	static std::vector<InternalRequests> internalRequests;
	static std::unique_ptr<AsyncJobQueue> asyncJobQueue;
	static std::unique_ptr<SyncJobQueue> syncJobQueue;

	static Graphics::RenderTarget *m_renderTarget;
	static RefCountedPtr<Graphics::Texture> m_renderTexture;
	static std::unique_ptr<Graphics::Drawables::TexturedQuad> m_renderQuad;
	static Graphics::RenderState *m_quadRenderState;

	static bool isRecordingVideo;
	static FILE *ffmpegFile;
};

#endif /* _PI_H */
