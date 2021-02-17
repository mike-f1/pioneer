#include "buildopts.h"

#include "InitState.h"

#include "MainMenuState.h"

#include "sphere/BaseSphere.h"
#include "CityOnPlanet.h"
#include "DebugInfo.h"
#include "EnumStrings.h"
#include "FaceParts.h"
#include "FileSystem.h"
#include "Game.h"
#include "GameLocator.h"
#include "GameConfig.h"
#include "GameConfSingleton.h"
#include "GameLog.h"
#include "GameState.h"
#include "InGameViews.h"
#include "InGameViewsLocator.h"
#include "JobQueue.h"
#include "Lang.h"
#include "LuaConsole.h"
#include "ModManager.h"
#include "OS.h"
#include "Beam.h"
#include "PiGui.h"
#include "Player.h"
#include "Projectile.h"
#include "SectorView.h"
#include "Sfx.h"
#include "Shields.h"
#include "ShipType.h"
#include "SpaceStation.h"
#include "Star.h"
#include "NavLights.h"
#include "ModelCache.h"
#include "Random.h"
#include "RandomSingleton.h"
#include "GameConfig.h"
#include "GameConfSingleton.h"
#include "VideoRecorder.h"
#include "PngWriter.h"

#include "../GameState.h"  // :P ...change file name or... well, wait for a complete rip? (TODO)

#include "gui/Gui.h"
#include "input/Input.h"
#include "input/InputFrame.h"
#include "input/InputLocator.h"
#include "libs/StringF.h"
#include "galaxy/GalaxyGenerator.h"
#include "graphics/opengl/RendererGL.h"
#include "graphics/Renderer.h"
#include "graphics/RendererLocator.h"
#include "sound/Sound.h"
#include "sound/SoundMusic.h"

#include "ui/Context.h"
#include "Pi.h"

#include "versioningInfo.h"

#if ENABLE_SERVER_AGENT
#include "ServerAgent.h"
#endif

// Here because of static initialization of InputFrames:
#include "ShipCpanelMultiFuncDisplays.h"
#include "InGameViews.h"
#include "ObjectViewerView.h"
#include "SectorView.h"
#include "SystemView.h"
#include "ship/ShipViewController.h"
#include "ship/PlayerShipController.h"

//#define WANT_SHIP_STAT 1 //TODO: move in an appropriate class

#if WANT_SHIP_STAT
#include "scenegraph/Model.h"
#include "CollMesh.h"
#endif

namespace MainState_ {

	static void draw_progress(float progress, RefCountedPtr<PiGui> pigui = Pi::pigui)
	{
		RendererLocator::getRenderer()->ClearScreen();
		{
			PiGuiFrameHelper piFH(pigui.Get(), RendererLocator::getRenderer()->GetSDLWindow());
			pigui->Render(progress, "INIT");
		}
		RendererLocator::getRenderer()->SwapBuffers();
	}

	InitState::InitState(const std::map<std::string, std::string> &options, bool no_gui):
		PiState(),
		m_options(options),
		m_no_gui(no_gui)
	{}

	InitState::~InitState()
	{}

	PiState *InitState::Update()
	{
	#ifdef PIONEER_PROFILER
		Profiler::reset();
	#endif

		Profiler::Timer timer;
		timer.Start();

		OS::EnableBreakpad();
		OS::NotifyLoadBegin();

		FileSystem::Init();
		FileSystem::userFiles.MakeDirectory(""); // ensure the config directory exists
	#ifdef PIONEER_PROFILER
		FileSystem::userFiles.MakeDirectory("profiler");
		m_statelessVars.profilerPath = FileSystem::JoinPathBelow(FileSystem::userFiles.GetRoot(), "profiler");
	#endif
		PROFILE_SCOPED()

		GameConfSingleton::Init(m_options);

		if (GameConfSingleton::getInstance().Int("RedirectStdio"))
			OS::RedirectStdio();

		std::string version(PIONEER_VERSION);
		if (strlen(PIONEER_EXTRAVERSION)) version += " (" PIONEER_EXTRAVERSION ")";
		const char *platformName = SDL_GetPlatform();
		if (platformName)
			Output("ver %s on: %s\n\n", version.c_str(), platformName);
		else
			Output("ver %s but could not detect platform name.\n\n", version.c_str());

		Output("%s\n", OS::GetOSInfoString().c_str());

		ModManager::Init();

		Lang::Resource res(Lang::GetResource("core", GameConfSingleton::getInstance().String("Lang")));
		Lang::MakeCore(res);

		// Initialize SDL
		uint32_t sdlInitFlags = SDL_INIT_VIDEO | SDL_INIT_JOYSTICK;
	#if defined(DEBUG) || defined(_DEBUG)
		sdlInitFlags |= SDL_INIT_NOPARACHUTE;
	#endif
		if (SDL_Init(sdlInitFlags) < 0) {
			Error("SDL initialization failed: %s\n", SDL_GetError());
		}

		OutputVersioningInfo();

		Graphics::RendererOGL::RegisterRenderer();

		// determine what renderer we should use, default to Opengl 3.x
		const std::string rendererName = GameConfSingleton::getInstance().String("RendererName", Graphics::RendererNameFromType(Graphics::RENDERER_OPENGL_3x));
		Graphics::RendererType rType = Graphics::RENDERER_OPENGL_3x;

		// Do rest of SDL video initialization and create Renderer
		Graphics::Settings videoSettings = {};
		videoSettings.rendererType = rType;
		videoSettings.width = GameConfSingleton::getInstance().Int("ScrWidth");
		videoSettings.height = GameConfSingleton::getInstance().Int("ScrHeight");
		videoSettings.fullscreen = (GameConfSingleton::getInstance().Int("StartFullscreen") != 0);
		videoSettings.hidden = m_no_gui;
		videoSettings.requestedSamples = GameConfSingleton::getInstance().Int("AntiAliasingMode");
		videoSettings.vsync = (GameConfSingleton::getInstance().Int("VSync") != 0);
		videoSettings.useTextureCompression = (GameConfSingleton::getInstance().Int("UseTextureCompression") != 0);
		videoSettings.useAnisotropicFiltering = (GameConfSingleton::getInstance().Int("UseAnisotropicFiltering") != 0);
		videoSettings.enableDebugMessages = (GameConfSingleton::getInstance().Int("EnableGLDebug") != 0);
		videoSettings.gl3ForwardCompatible = (GameConfSingleton::getInstance().Int("GL3ForwardCompatible") != 0);
		videoSettings.iconFile = OS::GetIconFilename();
		videoSettings.title = "Pioneer";

		RendererLocator::provideRenderer(Graphics::Init(videoSettings));

		Pi::CreateRenderTarget(videoSettings.width, videoSettings.height);
		RandomSingleton::Init(time(0));

		Output("Initialize Input\n");
		InputLocator::provideInput(new Input());
		std::vector<std::function<void(void)>> input_binding_registerer = {
			RadarWidget::RegisterInputBindings,
			InGameViews::RegisterInputBindings,
			ObjectViewerView::RegisterInputBindings,
			SectorView::RegisterInputBindings,
			SystemView::RegisterInputBindings,
			ShipViewController::RegisterInputBindings,
			PlayerShipController::RegisterInputBindings,
			InitState::RegisterInputBindings
		};
		InputLocator::getInput()->InitializeInputBindings(input_binding_registerer);

		// we can only do bindings once joysticks are initialised.
		if (!m_no_gui) // This re-saves the config file. With no GUI we want to allow multiple instances in parallel.
			KeyBindings::InitBindings();

		Pi::TestGPUJobsSupport();

		EnumStrings::Init();

		// get threads up
		uint32_t numThreads = GameConfSingleton::getInstance().Int("WorkerThreads");
		const int numCores = OS::GetNumCores();
		assert(numCores > 0);
		if (numThreads == 0) numThreads = std::max(uint32_t(numCores) - 1, 1U);
		Pi::asyncJobQueue.reset(new AsyncJobQueue(numThreads));
		Output("started %d worker threads\n", numThreads);
		Pi::syncJobQueue.reset(new SyncJobQueue);

		Output("ShipType::Init()\n");
		// XXX early, Lua init needs it
		ShipType::Init();

		// XXX UI requires Lua  but Pi::ui must exist before we start loading
		// templates. so now we have crap everywhere :/
		Output("Lua::Init()\n");
		Lua::Init();

		Pi::pigui.Reset(new PiGui(RendererLocator::getRenderer()->GetSDLWindow()));

		float ui_scale = GameConfSingleton::getInstance().Float("UIScaleFactor", 1.0f);
		if (Graphics::GetScreenHeight() < 768) {
			ui_scale = float(Graphics::GetScreenHeight()) / 768.0f;
		}

		Pi::ui.Reset(new UI::Context(
			Lua::manager,
			Graphics::GetScreenWidth(),
			Graphics::GetScreenHeight(),
			ui_scale));

	#ifdef ENABLE_SERVER_AGENT
		if (GameConfSingleton::getInstance().Int("EnableServerAgent")) {
			const std::string endpoint(GameConfSingleton::getInstance().String("ServerEndpoint"));
			if (!endpoint.empty()) {
				Output("Server agent enabled, endpoint: %s\n", endpoint.c_str());
				Pi::serverAgent = std::make_unique<HTTPServerAgent>(endpoint);
			}
		}
		if (!Pi::serverAgent) {
			Output("Server agent disabled\n");
			Pi::serverAgent = std::make_unique<NullServerAgent>();
		}
	#endif

		Pi::LuaInit();

		Gui::Init(Graphics::GetScreenWidth(), Graphics::GetScreenHeight(), 800, 600);

		// twice, to initialize the font correctly
		draw_progress(0.01f);
		draw_progress(0.01f);

		Output("GalaxyGenerator::Init()\n");
		if (GameConfSingleton::getInstance().HasEntry("GalaxyGenerator"))
			GalaxyGenerator::Init(GameConfSingleton::getInstance().String("GalaxyGenerator"),
				GameConfSingleton::getInstance().Int("GalaxyGeneratorVersion", GalaxyGenerator::LAST_VERSION));
		else
			GalaxyGenerator::Init();

		draw_progress(0.1f);

		Output("FaceParts::Init()\n");
		FaceParts::Init();
		draw_progress(0.2f);

		Output("Shields::Init()\n");
		Shields::Init();
		draw_progress(0.3f);

		Output("ModelCache::Init()\n");
		ModelCache::Init(ShipType::types);
		draw_progress(0.4f);

		//unsigned int control_word;
		//_clearfp();
		//_controlfp_s(&control_word, _EM_INEXACT | _EM_UNDERFLOW | _EM_ZERODIVIDE, _MCW_EM);
		//double fpexcept = Pi::timeAccelRates[1] / Pi::timeAccelRates[0];

		Output("BaseSphere::Init()\n");
		BaseSphere::Init(GameConfSingleton::getDetail().planets);
		draw_progress(0.5f);

		Output("CityOnPlanet::Init()\n");
		CityOnPlanet::Init();
		draw_progress(0.6f);

		Output("SpaceStation::Init()\n");
		SpaceStation::Init();
		draw_progress(0.7f);

		Output("NavLights::Init()\n");
		NavLights::Init();
		draw_progress(0.75f);

		Output("Sfx::Init()\n");
		SfxManager::Init();
		draw_progress(0.8f);

		if (!m_no_gui && !GameConfSingleton::getInstance().Int("DisableSound")) {
			Output("Sound::Init\n");
			Sound::Init();
			Sound::SetMasterVolume(GameConfSingleton::getInstance().Float("MasterVolume"));
			Sound::SetSfxVolume(GameConfSingleton::getInstance().Float("SfxVolume"));

			Sound::MusicPlayer::Init();
			Sound::MusicPlayer::SetVolume(GameConfSingleton::getInstance().Float("MusicVolume"));

			Sound::Pause(0);
			if (GameConfSingleton::getInstance().Int("MasterMuted")) Sound::Pause(1);
			if (GameConfSingleton::getInstance().Int("SfxMuted")) Sound::SetSfxVolume(0.f);
			if (GameConfSingleton::getInstance().Int("MusicMuted")) Sound::MusicPlayer::SetEnabled(false);
		}
		draw_progress(0.9f);

		OS::NotifyLoadEnd();
		draw_progress(0.95f);

	#if WANT_SHIP_STAT
		// test code to produce list of ship stats

		FILE *pStatFile = fopen("shipstat.csv","wt");
		if (pStatFile)
		{
			fprintf(pStatFile, "name,modelname,hullmass,capacity,fakevol,rescale,xsize,ysize,zsize,facc,racc,uacc,sacc,aacc,exvel\n");
			for (auto iter : ShipType::types)
			{
				const ShipType *shipdef = &(iter.second);
				SceneGraph::Model *model = ModelCache::FindModel(shipdef->modelName, false);

				double hullmass = shipdef->hullMass;
				double capacity = shipdef->capacity;

				double xsize = 0.0, ysize = 0.0, zsize = 0.0, fakevol = 0.0, rescale = 0.0, brad = 0.0;
				if (model) {
					std::unique_ptr<SceneGraph::Model> inst(model->MakeInstance());
					Aabb aabb = model->GetCollisionMesh()->GetAabb();
					xsize = aabb.max.x-aabb.min.x;
					ysize = aabb.max.y-aabb.min.y;
					zsize = aabb.max.z-aabb.min.z;
					fakevol = xsize*ysize*zsize;
					brad = aabb.GetRadius();
					rescale = pow(fakevol/(100 * (hullmass+capacity)), 0.3333333333);
				}

				double simass = (hullmass + capacity) * 1000.0;
				double angInertia = (2/5.0)*simass*brad*brad;
				double acc1 = shipdef->linThrust[Thruster::THRUSTER_FORWARD] / (9.81*simass);
				double acc2 = shipdef->linThrust[Thruster::THRUSTER_REVERSE] / (9.81*simass);
				double acc3 = shipdef->linThrust[Thruster::THRUSTER_UP] / (9.81*simass);
				double acc4 = shipdef->linThrust[Thruster::THRUSTER_RIGHT] / (9.81*simass);
				double acca = shipdef->angThrust/angInertia;
				double exvel = shipdef->effectiveExhaustVelocity;

				fprintf(pStatFile, "%s,%s,%.1f,%.1f,%.1f,%.3f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%f,%.1f\n",
					shipdef->name.c_str(), shipdef->modelName.c_str(), hullmass, capacity,
					fakevol, rescale, xsize, ysize, zsize, acc1, acc2, acc3, acc4, acca, exvel);
			}
			fclose(pStatFile);
		}
	#endif

		Pi::m_luaConsole.reset(new LuaConsole());

		draw_progress(1.0f);

		timer.Stop();
	#ifdef PIONEER_PROFILER
		Profiler::dumphtml(m_statelessVars.profilerPath.c_str());
	#endif
		Output("\n\nLoading took: %lf milliseconds\n", timer.millicycles());
		delete this;
		return new MainMenuState();
	}

	void InitState::RegisterInputBindings()
	{
		using namespace KeyBindings;
		using namespace std::placeholders;

		m_statelessVars.inputFrame = std::make_unique<InputFrame>("TweakAndSetting");

		auto &page = InputLocator::getInput()->GetBindingPage("General");

		auto &group = page.GetBindingGroup("Miscellaneous");

		m_piBindings.quickSave = m_statelessVars.inputFrame->AddActionBinding("BindQuickSave", group, ActionBinding(KeyBinding(SDLK_F9, KMOD_LCTRL)));
		m_statelessVars.inputFrame->AddCallbackFunction("BindQuickSave", std::bind(InitState::QuickSave, _1));

		m_piBindings.reqQuit = m_statelessVars.inputFrame->AddActionBinding("BindRequestQuit", group, ActionBinding(KeyBinding(SDLK_q, KMOD_LCTRL)));
		m_statelessVars.inputFrame->AddCallbackFunction("BindRequestQuit", [](bool down) { Pi::RequestQuit(); } );

		m_piBindings.screenShot = m_statelessVars.inputFrame->AddActionBinding("BindScreenshot", group, ActionBinding(KeyBinding(SDLK_a, KMOD_LCTRL)));
		m_statelessVars.inputFrame->AddCallbackFunction("BindScreenshot", std::bind(InitState::ScreenShot, _1));

		m_piBindings.toggleVideoRec = m_statelessVars.inputFrame->AddActionBinding("BindToggleVideoRec", group, ActionBinding(KeyBinding(SDLK_ASTERISK, KMOD_LCTRL)));
		m_statelessVars.inputFrame->AddCallbackFunction("BindToggleVideoRec", std::bind(InitState::ToggleVideoRecording, _1));

		auto &placeholderPage = InputLocator::getInput()->GetBindingPage("TweakAndSetting");
		placeholderPage.shouldBeTranslated = false;

		auto &placeholderGroup = placeholderPage.GetBindingGroup("None");

		// NOTE: All these bindings must use a modifier! Prefer CTRL over ALT or SHIFT
		#ifdef WITH_DEVKEYS
		m_piBindings.toggleDebugInfo = m_statelessVars.inputFrame->AddActionBinding("ToggleDebugInfo", placeholderGroup, ActionBinding(KeyBinding(SDLK_i, KMOD_LCTRL)));
		m_statelessVars.inputFrame->AddCallbackFunction("ToggleDebugInfo", [&](bool down) {
			if (down) return;
			if (m_statelessVars.debugInfo) {
				m_statelessVars.debugInfo.reset();
			} else {
				m_statelessVars.debugInfo = std::make_unique<DebugInfo>();
			}
			Output("On screen debug info is %s\n", m_statelessVars.debugInfo ? "shown" : "disabled");
		});

		m_piBindings.reloadShaders = m_statelessVars.inputFrame->AddActionBinding("ReloadShaders", placeholderGroup, ActionBinding(KeyBinding(SDLK_F11, KMOD_LCTRL)));
		m_statelessVars.inputFrame->AddCallbackFunction("ReloadShaders", [](bool down) { if (!down && RendererLocator::getRenderer()) RendererLocator::getRenderer()->ReloadShaders(); } );
		#endif // WITH_DEVKEYS

		#ifdef PIONEER_PROFILER
		m_piBindings.profilerBindOne = m_statelessVars.inputFrame->AddActionBinding("ProfilerOne", placeholderGroup, ActionBinding(KeyBinding(SDLK_p, KMOD_LCTRL)));
		m_statelessVars.inputFrame->AddCallbackFunction("ProfilerOne",  [&](bool down) { if (!down) m_statelessVars.doProfileOne = true; } );
		m_piBindings.profilerBindSlow = m_statelessVars.inputFrame->AddActionBinding("ProfilerSlow", placeholderGroup, ActionBinding(KeyBinding(SDLK_p, SDL_Keymod(KMOD_LCTRL | KMOD_LSHIFT))));
		m_statelessVars.inputFrame->AddCallbackFunction("ProfilerSlow", [&](bool down) {
			if (down) return;
			m_statelessVars.doProfileSlow = !m_statelessVars.doProfileSlow;
			Output("slow frame profiling %s\n", m_statelessVars.doProfileSlow ? "enabled" : "disabled");
		});
		#endif // PIONEER_PROFILER

		#ifdef WITH_OBJECTVIEWER
		m_piBindings.objectViewer = m_statelessVars.inputFrame->AddActionBinding("ObjectViewer", placeholderGroup, ActionBinding(KeyBinding(SDLK_F10, KMOD_LCTRL)));
		m_statelessVars.inputFrame->AddCallbackFunction("ObjectViewer", [](bool down) { if (!down && InGameViewsLocator::getInGameViews()) InGameViewsLocator::getInGameViews()->SetView(ViewType::OBJECT); });
		#endif // WITH_OBJECTVIEWER

		m_statelessVars.inputFrame->SetActive(true);
	}

	void InitState::QuickSave(bool down)
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

	void InitState::ScreenShot(bool down)
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

	void InitState::ToggleVideoRecording(bool down)
	{
		if (down) return;
		if (!m_statelessVars.videoRecorder) m_statelessVars.videoRecorder = std::make_unique<VideoRecorder>();
		else m_statelessVars.videoRecorder.reset();
	}

} // namespace MainState_
