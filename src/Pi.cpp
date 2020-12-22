// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Pi.h"

#include "sphere/BaseSphere.h"
#include "GameConfSingleton.h"
#include "GameState.h"
#include "JobQueue.h"
#include "LuaColor.h"
#include "LuaComms.h"
#include "LuaConsole.h"
#include "LuaConstants.h"
#include "LuaDev.h"
#include "LuaEngine.h"
#include "LuaFileSystem.h"
#include "LuaFormat.h"
#include "LuaGame.h"
#include "input/LuaInput.h"
#include "input/LuaInputFrames.h"
#include "LuaJson.h"
#include "LuaLang.h"
#include "LuaManager.h"
#include "LuaMusic.h"
#include "LuaNameGen.h"
#include "LuaSerializer.h"
#include "LuaShipDef.h"
#include "LuaSpace.h"
#include "LuaTimer.h"
#include "LuaVector.h"
#include "LuaVector2.h"
#include "OS.h"
#include "pi_state/PiState.h"
#include "pi_state/InitState.h"
#include "pi_state/GameState.h"
#include "pi_state/MainMenuState.h"

#if ENABLE_SERVER_AGENT
#include "LuaServerAgent.h"
#include "ServerAgent.h"
#endif

#include "PiGui.h"

#include "Missile.h"
#include "Player.h"
#include "Shields.h"
#include "ShipType.h"
#include "SpaceStation.h"
#include "Star.h"
#include "Random.h"
#include "RandomSingleton.h"
#include "gameui/Lua.h"
#include "galaxy/Faction.h"
#include "galaxy/StarSystem.h"
#include "libs/libs.h"
#include "libs/StringF.h"
#include "pigui/PiGuiLua.h"
#include "sound/Sound.h"
#include "sound/SoundMusic.h"

#include "gui/Gui.h"

#include "graphics/opengl/RendererGL.h"
#include "graphics/Renderer.h"
#include "graphics/RendererLocator.h"
#include "graphics/Texture.h"

#include "scenegraph/Lua.h"
#include "versioningInfo.h"

#ifdef PROFILE_LUA_TIME
#include <time.h>
#endif

#if defined(_MSC_VER) || defined(__MINGW32__)
// RegisterClassA and RegisterClassW are defined as macros in WinUser.h
#ifdef RegisterClass
#undef RegisterClass
#endif
#endif

std::unique_ptr<LuaNameGen> Pi::m_luaNameGen;
#ifdef ENABLE_SERVER_AGENT
std::unique_ptr<ServerAgent> Pi::serverAgent;
#endif
std::unique_ptr<LuaConsole> Pi::m_luaConsole;
RefCountedPtr<UI::Context> Pi::ui;
RefCountedPtr<PiGui> Pi::pigui;
Graphics::RenderTarget *Pi::m_renderTarget;
RefCountedPtr<Graphics::Texture> Pi::m_renderTexture;
std::unique_ptr<Graphics::Drawables::TexturedQuad> Pi::m_renderQuad;
Graphics::RenderState *Pi::m_quadRenderState = nullptr;
std::vector<Pi::InternalRequests> Pi::m_internalRequests;

std::unique_ptr<AsyncJobQueue> Pi::asyncJobQueue;
std::unique_ptr<SyncJobQueue> Pi::syncJobQueue;

Pi::Pi(const std::map<std::string, std::string> &options, const SystemPath &startPath, bool no_gui)
{
	MainState_::PiState *piState = new MainState_::InitState(options, no_gui);
	piState = piState->Update(); // <- normally Update 'advance' to MainMenuState
	if (startPath != SystemPath(0, 0, 0, 0, 0)) {
		// If start point is present, delete MainMenuState and move to game state
		// TODO: easy to add an option to load directly a savegame
		delete piState;
		GameStateStatic::MakeNewGame(startPath);
		piState =  new MainState_::GameState();
	}

	//XXX global ambient colour hack to make explicit the old default ambient colour dependency
	// for some models
	RendererLocator::getRenderer()->SetAmbientColor(Color(51, 51, 51, 255));

	while (piState) {
		piState = piState->Update();
	}
}

Pi::~Pi()
{}

// Leaving define in place in case of future rendering problems.
#define USE_RTT 0

// static
JobQueue *Pi::GetAsyncJobQueue() { return asyncJobQueue.get(); }
// static
JobQueue *Pi::GetSyncJobQueue() { return syncJobQueue.get(); }

//static
void Pi::CreateRenderTarget(const uint16_t width, const uint16_t height)
{
	/*	@fluffyfreak here's a rendertarget implementation you can use for oculusing and other things. It's pretty simple:
		 - fill out a RenderTargetDesc struct and call Renderer::CreateRenderTarget
		 - pass target to Renderer::SetRenderTarget to start rendering to texture
		 - set up viewport, clear etc, then draw as usual
		 - SetRenderTarget(0) to resume render to screen
		 - you can access the attached texture with GetColorTexture to use it with a material
		You can reuse the same target with multiple textures.
		In that case, leave the color format to NONE so the initial texture is not created, then use SetColorTexture to attach your own.
	*/
#if USE_RTT
	Graphics::RenderStateDesc rsd;
	rsd.depthTest = false;
	rsd.depthWrite = false;
	rsd.blendMode = Graphics::BLEND_SOLID;
	m_quadRenderState = RendererLocator::getRenderer()->CreateRenderState(rsd);

	Graphics::TextureDescriptor texDesc(
		Graphics::TEXTURE_RGBA_8888,
		vector2f(width, height),
		Graphics::LINEAR_CLAMP, false, false, 0);
	m_renderTexture.Reset(RendererLocator::getRenderer()->CreateTexture(texDesc));
	m_renderQuad.reset(new Graphics::Drawables::TexturedQuad(
		RendererLocator::getRenderer(), m_renderTexture.Get(),
		vector2f(0.0f, 0.0f), vector2f(float(Graphics::GetScreenWidth()), float(Graphics::GetScreenHeight())),
		m_quadRenderState));

	// Complete the RT description so we can request a buffer.
	// NB: we don't want it to create use a texture because we share it with the textured quad created above.
	Graphics::RenderTargetDesc rtDesc(
		width,
		height,
		Graphics::TEXTURE_NONE, // don't create a texture
		Graphics::TEXTURE_DEPTH,
		false);
	m_renderTarget = RendererLocator::getRenderer()->CreateRenderTarget(rtDesc);

	m_renderTarget->SetColorTexture(m_renderTexture.Get());
#endif
}

//static
void Pi::DrawRenderTarget()
{
#if USE_RTT
	RendererLocator::getRenderer()->BeginFrame();
	RendererLocator::getRenderer()->SetViewport(0, 0, Graphics::GetScreenWidth(), Graphics::GetScreenHeight());
	RendererLocator::getRenderer()->SetTransform(matrix4x4f::Identity());

	{
		RendererLocator::getRenderer()->SetMatrixMode(Graphics::MatrixMode::PROJECTION);
		RendererLocator::getRenderer()->PushMatrix();
		RendererLocator::getRenderer()->SetOrthographicProjection(0, Graphics::GetScreenWidth(), Graphics::GetScreenHeight(), 0, -1, 1);
		RendererLocator::getRenderer()->SetMatrixMode(Graphics::MatrixMode::MODELVIEW);
		RendererLocator::getRenderer()->PushMatrix();
		RendererLocator::getRenderer()->LoadIdentity();
	}

	m_renderQuad->Draw(RendererLocator::getRenderer());

	{
		RendererLocator::getRenderer()->SetMatrixMode(Graphics::MatrixMode::PROJECTION);
		RendererLocator::getRenderer()->PopMatrix();
		RendererLocator::getRenderer()->SetMatrixMode(Graphics::MatrixMode::MODELVIEW);
		RendererLocator::getRenderer()->PopMatrix();
	}

	RendererLocator::getRenderer()->EndFrame();
#endif
}

//static
void Pi::BeginRenderTarget()
{
#if USE_RTT
	RendererLocator::getRenderer()->SetRenderTarget(m_renderTarget);
	RendererLocator::getRenderer()->ClearScreen();
#endif
}

//static
void Pi::EndRenderTarget()
{
#if USE_RTT
	RendererLocator::getRenderer()->SetRenderTarget(nullptr);
#endif
}

void Pi::LuaInit()
{
	PROFILE_SCOPED()
	LuaObject<PropertiedObject>::RegisterClass();

	LuaObject<Body>::RegisterClass();
	LuaObject<Ship>::RegisterClass();
	LuaObject<SpaceStation>::RegisterClass();
	LuaObject<Planet>::RegisterClass();
	LuaObject<Star>::RegisterClass();
	LuaObject<Player>::RegisterClass();
	LuaObject<Missile>::RegisterClass();
	LuaObject<CargoBody>::RegisterClass();
	LuaObject<ModelBody>::RegisterClass();
	LuaObject<HyperspaceCloud>::RegisterClass();

	LuaObject<StarSystem>::RegisterClass();
	LuaObject<SystemPath>::RegisterClass();
	LuaObject<SystemBody>::RegisterClass();
	LuaObject<Random>::RegisterClass();
	LuaObject<Faction>::RegisterClass();
	LuaObject<InputFrame>::RegisterClass();

	LuaObject<LuaSerializer>::RegisterClass();
	LuaObject<LuaTimer>::RegisterClass();

	LuaConstants::Register(Lua::manager->GetLuaState());
	LuaLang::Register();
	LuaEngine::Register();
	LuaInput::Register();
	LuaFileSystem::Register();
	LuaJson::Register();
#ifdef ENABLE_SERVER_AGENT
	LuaServerAgent::Register();
#endif
	LuaGame::Register();
	LuaComms::Register();
	LuaFormat::Register();
	LuaSpace::Register();
	LuaShipDef::Register();
	LuaMusic::Register();
	LuaDev::Register();
	LuaConsole::Register();
	LuaVector::Register(Lua::manager->GetLuaState());
	LuaVector2::Register(Lua::manager->GetLuaState());
	LuaColor::Register(Lua::manager->GetLuaState());

	// XXX sigh
	UI::Lua::Init();
	GameUI::Lua::Init();
	SceneGraph::Lua::Init();

	LuaObject<PiGui>::RegisterClass();
	PiGUI::Lua::Init();

	// XXX load everything. for now, just modules
	lua_State *l = Lua::manager->GetLuaState();
	pi_lua_import(l, "libs/autoload.lua", true);
	pi_lua_import_recursive(l, "ui");
	pi_lua_import(l, "pigui/pigui.lua", true);
	pi_lua_import_recursive(l, "pigui/modules");
	pi_lua_import_recursive(l, "pigui/views");
	pi_lua_import_recursive(l, "modules");

	Pi::m_luaNameGen.reset(new LuaNameGen());
}

void Pi::TestGPUJobsSupport()
{
	bool supportsGPUJobs = (GameConfSingleton::getInstance().Int("EnableGPUJobs") == 1);
	if (supportsGPUJobs) {
		uint32_t octaves = 8;
		for (uint32_t i = 0; i < 6; i++) {
			std::unique_ptr<Graphics::Material> material;
			Graphics::MaterialDescriptor desc;
			desc.effect = Graphics::EffectType::GEN_GASGIANT_TEXTURE;
			desc.quality = (octaves << 16) | i;
			desc.textures = 3;
			material.reset(RendererLocator::getRenderer()->CreateMaterial(desc));
			supportsGPUJobs &= material->IsProgramLoaded();
		}
		if (!supportsGPUJobs) {
			// failed - retry

			// reset the GPU jobs flag
			supportsGPUJobs = true;

			// retry the shader compilation
			octaves = 5; // reduce the number of octaves
			for (uint32_t i = 0; i < 6; i++) {
				std::unique_ptr<Graphics::Material> material;
				Graphics::MaterialDescriptor desc;
				desc.effect = Graphics::EffectType::GEN_GASGIANT_TEXTURE;
				desc.quality = (octaves << 16) | i;
				desc.textures = 3;
				material.reset(RendererLocator::getRenderer()->CreateMaterial(desc));
				supportsGPUJobs &= material->IsProgramLoaded();
			}

			if (!supportsGPUJobs) {
				// failed
				Warning("EnableGPUJobs DISABLED");
				GameConfSingleton::getInstance().SetInt("EnableGPUJobs", 0); // disable GPU Jobs
				GameConfSingleton::getInstance().Save();
			}
		}
	}
}

void Pi::OnChangeDetailLevel()
{
	BaseSphere::OnChangeDetailLevel(GameConfSingleton::getDetail().planets);
}

void Pi::HandleRequests(MainState &current)
{
	for (auto request : m_internalRequests) {
		switch (request) {
		case END_GAME:
			current = MainState::MAIN_MENU;
			break;
		case QUIT_GAME:
			current = MainState::QUITTING;
			break;
		default:
			Output("Pi::HandleRequests, unhandled request type processed.\n");
			break;
		}
	}
	m_internalRequests.clear();
}

void Pi::RequestEndGame()
{
	m_internalRequests.push_back(END_GAME);
}

void Pi::RequestQuit()
{
	m_internalRequests.push_back(QUIT_GAME);
}
