#include "QuitState.h"

#include "Pi.h"
#include "LuaManager.h"
#include "LuaNameGen.h"
#include "Projectile.h"
#include "Beam.h"
#include "NavLights.h"
#include "Shields.h"
#include "Sfx.h"
#include "sound/Sound.h"
#include "CityOnPlanet.h"
#include "JobQueue.h"
#include "gui/Gui.h"
#include "ui/Context.h"
#include "PiGui.h"
#include "sphere/BaseSphere.h"
#include "FaceParts.h"
#include "graphics/Renderer.h"
#include "graphics/RendererLocator.h"
#include "input/LuaInputFrames.h"
#include "galaxy/GalaxyGenerator.h"

#if !defined(_MSC_VER) && !defined(__MINGW32__)
#define _popen popen
#define _pclose pclose
#endif

namespace MainState_ {

	static void LuaUninit()
	{
		Pi::m_luaNameGen.reset();

		Lua::Uninit();
	}

	PiState *QuitState::Update()
	{
		Output("Shutting down...\n");
		Projectile::FreeModel();
		Beam::FreeModel();
		NavLights::Uninit();
		Shields::Uninit();
		SfxManager::Uninit();
		Sound::Uninit();
		CityOnPlanet::Uninit();
		BaseSphere::Uninit();
		FaceParts::Uninit();
		Graphics::Uninit();
		Pi::pigui.Reset(nullptr);
		Pi::ui.Reset(nullptr);
		Pi::pigui.Reset(nullptr);
		// TODO: remove explicit Reset of LuaInputFrames
		LuaInputFrames::Reset();
		LuaUninit();
		Gui::Uninit();
		delete RendererLocator::getRenderer();
		GalaxyGenerator::Uninit();
		SDL_Quit();
		FileSystem::Uninit();
		Pi::asyncJobQueue.reset();
		Pi::syncJobQueue.reset();

		delete this;
		return nullptr;
	}

} // namespace MainState_
