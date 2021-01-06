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
	class InitState;
	class QuitState;
}

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

enum class MainState;

class Pi {
	friend class MainState_::PiState;
	friend class MainState_::GameState;
	friend class MainState_::InitState;
	friend class MainState_::QuitState;
public:
	Pi(const std::map<std::string, std::string> &options, const SystemPath &startPath, bool no_gui = false);
	~Pi();

	static void RequestEndGame();
	static void RequestQuit();
	static void OnChangeDetailLevel();

	static std::unique_ptr<LuaNameGen> m_luaNameGen;

#if ENABLE_SERVER_AGENT
	static std::unique_ptr<ServerAgent> serverAgent;
#endif

	static RefCountedPtr<UI::Context> ui;
	static RefCountedPtr<PiGui> pigui;

	static std::unique_ptr<LuaConsole> m_luaConsole;

	static JobQueue *GetAsyncJobQueue();
	static JobQueue *GetSyncJobQueue();

	static void HandleRequests(MainState &current);

private:
	// msgs/requests that can be posted which the game processes at the end of a game loop in HandleRequests
	enum InternalRequests {
		END_GAME = 0,
		QUIT_GAME,
	};

	static void LuaInit();

	static void CreateRenderTarget(const uint16_t width, const uint16_t height);
	static void DrawRenderTarget();
	static void BeginRenderTarget();
	static void EndRenderTarget();

	static void TestGPUJobsSupport();

	static std::vector<InternalRequests> m_internalRequests;
	static std::unique_ptr<AsyncJobQueue> asyncJobQueue;
	static std::unique_ptr<SyncJobQueue> syncJobQueue;

	static Graphics::RenderTarget *m_renderTarget;
	static RefCountedPtr<Graphics::Texture> m_renderTexture;
	static std::unique_ptr<Graphics::Drawables::TexturedQuad> m_renderQuad;
	static Graphics::RenderState *m_quadRenderState;
};

#endif /* _PI_H */
