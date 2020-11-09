// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Pi.h"

#include "sphere/BaseSphere.h"
#include "CityOnPlanet.h"
#include "DeathView.h"
#include "DebugInfo.h"
#include "EnumStrings.h"
#include "FaceParts.h"
#include "FileSystem.h"
#include "Frame.h"
#include "Game.h"
#include "GameLocator.h"
#include "GameConfig.h"
#include "GameConfSingleton.h"
#include "GameLog.h"
#include "GameState.h"
#include "InGameViews.h"
#include "InGameViewsLocator.h"
#include "Input.h"
#include "Intro.h"
#include "JobQueue.h"
#include "KeyBindings.h"
#include "Lang.h"
#include "LuaColor.h"
#include "LuaComms.h"
#include "LuaConsole.h"
#include "LuaConstants.h"
#include "LuaDev.h"
#include "LuaEngine.h"
#include "LuaEvent.h"
#include "LuaFileSystem.h"
#include "LuaFormat.h"
#include "LuaGame.h"
#include "LuaInput.h"
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
#include "Missile.h"
#include "ModManager.h"
#include "ModelCache.h"
#include "NavLights.h"
#include "OS.h"
#include "PngWriter.h"
#include "sound/AmbientSounds.h"
#if WITH_OBJECTVIEWER
#include "ObjectViewerView.h"
#endif // WITH_OBJECTVIEWER
#include "Beam.h"
#include "PiGui.h"
#include "Player.h"
#include "Projectile.h"
#include "SectorView.h"
#include "Sfx.h"
#include "Shields.h"
#include "ShipCpanel.h"
#include "ShipType.h"
#include "Space.h"
#include "SpaceStation.h"
#include "Star.h"
#include "libs/StringF.h"
#include "Random.h"
#include "RandomSingleton.h"
#include "Tombstone.h"
#include "WorldView.h"
#include "galaxy/GalaxyGenerator.h"
#include "gameui/Lua.h"
#include "libs/libs.h"
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

#if !defined(_MSC_VER) && !defined(__MINGW32__)
#define _popen popen
#define _pclose pclose
#endif

float Pi::m_gameTickAlpha;
std::unique_ptr<LuaNameGen> Pi::m_luaNameGen;
std::unique_ptr<InputFrame> Pi::m_inputFrame;
Pi::PiBinding Pi::m_piBindings;
#ifdef ENABLE_SERVER_AGENT
ServerAgent *Pi::serverAgent;
#endif
std::unique_ptr<Input> Pi::input;
std::unique_ptr<LuaConsole> Pi::m_luaConsole;
float Pi::m_frameTime;
bool Pi::doingMouseGrab;
#if WITH_DEVKEYS
bool Pi::showDebugInfo = false;
#endif
#if PIONEER_PROFILER
std::string Pi::profilerPath;
bool Pi::doProfileSlow = false;
bool Pi::doProfileOne = false;
#endif
RefCountedPtr<UI::Context> Pi::ui;
RefCountedPtr<PiGui> Pi::pigui;
std::unique_ptr<Cutscene> Pi::m_cutscene;
Graphics::RenderTarget *Pi::m_renderTarget;
RefCountedPtr<Graphics::Texture> Pi::m_renderTexture;
std::unique_ptr<Graphics::Drawables::TexturedQuad> Pi::m_renderQuad;
Graphics::RenderState *Pi::m_quadRenderState = nullptr;
std::vector<Pi::InternalRequests> Pi::internalRequests;
bool Pi::isRecordingVideo = false;
FILE *Pi::ffmpegFile = nullptr;

Pi::MainState Pi::m_mainState = Pi::MainState::MAIN_MENU;

std::unique_ptr<AsyncJobQueue> Pi::asyncJobQueue;
std::unique_ptr<SyncJobQueue> Pi::syncJobQueue;

std::unique_ptr<DebugInfo> s_debugInfo;

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

static void LuaInit()
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

static void LuaUninit()
{
	Pi::m_luaNameGen.reset();

	Lua::Uninit();
}

static void LuaInitGame()
{
	LuaEvent::Clear();
}

void TestGPUJobsSupport()
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

static void draw_progress(float progress)
{
	RendererLocator::getRenderer()->ClearScreen();
	PiGui::NewFrame(RendererLocator::getRenderer()->GetSDLWindow());
	Pi::DrawPiGui(progress, "INIT");
	RendererLocator::getRenderer()->SwapBuffers();
}

void Pi::Init(const std::map<std::string, std::string> &options, bool no_gui)
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
	profilerPath = FileSystem::JoinPathBelow(FileSystem::userFiles.GetRoot(), "profiler");
#endif
	PROFILE_SCOPED()

	GameConfSingleton::Init(options);

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
	videoSettings.hidden = no_gui;
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
	Pi::input = std::make_unique<Input>();

	RegisterInputBindings();

	// we can only do bindings once joysticks are initialised.
	if (!no_gui) // This re-saves the config file. With no GUI we want to allow multiple instances in parallel.
		KeyBindings::InitBindings();

	TestGPUJobsSupport();

	EnumStrings::Init();

	// get threads up
	uint32_t numThreads = GameConfSingleton::getInstance().Int("WorkerThreads");
	const int numCores = OS::GetNumCores();
	assert(numCores > 0);
	if (numThreads == 0) numThreads = std::max(uint32_t(numCores) - 1, 1U);
	asyncJobQueue.reset(new AsyncJobQueue(numThreads));
	Output("started %d worker threads\n", numThreads);
	syncJobQueue.reset(new SyncJobQueue);

	Output("ShipType::Init()\n");
	// XXX early, Lua init needs it
	ShipType::Init();

	// XXX UI requires Lua  but Pi::ui must exist before we start loading
	// templates. so now we have crap everywhere :/
	Output("Lua::Init()\n");
	Lua::Init();

	Pi::pigui.Reset(new PiGui);
	Pi::pigui->Init(RendererLocator::getRenderer()->GetSDLWindow());

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
	Pi::serverAgent = 0;
	if (config->Int("EnableServerAgent")) {
		const std::string endpoint(config->String("ServerEndpoint"));
		if (endpoint.size() > 0) {
			Output("Server agent enabled, endpoint: %s\n", endpoint.c_str());
			Pi::serverAgent = new HTTPServerAgent(endpoint);
		}
	}
	if (!Pi::serverAgent) {
		Output("Server agent disabled\n");
		Pi::serverAgent = new NullServerAgent();
	}
#endif

	LuaInit();

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

	if (!no_gui && !GameConfSingleton::getInstance().Int("DisableSound")) {
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

#if 0
	// test code to produce list of ship stats

	FILE *pStatFile = fopen("shipstat.csv","wt");
	if (pStatFile)
	{
		fprintf(pStatFile, "name,modelname,hullmass,capacity,fakevol,rescale,xsize,ysize,zsize,facc,racc,uacc,sacc,aacc,exvel\n");
		for (auto iter : ShipType::types)
		{
			const ShipType *shipdef = &(iter.second);
			SceneGraph::Model *model = Pi::FindModel(shipdef->modelName, false);

			double hullmass = shipdef->hullMass;
			double capacity = shipdef->capacity;

			double xsize = 0.0, ysize = 0.0, zsize = 0.0, fakevol = 0.0, rescale = 0.0, brad = 0.0;
			if (model) {
				std::unique_ptr<SceneGraph::Model> inst(model->MakeInstance());
				model->CreateCollisionMesh();
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

	m_luaConsole.reset(new LuaConsole());

	draw_progress(1.0f);

	timer.Stop();
#ifdef PIONEER_PROFILER
	Profiler::dumphtml(profilerPath.c_str());
#endif
	Output("\n\nLoading took: %lf milliseconds\n", timer.millicycles());
}

bool Pi::IsConsoleActive()
{
	return m_luaConsole && m_luaConsole->IsActive();
}

void Pi::Quit()
{
	if (GameLocator::getGame()) { // always end the game if there is one before quitting
		Pi::EndGame();
	}
	if (Pi::ffmpegFile != nullptr) {
		_pclose(Pi::ffmpegFile);
	}
	m_inputFrame.reset();
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
	Pi::pigui->Uninit();
	Pi::ui.Reset(nullptr);
	Pi::pigui.Reset(nullptr);
	LuaUninit();
	Gui::Uninit();
	delete RendererLocator::getRenderer();
	GalaxyGenerator::Uninit();
	SDL_Quit();
	FileSystem::Uninit();
	asyncJobQueue.reset();
	syncJobQueue.reset();
	exit(0);
}

void Pi::OnChangeDetailLevel()
{
	BaseSphere::OnChangeDetailLevel(GameConfSingleton::getDetail().planets);
}

bool Pi::HandleEscKey()
{
	if (!InGameViewsLocator::getInGameViews()) return true;

	switch (InGameViewsLocator::getInGameViews()->GetViewType()) {
	case ViewType::OBJECT:
	case ViewType::SPACESTATION:
	case ViewType::INFO:
	case ViewType::SECTOR: InGameViewsLocator::getInGameViews()->SetView(ViewType::WORLD); break;
	case ViewType::GALACTIC:
	case ViewType::SYSTEMINFO:
	case ViewType::SYSTEM: InGameViewsLocator::getInGameViews()->SetView(ViewType::SECTOR); break;
	case ViewType::NONE:
	case ViewType::DEATH: break;
	case ViewType::WORLD: return true;
	};
	return false;
}

void Pi::HandleEvents()
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

	Pi::input->ResetFrameInput();
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
		if (ui->DispatchSDLEvent(event))
			continue;

		bool consoleActive = Pi::IsConsoleActive();
		if (consoleActive) {
			m_luaConsole->CheckEvent(event);
		} else {
		}

		//if (Pi::IsConsoleActive())
		//	continue;

		Gui::HandleSDLEvent(&event);
		input->HandleSDLEvent(event);

		if (consoleActive != Pi::IsConsoleActive()) {
			skipTextInput = true;
			continue;
		}
	}
}

void Pi::HandleRequests()
{
	for (auto request : internalRequests) {
		switch (request) {
		case END_GAME:
			EndGame();
			break;
		case QUIT_GAME:
			Quit();
			break;
		default:
			Output("Pi::HandleRequests, unhandled request type processed.\n");
			break;
		}
	}
	internalRequests.clear();
}

void Pi::RegisterInputBindings()
{
	using namespace KeyBindings;
	using namespace std::placeholders;

	m_inputFrame = std::make_unique<InputFrame>("ObjectViewer");

	auto &page = Pi::input->GetBindingPage("TweakAndSetting");
	page.shouldBeTranslated = false;

	auto &group = page.GetBindingGroup("None");

	// NOTE: All these bindings must use a modifier! Prefer CTRL over ALT or SHIFT
	m_piBindings.quickSave = m_inputFrame->AddActionBinding("QuickSave", group, ActionBinding(KeyBinding(SDLK_F9, KMOD_LCTRL)));
	m_piBindings.quickSave->StoreOnActionCallback(std::bind(&Pi::QuickSave, _1));

	m_piBindings.reqQuit = m_inputFrame->AddActionBinding("RequestQuit", group, ActionBinding(KeyBinding(SDLK_q, KMOD_LCTRL)));
	m_piBindings.reqQuit->StoreOnActionCallback(std::bind(&Pi::RequestQuit));

	m_piBindings.screenShot = m_inputFrame->AddActionBinding("Screenshot", group, ActionBinding(KeyBinding(SDLK_a, KMOD_LCTRL)));
	m_piBindings.screenShot->StoreOnActionCallback(std::bind(&Pi::ScreenShot, _1));

	m_piBindings.toggleVideoRec = m_inputFrame->AddActionBinding("ToggleVideoRec", group, ActionBinding(KeyBinding(SDLK_ASTERISK, KMOD_LCTRL)));
	m_piBindings.toggleVideoRec->StoreOnActionCallback(std::bind(&Pi::ToggleVideoRecording, _1));

	#ifdef WITH_DEVKEYS
	m_piBindings.toggleDebugInfo = m_inputFrame->AddActionBinding("ToggleDebugInfo", group, ActionBinding(KeyBinding(SDLK_i, KMOD_LCTRL)));
	m_piBindings.toggleDebugInfo->StoreOnActionCallback(std::bind(&Pi::ToggleDebug, _1));

	m_piBindings.reloadShaders = m_inputFrame->AddActionBinding("ReloadShaders", group, ActionBinding(KeyBinding(SDLK_F11, KMOD_LCTRL)));
	m_piBindings.reloadShaders->StoreOnActionCallback(&Pi::ReloadShaders);
	#endif // WITH_DEVKEYS

	#ifdef PIONEER_PROFILER
	m_piBindings.profilerBindOne = m_inputFrame->AddActionBinding("ProfilerOne", group, ActionBinding(KeyBinding(SDLK_p, KMOD_LCTRL)));
	m_piBindings.profilerBindOne->StoreOnActionCallback(&Pi::ProfilerCommandOne);
	m_piBindings.profilerBindSlow = m_inputFrame->AddActionBinding("ProfilerSlow", group, ActionBinding(KeyBinding(SDLK_p, SDL_Keymod(KMOD_LCTRL | KMOD_LSHIFT))));
	m_piBindings.profilerBindSlow->StoreOnActionCallback(&Pi::ProfilerCommandSlow);
	#endif // PIONEER_PROFILER

	#ifdef WITH_OBJECTVIEWER
	m_piBindings.objectViewer = m_inputFrame->AddActionBinding("ObjectViewer", group, ActionBinding(KeyBinding(SDLK_F10, KMOD_LCTRL)));
	m_piBindings.objectViewer->StoreOnActionCallback(&Pi::ObjectViewer);
	#endif // WITH_OBJECTVIEWER

	m_inputFrame->SetActive(true);
}

void Pi::QuickSave(bool down)
{
	if (down) return;
	if (GameLocator::getGame()) {
		if (GameLocator::getGame()->IsHyperspace()) {
			GameLocator::getGame()->GetGameLog().Add(Lang::CANT_SAVE_IN_HYPERSPACE);
		} else {
			const std::string name = "_quicksave";
			const std::string path = FileSystem::JoinPath(GameConfSingleton::GetSaveDirFull(), name);
			try {
				GameState::SaveGame(name);
				Output("Quick save: %s\n", name.c_str());
				GameLocator::getGame()->GetGameLog().Add(Lang::GAME_SAVED_TO + path);
			} catch (CouldNotOpenFileException) {
				GameLocator::getGame()->GetGameLog().Add(stringf(Lang::COULD_NOT_OPEN_FILENAME, formatarg("path", path)));
			} catch (CouldNotWriteToFileException) {
				GameLocator::getGame()->GetGameLog().Add(Lang::GAME_SAVE_CANNOT_WRITE);
			}
		}
	}
}

void Pi::ScreenShot(bool down)
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

void Pi::ToggleVideoRecording(bool down)
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

#if WITH_DEVKEYS
void Pi::ToggleDebug(bool down)
{
	if (down) return;
	Pi::showDebugInfo = !Pi::showDebugInfo;
	if (Pi::showDebugInfo) {
		s_debugInfo = std::make_unique<DebugInfo>();
	} else {
		s_debugInfo.reset();
	}
}

void Pi::ReloadShaders(bool down)
{
	if (down) return;
	RendererLocator::getRenderer()->ReloadShaders();
}
#endif // WITH_DEVKEYS

#ifdef PIONEER_PROFILER
void Pi::ProfilerCommandOne(bool down)
{
	if (down) return;

	Pi::doProfileOne = true;
}

void Pi::ProfilerCommandSlow(bool down)
{
	if (down) return;

	Pi::doProfileSlow = !Pi::doProfileSlow;
	Output("slow frame profiling %s\n", Pi::doProfileSlow ? "enabled" : "disabled");
}
#endif // PIONEER_PROFILER

#if WITH_OBJECTVIEWER
void Pi::ObjectViewer(bool down)
{
	if (down) return;
	InGameViewsLocator::getInGameViews()->SetView(ViewType::OBJECT);
}
#endif // WITH_OBJECTVIEWER

void Pi::CutSceneLoop(double step, Cutscene *m_cutscene)
{
	// XXX hack
	// if we hit our exit conditions then ignore further queued events
	// protects against eg double-click during game generation
	if (GameLocator::getGame()) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
		}
	}

	Pi::BeginRenderTarget();
	RendererLocator::getRenderer()->BeginFrame();
	m_cutscene->Draw(step);
	RendererLocator::getRenderer()->EndFrame();

	RendererLocator::getRenderer()->ClearDepthBuffer();

	// Mainly for Console
	Pi::ui->Update();
	Pi::ui->Draw();

	Pi::HandleEvents();

	Gui::Draw();

	if (dynamic_cast<Intro *>(m_cutscene) != nullptr) {
		PiGui::NewFrame(RendererLocator::getRenderer()->GetSDLWindow());
		DrawPiGui(step, "MAINMENU");
	}

	Pi::EndRenderTarget();

	// render the rendertarget texture
	Pi::DrawRenderTarget();
	RendererLocator::getRenderer()->SwapBuffers();

	Pi::HandleRequests();

#ifdef ENABLE_SERVER_AGENT
	Pi::serverAgent->ProcessResponses();
#endif
}

void Pi::InitGame()
{
	// this is a bit brittle. skank may be forgotten and survive between
	// games

	input->InitGame();

	if (!GameConfSingleton::getInstance().Int("DisableSound")) AmbientSounds::Init();

	LuaInitGame();
}

void Pi::TerminateGame()
{
	input->TerminateGame();
}

static void OnPlayerDockOrUndock()
{
	GameLocator::getGame()->RequestTimeAccel(Game::TIMEACCEL_1X);
	GameLocator::getGame()->SetTimeAccel(Game::TIMEACCEL_1X);
}

void Pi::StartGame()
{
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
	LuaEvent::Queue("onGameStart");
	LuaEvent::Emit();
}

void Pi::Start(const SystemPath &startPath)
{
	if (startPath != SystemPath(0, 0, 0, 0, 0)) {
		GameState::MakeNewGame(startPath);
		m_mainState = MainState::TO_GAME_START;
	} else {
		m_mainState = MainState::TO_MAIN_MENU;
	}

	//XXX global ambient colour hack to make explicit the old default ambient colour dependency
	// for some models
	RendererLocator::getRenderer()->SetAmbientColor(Color(51, 51, 51, 255));

	float time = 0.0;
	uint32_t last_time = SDL_GetTicks();

	while (1) {
		m_frameTime = 0.001f * (SDL_GetTicks() - last_time);
		last_time = SDL_GetTicks();

		switch (m_mainState) {
		case MainState::MAIN_MENU:
			CutSceneLoop(m_frameTime, m_cutscene.get());
			if (GameLocator::getGame() != nullptr) {
				m_mainState = MainState::TO_GAME_START;
			}
		break;
		case MainState::GAME_START:
			MainLoop();
			// no m_mainState set as it can be either TO_TOMBSTONE or TO_GAME_START
		break;
		case MainState::TO_GAME_START:
			m_cutscene.reset();
			InitGame();
			StartGame();
			m_mainState = MainState::GAME_START;
		break;
		case MainState::TO_MAIN_MENU:
			m_cutscene.reset(new Intro(Graphics::GetScreenWidth(),
				Graphics::GetScreenHeight(),
				GameConfSingleton::GetAmountBackgroundStars()
				));
			TerminateGame();
			m_mainState = MainState::MAIN_MENU;
		break;
		case MainState::TO_TOMBSTONE:
			EndGame();
			m_cutscene.reset(new Tombstone(Graphics::GetScreenWidth(),
				Graphics::GetScreenHeight()
				));
			time = 0.0;
			m_mainState = MainState::TOMBSTONE;
		break;
		case MainState::TOMBSTONE:
			time += m_frameTime;
			CutSceneLoop(m_frameTime, m_cutscene.get());
			if ((time > 2.0) && (input->IsAnyKeyJustPressed())) {
				m_cutscene.reset();
				m_mainState = MainState::TO_MAIN_MENU;
			}
		break;
		}
	}
}

// request that the game is ended as soon as safely possible
void Pi::RequestEndGame()
{
	internalRequests.push_back(END_GAME);
}

void Pi::RequestQuit()
{
	internalRequests.push_back(QUIT_GAME);
}

void Pi::EndGame()
{
	Pi::SetMouseGrab(false);

	Sound::MusicPlayer::Stop();
	Sound::DestroyAllEvents();

	// final event
	LuaEvent::Queue("onGameEnd");
	LuaEvent::Emit();

	if (!GameConfSingleton::getInstance().Int("DisableSound")) AmbientSounds::Uninit();
	Sound::DestroyAllEvents();

	assert(GameLocator::getGame());

	GameState::DestroyGame();

	Lua::manager->CollectGarbage();
}

void Pi::MainLoop()
{
	double time_player_died = 0;
#if WITH_DEVKEYS
	if (s_debugInfo)  s_debugInfo->NewCycle();
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
			int pstate = GameLocator::getGame()->GetPlayer()->GetFlightState();
			if (pstate == Ship::DOCKED || pstate == Ship::DOCKING || pstate == Ship::UNDOCKING)
				m_gameTickAlpha = 1.0;
			else
				m_gameTickAlpha = accumulator / step;

#if WITH_DEVKEYS
			if (s_debugInfo)  s_debugInfo->IncreasePhys(phys_ticks);
#endif
		} else {
			// paused
			PROFILE_SCOPED_RAW("paused")
			BaseSphere::UpdateAllBaseSphereDerivatives();
		}
#if WITH_DEVKEYS
		if (s_debugInfo)  s_debugInfo->IncreaseFrame();
#endif

		// did the player die?
		if (GameLocator::getGame()->GetPlayer()->IsDead()) {
			if (time_player_died > 0.0) {
				if (GameLocator::getGame()->GetTime() - time_player_died > 8.0) {
					InGameViewsLocator::getInGameViews()->SetView(ViewType::NONE);
					m_mainState = MainState::TO_TOMBSTONE;
					return;
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
			b->UpdateInterpTransform(Pi::GetGameTickAlpha());
		}

		Frame::GetRootFrame()->UpdateInterpTransform(Pi::GetGameTickAlpha());

		InGameViewsLocator::getInGameViews()->UpdateView(m_frameTime);
		InGameViewsLocator::getInGameViews()->Draw3DView();

		// hide cursor for ship control. Do this before imgui runs, to prevent the mouse pointer from jumping
		Pi::SetMouseGrab(input->MouseButtonState(SDL_BUTTON_RIGHT) | input->MouseButtonState(SDL_BUTTON_MIDDLE));

		// XXX HandleEvents at the moment must be after view->Draw3D and before
		// Gui::Draw so that labels drawn to screen can have mouse events correctly
		// detected. Gui::Draw wipes memory of label positions.
		Pi::HandleEvents();

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
		if ((!GameLocator::getGame() || !InGameViewsLocator::getInGameViews()->IsDeathView()) && InGameViewsLocator::getInGameViews()->ShouldDrawGui()) {
			Pi::ui->Update();
			Pi::ui->Draw();
		}

		Pi::EndRenderTarget();
		Pi::DrawRenderTarget();
		if (GameLocator::getGame() && !GameLocator::getGame()->GetPlayer()->IsDead()) {
			// FIXME: Always begin a camera frame because WorldSpaceToScreenSpace
			// requires it and is exposed to pigui.
			InGameViewsLocator::getInGameViews()->GetWorldView()->BeginCameraFrame();
			PiGui::NewFrame(RendererLocator::getRenderer()->GetSDLWindow(), InGameViewsLocator::getInGameViews()->ShouldDrawGui());

			InGameViewsLocator::getInGameViews()->DrawUI(m_frameTime);

#if WITH_DEVKEYS
			if (Pi::showDebugInfo)  {
				s_debugInfo->Update();
				s_debugInfo->Print();
			}
#endif
			DrawPiGui(m_frameTime, "GAME");

			InGameViewsLocator::getInGameViews()->GetWorldView()->EndCameraFrame();
		}

		RendererLocator::getRenderer()->SwapBuffers();

		// game exit will have cleared GameLocator::getGame(). we can't continue.
		if (!GameLocator::getGame()) {
			// XXX: Not checked, but sure there's needs to change state..
			m_mainState = MainState::TO_MAIN_MENU;
			return;
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

		syncJobQueue->RunJobs(SYNC_JOBS_PER_LOOP);
		asyncJobQueue->FinishJobs();
		syncJobQueue->FinishJobs();

		HandleRequests();

#ifdef PIONEER_PROFILER
		const uint32_t profTicks = SDL_GetTicks();
		if (Pi::doProfileOne || (Pi::doProfileSlow && (profTicks - newTicks) > 100)) { // slow: < ~10fps
			Output("dumping profile data\n");
			Profiler::dumphtml(profilerPath.c_str());
			Pi::doProfileOne = false;
		}
#endif // PIONEER_PROFILER

		if (isRecordingVideo && (Pi::ffmpegFile != nullptr)) {
			Graphics::ScreendumpState sd;
			RendererLocator::getRenderer()->Screendump(sd);
			fwrite(sd.pixels.get(), sizeof(uint32_t) * RendererLocator::getRenderer()->GetWindowWidth() * RendererLocator::getRenderer()->GetWindowHeight(), 1, Pi::ffmpegFile);
		}

#ifdef PIONEER_PROFILER
		Profiler::reset();
#endif
	}
	m_mainState = MainState::TO_MAIN_MENU;
}

void Pi::SetMouseGrab(bool on)
{
	if (!doingMouseGrab && on) {
		RendererLocator::getRenderer()->SetGrab(true);
		Pi::ui->SetMousePointerEnabled(false);
		doingMouseGrab = true;
	} else if (doingMouseGrab && !on) {
		RendererLocator::getRenderer()->SetGrab(false);
		Pi::ui->SetMousePointerEnabled(true);
		doingMouseGrab = false;
	}
}

void Pi::DrawPiGui(double delta, std::string handler)
{
	PROFILE_SCOPED()

	if (!IsConsoleActive())
		Pi::pigui->Render(delta, handler);

	PiGui::RenderImGui();
}
