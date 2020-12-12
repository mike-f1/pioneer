// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Pi.h"

#include "sphere/BaseSphere.h"
#include "CityOnPlanet.h"
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
#include "input/InputLocator.h"
#include "input/Input.h"
#include "JobQueue.h"
#include "Lang.h"
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
#include "Missile.h"
#include "ModManager.h"
#include "ModelCache.h"
#include "NavLights.h"
#include "OS.h"
#include "pi_state/PiState.h"
#include "pi_state/GameState.h"
#include "pi_state/MainMenuState.h"
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
#include "ShipType.h"
#include "SpaceStation.h"
#include "Star.h"
#include "libs/StringF.h"
#include "Random.h"
#include "RandomSingleton.h"
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

std::unique_ptr<LuaNameGen> Pi::m_luaNameGen = {};
#ifdef ENABLE_SERVER_AGENT
ServerAgent *Pi::serverAgent;
#endif
std::unique_ptr<LuaConsole> Pi::m_luaConsole;
#if PIONEER_PROFILER
std::string Pi::profilerPath;
bool Pi::doProfileSlow = false;
bool Pi::doProfileOne = false;
#endif
RefCountedPtr<UI::Context> Pi::ui;
RefCountedPtr<PiGui> Pi::pigui;
Graphics::RenderTarget *Pi::m_renderTarget;
RefCountedPtr<Graphics::Texture> Pi::m_renderTexture;
std::unique_ptr<Graphics::Drawables::TexturedQuad> Pi::m_renderQuad;
Graphics::RenderState *Pi::m_quadRenderState = nullptr;
std::vector<Pi::InternalRequests> Pi::internalRequests;
bool Pi::isRecordingVideo = false;
FILE *Pi::ffmpegFile = nullptr;

std::unique_ptr<AsyncJobQueue> Pi::asyncJobQueue;
std::unique_ptr<SyncJobQueue> Pi::syncJobQueue;

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

static void LuaUninit()
{
	Pi::m_luaNameGen.reset();

	Lua::Uninit();
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
	InputLocator::provideInput(new Input());

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

void Pi::Quit()
{
	if (Pi::ffmpegFile != nullptr) {
		_pclose(Pi::ffmpegFile);
	}
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
	// TODO: remove explicit Reset of LuaInputFrames
	LuaInputFrames::Reset();
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

void Pi::RegisterInputBindings()
{
	using namespace KeyBindings;
	using namespace std::placeholders;

	// TODO: left here to place static initialization of inputFrames...
}

void Pi::HandleRequests(MainState &current)
{
	for (auto request : internalRequests) {
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
	internalRequests.clear();
}

void Pi::Start(const SystemPath &startPath)
{
	MainState_::PiState *piState = nullptr;
	if (startPath != SystemPath(0, 0, 0, 0, 0)) {
		GameStateStatic::MakeNewGame(startPath);
		piState =  new MainState_::GameState();
	} else {
		piState = new MainState_::MainMenuState();
	}

	//XXX global ambient colour hack to make explicit the old default ambient colour dependency
	// for some models
	RendererLocator::getRenderer()->SetAmbientColor(Color(51, 51, 51, 255));

	while (piState) {
		piState = piState->Update();
	}
	Quit();
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

void Pi::DrawPiGui(double delta, std::string handler)
{
	PROFILE_SCOPED()
	bool active = true;
	if (m_luaConsole) active = !m_luaConsole->IsActive();

	if (active)
		Pi::pigui->Render(delta, handler);

	PiGui::RenderImGui();
}
