// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "buildopts.h"

#include "Pi.h"

#include "BaseSphere.h"
#include "CityOnPlanet.h"
#include "DeathView.h"
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
#include "sound/AmbientSounds.h"
#if WITH_OBJECTVIEWER
#include "ObjectViewerView.h"
#endif
#include "Beam.h"
#include "PiGui.h"
#include "Player.h"
#include "Projectile.h"
#include "Sfx.h"
#include "Shields.h"
#include "ShipCpanel.h"
#include "ShipType.h"
#include "Space.h"
#include "SpaceStation.h"
#include "Star.h"
#include "StringF.h"
#include "SystemView.h"
#include "Random.h"
#include "RandomSingleton.h"
#include "Tombstone.h"
#include "UIView.h"
#include "WorldView.h"
#include "galaxy/GalaxyGenerator.h"
#include "gameui/Lua.h"
#include "libs.h"
#include "pigui/PiGuiLua.h"
#include "ship/PlayerShipController.h"
#include "ship/ShipViewController.h"
#include "sound/Sound.h"
#include "sound/SoundMusic.h"

#include "graphics/Renderer.h"

#if WITH_DEVKEYS
#include "graphics/Graphics.h"
#include "graphics/Light.h"
#include "graphics/Stats.h"
#endif // WITH_DEVKEYS

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

float Pi::gameTickAlpha;
LuaNameGen *Pi::luaNameGen;
#ifdef ENABLE_SERVER_AGENT
ServerAgent *Pi::serverAgent;
#endif
Input Pi::input;
TransferPlanner *Pi::planner;
LuaConsole *Pi::luaConsole;
float Pi::frameTime;
bool Pi::doingMouseGrab;
#if WITH_DEVKEYS
bool Pi::showDebugInfo = false;
#endif
#if PIONEER_PROFILER
std::string Pi::profilerPath;
bool Pi::doProfileSlow = false;
bool Pi::doProfileOne = false;
#endif
int Pi::statSceneTris = 0;
int Pi::statNumPatches = 0;
Graphics::Renderer *Pi::renderer;
RefCountedPtr<UI::Context> Pi::ui;
RefCountedPtr<PiGui> Pi::pigui;
std::unique_ptr<Cutscene> Pi::cutscene;
Graphics::RenderTarget *Pi::renderTarget;
RefCountedPtr<Graphics::Texture> Pi::renderTexture;
std::unique_ptr<Graphics::Drawables::TexturedQuad> Pi::renderQuad;
Graphics::RenderState *Pi::quadRenderState = nullptr;
std::vector<Pi::InternalRequests> Pi::internalRequests;
bool Pi::isRecordingVideo = false;
FILE *Pi::ffmpegFile = nullptr;

Pi::MainState Pi::m_mainState = Pi::MainState::MAIN_MENU;

std::unique_ptr<AsyncJobQueue> Pi::asyncJobQueue;
std::unique_ptr<SyncJobQueue> Pi::syncJobQueue;

// Leaving define in place in case of future rendering problems.
#define USE_RTT 0

// static
JobQueue *Pi::GetAsyncJobQueue() { return asyncJobQueue.get(); }
// static
JobQueue *Pi::GetSyncJobQueue() { return syncJobQueue.get(); }


//static
void Pi::CreateRenderTarget(const Uint16 width, const Uint16 height)
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
	quadRenderState = Pi::renderer->CreateRenderState(rsd);

	Graphics::TextureDescriptor texDesc(
		Graphics::TEXTURE_RGBA_8888,
		vector2f(width, height),
		Graphics::LINEAR_CLAMP, false, false, 0);
	Pi::renderTexture.Reset(Pi::renderer->CreateTexture(texDesc));
	Pi::renderQuad.reset(new Graphics::Drawables::TexturedQuad(
		Pi::renderer, Pi::renderTexture.Get(),
		vector2f(0.0f, 0.0f), vector2f(float(Graphics::GetScreenWidth()), float(Graphics::GetScreenHeight())),
		quadRenderState));

	// Complete the RT description so we can request a buffer.
	// NB: we don't want it to create use a texture because we share it with the textured quad created above.
	Graphics::RenderTargetDesc rtDesc(
		width,
		height,
		Graphics::TEXTURE_NONE, // don't create a texture
		Graphics::TEXTURE_DEPTH,
		false);
	Pi::renderTarget = Pi::renderer->CreateRenderTarget(rtDesc);

	Pi::renderTarget->SetColorTexture(Pi::renderTexture.Get());
#endif
}

//static
void Pi::DrawRenderTarget()
{
#if USE_RTT
	Pi::renderer->BeginFrame();
	Pi::renderer->SetViewport(0, 0, Graphics::GetScreenWidth(), Graphics::GetScreenHeight());
	Pi::renderer->SetTransform(matrix4x4f::Identity());

	{
		Pi::renderer->SetMatrixMode(Graphics::MatrixMode::PROJECTION);
		Pi::renderer->PushMatrix();
		Pi::renderer->SetOrthographicProjection(0, Graphics::GetScreenWidth(), Graphics::GetScreenHeight(), 0, -1, 1);
		Pi::renderer->SetMatrixMode(Graphics::MatrixMode::MODELVIEW);
		Pi::renderer->PushMatrix();
		Pi::renderer->LoadIdentity();
	}

	Pi::renderQuad->Draw(Pi::renderer);

	{
		Pi::renderer->SetMatrixMode(Graphics::MatrixMode::PROJECTION);
		Pi::renderer->PopMatrix();
		Pi::renderer->SetMatrixMode(Graphics::MatrixMode::MODELVIEW);
		Pi::renderer->PopMatrix();
	}

	Pi::renderer->EndFrame();
#endif
}

//static
void Pi::BeginRenderTarget()
{
#if USE_RTT
	Pi::renderer->SetRenderTarget(Pi::renderTarget);
	Pi::renderer->ClearScreen();
#endif
}

//static
void Pi::EndRenderTarget()
{
#if USE_RTT
	Pi::renderer->SetRenderTarget(nullptr);
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

	Pi::luaNameGen = new LuaNameGen(Lua::manager);
}

static void LuaUninit()
{
	delete Pi::luaNameGen;

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
		Uint32 octaves = 8;
		for (Uint32 i = 0; i < 6; i++) {
			std::unique_ptr<Graphics::Material> material;
			Graphics::MaterialDescriptor desc;
			desc.effect = Graphics::EFFECT_GEN_GASGIANT_TEXTURE;
			desc.quality = (octaves << 16) | i;
			desc.textures = 3;
			material.reset(Pi::renderer->CreateMaterial(desc));
			supportsGPUJobs &= material->IsProgramLoaded();
		}
		if (!supportsGPUJobs) {
			// failed - retry

			// reset the GPU jobs flag
			supportsGPUJobs = true;

			// retry the shader compilation
			octaves = 5; // reduce the number of octaves
			for (Uint32 i = 0; i < 6; i++) {
				std::unique_ptr<Graphics::Material> material;
				Graphics::MaterialDescriptor desc;
				desc.effect = Graphics::EFFECT_GEN_GASGIANT_TEXTURE;
				desc.quality = (octaves << 16) | i;
				desc.textures = 3;
				material.reset(Pi::renderer->CreateMaterial(desc));
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

// TODO: make this a part of the class and/or improve the mechanism
void RegisterInputBindings()
{
	PlayerShipController::RegisterInputBindings();

	ShipViewController::InputBindings.RegisterBindings();

	WorldView::RegisterInputBindings();
}

static void draw_progress(float progress)
{
	Pi::renderer->ClearScreen();
	PiGui::NewFrame(Pi::renderer->GetSDLWindow());
	Pi::DrawPiGui(progress, "INIT");
	Pi::renderer->SwapBuffers();
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
	Uint32 sdlInitFlags = SDL_INIT_VIDEO | SDL_INIT_JOYSTICK;
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
	//if(rendererName == Graphics::RendererNameFromType(Graphics::RENDERER_OPENGL_3x))
	//{
	//	rType = Graphics::RENDERER_OPENGL_3x;
	//}

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

	Pi::renderer = Graphics::Init(videoSettings);

	Pi::CreateRenderTarget(videoSettings.width, videoSettings.height);
	RandomSingleton::Init(time(0));

	input.Init();
	input.onKeyPress.connect(sigc::ptr_fun(&Pi::HandleKeyDown));

	// we can only do bindings once joysticks are initialised.
	if (!no_gui) // This re-saves the config file. With no GUI we want to allow multiple instances in parallel.
		KeyBindings::InitBindings();

	RegisterInputBindings();

	TestGPUJobsSupport();

	EnumStrings::Init();

	// get threads up
	Uint32 numThreads = GameConfSingleton::getInstance().Int("WorkerThreads");
	const int numCores = OS::GetNumCores();
	assert(numCores > 0);
	if (numThreads == 0) numThreads = std::max(Uint32(numCores) - 1, 1U);
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
	Pi::pigui->Init(Pi::renderer->GetSDLWindow());

	float ui_scale = GameConfSingleton::getInstance().Float("UIScaleFactor", 1.0f);
	if (Graphics::GetScreenHeight() < 768) {
		ui_scale = float(Graphics::GetScreenHeight()) / 768.0f;
	}

	Pi::ui.Reset(new UI::Context(
		Lua::manager,
		Pi::renderer,
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

	Gui::Init(renderer, Graphics::GetScreenWidth(), Graphics::GetScreenHeight(), 800, 600);

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

	Output("ModelCache::Init()\n");
	ModelCache::Init(Pi::renderer);
	draw_progress(0.3f);

	Output("Shields::Init()\n");
	Shields::Init(Pi::renderer);
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
	NavLights::Init(Pi::renderer);
	draw_progress(0.75f);

	Output("Sfx::Init()\n");
	SfxManager::Init(Pi::renderer);
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

	luaConsole = new LuaConsole();
	KeyBindings::toggleLuaConsole.onPress.connect(sigc::mem_fun(Pi::luaConsole, &LuaConsole::Toggle));

	planner = new TransferPlanner();

	draw_progress(1.0f);

	timer.Stop();
#ifdef PIONEER_PROFILER
	Profiler::dumphtml(profilerPath.c_str());
#endif
	Output("\n\nLoading took: %lf milliseconds\n", timer.millicycles());
}

bool Pi::IsConsoleActive()
{
	return luaConsole && luaConsole->IsActive();
}

void Pi::Quit()
{
	if (GameLocator::getGame()) { // always end the game if there is one before quitting
		Pi::EndGame();
	}
	if (Pi::ffmpegFile != nullptr) {
		_pclose(Pi::ffmpegFile);
	}
	Projectile::FreeModel();
	Beam::FreeModel();
	delete Pi::luaConsole;
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
	delete Pi::renderer;
	GalaxyGenerator::Uninit();
	delete Pi::planner;
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

void Pi::HandleKeyDown(SDL_Keysym *key)
{
	if (key->sym == SDLK_ESCAPE) {
		if (GameLocator::getGame()) {
			// only accessible once game started
			HandleEscKey();
		}
		return;
	}
	const bool CTRL = input.KeyState(SDLK_LCTRL) || input.KeyState(SDLK_RCTRL);

	// special keys.
	if (CTRL) {
		switch (key->sym) {
		case SDLK_q: // Quit
			Pi::RequestQuit();
			break;
		case SDLK_PRINTSCREEN: // print
		case SDLK_KP_MULTIPLY: // screen
		{
			char buf[256];
			const time_t t = time(0);
			struct tm *_tm = localtime(&t);
			strftime(buf, sizeof(buf), "screenshot-%Y%m%d-%H%M%S.png", _tm);
			Graphics::ScreendumpState sd;
			Pi::renderer->Screendump(sd);
			write_screenshot(sd, buf);
			break;
		}

		case SDLK_SCROLLLOCK: // toggle video recording
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
			break;
#if WITH_DEVKEYS
		case SDLK_i: // Toggle Debug info
			Pi::showDebugInfo = !Pi::showDebugInfo;
			break;

#ifdef PIONEER_PROFILER
		case SDLK_p: // alert it that we want to profile
			if (input.KeyState(SDLK_LSHIFT) || input.KeyState(SDLK_RSHIFT))
				Pi::doProfileOne = true;
			else {
				Pi::doProfileSlow = !Pi::doProfileSlow;
				Output("slow frame profiling %s\n", Pi::doProfileSlow ? "enabled" : "disabled");
			}
			break;
#endif

		case SDLK_F12: {
			if (GameLocator::getGame()) {
				vector3d dir = -GameLocator::getGame()->GetPlayer()->GetOrient().VectorZ();
				/* add test object */
				if (input.KeyState(SDLK_RSHIFT)) {
					Missile *missile =
						new Missile(ShipType::MISSILE_GUIDED, GameLocator::getGame()->GetPlayer());
					missile->SetOrient(GameLocator::getGame()->GetPlayer()->GetOrient());
					missile->SetFrame(GameLocator::getGame()->GetPlayer()->GetFrame());
					missile->SetPosition(GameLocator::getGame()->GetPlayer()->GetPosition() + 50.0 * dir);
					missile->SetVelocity(GameLocator::getGame()->GetPlayer()->GetVelocity());
					GameLocator::getGame()->GetSpace()->AddBody(missile);
					missile->AIKamikaze(GameLocator::getGame()->GetPlayer()->GetCombatTarget());
				} else if (input.KeyState(SDLK_LSHIFT)) {
					SpaceStation *s = static_cast<SpaceStation *>(GameLocator::getGame()->GetPlayer()->GetNavTarget());
					if (s) {
						Ship *ship = new Ship(ShipType::POLICE);
						int port = s->GetFreeDockingPort(ship);
						if (port != -1) {
							Output("Putting ship into station\n");
							// Make police ship intent on killing the player
							ship->AIKill(GameLocator::getGame()->GetPlayer());
							ship->SetFrame(GameLocator::getGame()->GetPlayer()->GetFrame());
							ship->SetDockedWith(s, port);
							GameLocator::getGame()->GetSpace()->AddBody(ship);
						} else {
							delete ship;
							Output("No docking ports free dude\n");
						}
					} else {
						Output("Select a space station...\n");
					}
				} else {
					Ship *ship = new Ship(ShipType::POLICE);
					if (!input.KeyState(SDLK_LALT)) { //Left ALT = no AI
						if (!input.KeyState(SDLK_LCTRL))
							ship->AIFlyTo(GameLocator::getGame()->GetPlayer()); // a less lethal option
						else
							ship->AIKill(GameLocator::getGame()->GetPlayer()); // a really lethal option!
					}
					lua_State *l = Lua::manager->GetLuaState();
					pi_lua_import(l, "Equipment");
					LuaTable equip(l, -1);
					LuaObject<Ship>::CallMethod<>(ship, "AddEquip", equip.Sub("laser").Sub("pulsecannon_dual_1mw"));
					LuaObject<Ship>::CallMethod<>(ship, "AddEquip", equip.Sub("misc").Sub("laser_cooling_booster"));
					LuaObject<Ship>::CallMethod<>(ship, "AddEquip", equip.Sub("misc").Sub("atmospheric_shielding"));
					lua_pop(l, 5);
					ship->SetFrame(GameLocator::getGame()->GetPlayer()->GetFrame());
					ship->SetPosition(GameLocator::getGame()->GetPlayer()->GetPosition() + 100.0 * dir);
					ship->SetVelocity(GameLocator::getGame()->GetPlayer()->GetVelocity());
					ship->UpdateEquipStats();
					GameLocator::getGame()->GetSpace()->AddBody(ship);
				}
			}
			break;
		}
#endif /* DEVKEYS */
#if WITH_OBJECTVIEWER
		case SDLK_F10:
			GameLocator::getGame()->GetInGameViews()->SetView(ViewType::OBJECT);
			break;
#endif
		case SDLK_F11:
			// XXX only works on X11
			//SDL_WM_ToggleFullScreen(Pi::scrSurface);
#if WITH_DEVKEYS
			renderer->ReloadShaders();
#endif
			break;
		case SDLK_F9: // Quicksave
		{
			if (GameLocator::getGame()) {
				if (GameLocator::getGame()->IsHyperspace()) {
					GameLocator::getGame()->log->Add(Lang::CANT_SAVE_IN_HYPERSPACE);
				} else {
					const std::string name = "_quicksave";
					const std::string path = FileSystem::JoinPath(GameConfSingleton::GetSaveDirFull(), name);
					try {
						GameState::SaveGame(name);
						Output("Quick save: %s\n", name.c_str());
						GameLocator::getGame()->log->Add(Lang::GAME_SAVED_TO + path);
					} catch (CouldNotOpenFileException) {
						GameLocator::getGame()->log->Add(stringf(Lang::COULD_NOT_OPEN_FILENAME, formatarg("path", path)));
					} catch (CouldNotWriteToFileException) {
						GameLocator::getGame()->log->Add(Lang::GAME_SAVE_CANNOT_WRITE);
					}
				}
			}
			break;
		}
		default:
			break; // This does nothing but it stops the compiler warnings
		}
	}
}

void Pi::HandleEscKey()
{
	if (!GameLocator::getGame()->GetInGameViews()->IsEmptyView()) {
		if (GameLocator::getGame()->GetInGameViews()->IsSectorView()) {
			GameLocator::getGame()->GetInGameViews()->SetView(ViewType::WORLD);
		} else if ((GameLocator::getGame()->GetInGameViews()->IsSystemView()) || (GameLocator::getGame()->GetInGameViews()->IsSystemInfoView())) {
			GameLocator::getGame()->GetInGameViews()->SetView(ViewType::SECTOR);
		} else {
			UIView *view = dynamic_cast<UIView *>(GameLocator::getGame()->GetInGameViews()->GetView());
			if (view) {
				// checks the template name
				const char *tname = view->GetTemplateName();
				if (tname) {
					if (!strcmp(tname, "GalacticView")) {
						GameLocator::getGame()->GetInGameViews()->SetView(ViewType::SECTOR);
					} else if (!strcmp(tname, "InfoView") || !strcmp(tname, "StationView")) {
						GameLocator::getGame()->GetInGameViews()->SetView(ViewType::WORLD);
					}
				}
			}
		}
	}
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

	Pi::input.mouseMotion[0] = Pi::input.mouseMotion[1] = 0;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT) {
			Pi::RequestQuit();
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
		if (!consoleActive) {
			KeyBindings::DispatchSDLEvent(&event);
			if (GameLocator::getGame()) GameLocator::getGame()->GetInGameViews()->HandleSDLEvent(event);
		} else {
			KeyBindings::toggleLuaConsole.CheckSDLEventAndDispatch(&event);
		}
		if (consoleActive != Pi::IsConsoleActive()) {
			skipTextInput = true;
			continue;
		}

		if (Pi::IsConsoleActive())
			continue;

		Gui::HandleSDLEvent(&event);
		input.HandleSDLEvent(event);
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

void Pi::CutSceneLoop(double step, Cutscene *cutscene)
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
	Pi::renderer->BeginFrame();
	cutscene->Draw(step);
	Pi::renderer->EndFrame();

	Pi::renderer->ClearDepthBuffer();

	// Mainly for Console
	Pi::ui->Update();
	Pi::ui->Draw();

	Pi::HandleEvents();

	Gui::Draw();

	if (dynamic_cast<Intro *>(cutscene) != nullptr) {
		PiGui::NewFrame(Pi::renderer->GetSDLWindow());
		DrawPiGui(step, "MAINMENU");
	}

	Pi::EndRenderTarget();

	// render the rendertarget texture
	Pi::DrawRenderTarget();
	Pi::renderer->SwapBuffers();

	Pi::HandleRequests();

#ifdef ENABLE_SERVER_AGENT
	Pi::serverAgent->ProcessResponses();
#endif
}

void Pi::InitGame()
{
	// this is a bit brittle. skank may be forgotten and survive between
	// games

	input.InitGame();

	if (!GameConfSingleton::getInstance().Int("DisableSound")) AmbientSounds::Init();

	LuaInitGame();
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
	GameLocator::getGame()->GetInGameViews()->GetCpan()->ShowAll();
	GameLocator::getGame()->GetInGameViews()->SetView(ViewType::WORLD);

#ifdef REMOTE_LUA_REPL
#ifndef REMOTE_LUA_REPL_PORT
#define REMOTE_LUA_REPL_PORT 12345
#endif
	luaConsole->OpenTCPDebugConnection(REMOTE_LUA_REPL_PORT);
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
	Pi::renderer->SetAmbientColor(Color(51, 51, 51, 255));

	float time = 0.0;
	Uint32 last_time = SDL_GetTicks();

	while (1) {
		frameTime = 0.001f * (SDL_GetTicks() - last_time);
		last_time = SDL_GetTicks();

		switch (m_mainState) {
		case MainState::MAIN_MENU:
			CutSceneLoop(frameTime, cutscene.get());
			if (GameLocator::getGame() != nullptr) {
				m_mainState = MainState::TO_GAME_START;
			}
		break;
		case MainState::GAME_START:
			MainLoop();
			// no m_mainState set as it can be either TO_TOMBSTONE or TO_GAME_START
		break;
		case MainState::TO_GAME_START:
			cutscene.reset();
			InitGame();
			StartGame();
			m_mainState = MainState::GAME_START;
		break;
		case MainState::TO_MAIN_MENU:
			cutscene.reset(new Intro(Pi::renderer,
				Graphics::GetScreenWidth(),
				Graphics::GetScreenHeight(),
				GameConfSingleton::GetAmountBackgroundStars()
				));
			m_mainState = MainState::MAIN_MENU;
		break;
		case MainState::TO_TOMBSTONE:
			EndGame();
			cutscene.reset(new Tombstone(Pi::renderer,
				Graphics::GetScreenWidth(),
				Graphics::GetScreenHeight()
				));
			time = 0.0;
			m_mainState = MainState::TOMBSTONE;
		break;
		case MainState::TOMBSTONE:
			time += frameTime;
			CutSceneLoop(frameTime, cutscene.get());
			if ((time > 2.0) && ((input.MouseButtonState(SDL_BUTTON_LEFT)) || input.KeyState(SDLK_SPACE))) {
				cutscene.reset();
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
#ifdef MAKING_VIDEO
	Uint32 last_screendump = SDL_GetTicks();
	int dumpnum = 0;
#endif /* MAKING_VIDEO */

#if WITH_DEVKEYS
	Uint32 last_stats = SDL_GetTicks();
	int frame_stat = 0;
	int phys_stat = 0;
	char fps_readout[2048];
	memset(fps_readout, 0, sizeof(fps_readout));
#endif

	int MAX_PHYSICS_TICKS = GameConfSingleton::getInstance().Int("MaxPhysicsCyclesPerRender");
	if (MAX_PHYSICS_TICKS <= 0)
		MAX_PHYSICS_TICKS = 4;

	double currentTime = 0.001 * double(SDL_GetTicks());
	double accumulator = GameLocator::getGame()->GetTimeStep();
	Pi::gameTickAlpha = 0;

#ifdef PIONEER_PROFILER
	Profiler::reset();
#endif

	while (GameLocator::getGame()) {
		PROFILE_SCOPED()

#ifdef ENABLE_SERVER_AGENT
		Pi::serverAgent->ProcessResponses();
#endif

		const Uint32 newTicks = SDL_GetTicks();
		double newTime = 0.001 * double(newTicks);
		Pi::frameTime = newTime - currentTime;
		if (Pi::frameTime > 0.25) Pi::frameTime = 0.25;
		currentTime = newTime;
		accumulator += Pi::frameTime * GameLocator::getGame()->GetTimeAccelRate();

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
				BaseSphere::UpdateAllBaseSphereDerivatives();

				accumulator -= step;
			}
			// rendering interpolation between frames: don't use when docked
			int pstate = GameLocator::getGame()->GetPlayer()->GetFlightState();
			if (pstate == Ship::DOCKED || pstate == Ship::DOCKING || pstate == Ship::UNDOCKING)
				Pi::gameTickAlpha = 1.0;
			else
				Pi::gameTickAlpha = accumulator / step;

#if WITH_DEVKEYS
			phys_stat += phys_ticks;
#endif
		} else {
			// paused
			PROFILE_SCOPED_RAW("paused")
			BaseSphere::UpdateAllBaseSphereDerivatives();
		}
#if WITH_DEVKEYS
		frame_stat++;
#endif

		// did the player die?
		if (GameLocator::getGame()->GetPlayer()->IsDead()) {
			if (time_player_died > 0.0) {
				if (GameLocator::getGame()->GetTime() - time_player_died > 8.0) {
					GameLocator::getGame()->GetInGameViews()->SetView(ViewType::NONE);
					m_mainState = MainState::TO_TOMBSTONE;
					return;
				}
			} else {
				GameLocator::getGame()->SetTimeAccel(Game::TIMEACCEL_1X);
				GameLocator::getGame()->GetInGameViews()->GetDeathView()->Init();
				GameLocator::getGame()->GetInGameViews()->SetView(ViewType::DEATH);
				time_player_died = GameLocator::getGame()->GetTime();
			}
		}

		Pi::BeginRenderTarget();
		Pi::renderer->SetViewport(0, 0, Graphics::GetScreenWidth(), Graphics::GetScreenHeight());
		Pi::renderer->BeginFrame();
		Pi::renderer->SetTransform(matrix4x4f::Identity());

		/* Calculate position for this rendered frame (interpolated between two physics ticks */
		// XXX should this be here? what is this anyway?
		for (Body *b : GameLocator::getGame()->GetSpace()->GetBodies()) {
			b->UpdateInterpTransform(Pi::GetGameTickAlpha());
		}

		Frame::GetRootFrame()->UpdateInterpTransform(Pi::GetGameTickAlpha());

		GameLocator::getGame()->GetInGameViews()->UpdateView();
		GameLocator::getGame()->GetInGameViews()->Draw3DView();

		// hide cursor for ship control. Do this before imgui runs, to prevent the mouse pointer from jumping
		Pi::SetMouseGrab(input.MouseButtonState(SDL_BUTTON_RIGHT) | input.MouseButtonState(SDL_BUTTON_MIDDLE));

		// XXX HandleEvents at the moment must be after view->Draw3D and before
		// Gui::Draw so that labels drawn to screen can have mouse events correctly
		// detected. Gui::Draw wipes memory of label positions.
		Pi::HandleEvents();

#ifdef REMOTE_LUA_REPL
		Pi::luaConsole->HandleTCPDebugConnections();
#endif

		Pi::renderer->EndFrame();

		Pi::renderer->ClearDepthBuffer();
		if (GameLocator::getGame()->GetInGameViews()->DrawGui()) {
			Gui::Draw();
		}

		// XXX don't draw the UI during death obviously a hack, and still
		// wrong, because we shouldn't this when the HUD is disabled, but
		// probably sure draw it if they switch to eg infoview while the HUD is
		// disabled so we need much smarter control for all this rubbish
		if ((!GameLocator::getGame() || !GameLocator::getGame()->GetInGameViews()->IsDeathView()) && GameLocator::getGame()->GetInGameViews()->DrawGui()) {
			Pi::ui->Update();
			Pi::ui->Draw();
		}

		Pi::EndRenderTarget();
		Pi::DrawRenderTarget();
		if (GameLocator::getGame() && !GameLocator::getGame()->GetPlayer()->IsDead()) {
			// FIXME: Always begin a camera frame because WorldSpaceToScreenSpace
			// requires it and is exposed to pigui.
			GameLocator::getGame()->GetInGameViews()->GetWorldView()->BeginCameraFrame();
			PiGui::NewFrame(Pi::renderer->GetSDLWindow(), GameLocator::getGame()->GetInGameViews()->DrawGui());
			DrawPiGui(Pi::frameTime, "GAME");
			GameLocator::getGame()->GetInGameViews()->GetWorldView()->EndCameraFrame();
		}

#if WITH_DEVKEYS
		// NB: this needs to be rendered last so that it appears over all other game elements
		//	preferrably like this where it is just before the buffer swap
		if (Pi::showDebugInfo) {
			Gui::Screen::EnterOrtho();
			Gui::Screen::PushFont("ConsoleFont");
			static RefCountedPtr<Graphics::VertexBuffer> s_debugInfovb;
			Gui::Screen::RenderStringBuffer(s_debugInfovb, fps_readout, 0, 0);
			Gui::Screen::PopFont();
			Gui::Screen::LeaveOrtho();
		}
#endif

		Pi::renderer->SwapBuffers();

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
		GameLocator::getGame()->GetInGameViews()->GetCpan()->Update();
		Sound::MusicPlayer::Update();

		syncJobQueue->RunJobs(SYNC_JOBS_PER_LOOP);
		asyncJobQueue->FinishJobs();
		syncJobQueue->FinishJobs();

		HandleRequests();

#if WITH_DEVKEYS
		if (Pi::showDebugInfo && SDL_GetTicks() - last_stats > 1000) {
			size_t lua_mem = Lua::manager->GetMemoryUsage();
			int lua_memB = int(lua_mem & ((1u << 10) - 1));
			int lua_memKB = int(lua_mem >> 10) % 1024;
			int lua_memMB = int(lua_mem >> 20);
			const Graphics::Stats::TFrameData &stats = Pi::renderer->GetStats().FrameStatsPrevious();
			const Uint32 numDrawCalls = stats.m_stats[Graphics::Stats::STAT_DRAWCALL];
			const Uint32 numBuffersCreated = stats.m_stats[Graphics::Stats::STAT_CREATE_BUFFER];
			const Uint32 numDrawTris = stats.m_stats[Graphics::Stats::STAT_DRAWTRIS];
			const Uint32 numDrawPointSprites = stats.m_stats[Graphics::Stats::STAT_DRAWPOINTSPRITES];
			const Uint32 numDrawBuildings = stats.m_stats[Graphics::Stats::STAT_BUILDINGS];
			const Uint32 numDrawCities = stats.m_stats[Graphics::Stats::STAT_CITIES];
			const Uint32 numDrawGroundStations = stats.m_stats[Graphics::Stats::STAT_GROUNDSTATIONS];
			const Uint32 numDrawSpaceStations = stats.m_stats[Graphics::Stats::STAT_SPACESTATIONS];
			const Uint32 numDrawAtmospheres = stats.m_stats[Graphics::Stats::STAT_ATMOSPHERES];
			const Uint32 numDrawPatches = stats.m_stats[Graphics::Stats::STAT_PATCHES];
			const Uint32 numDrawPlanets = stats.m_stats[Graphics::Stats::STAT_PLANETS];
			const Uint32 numDrawGasGiants = stats.m_stats[Graphics::Stats::STAT_GASGIANTS];
			const Uint32 numDrawStars = stats.m_stats[Graphics::Stats::STAT_STARS];
			const Uint32 numDrawShips = stats.m_stats[Graphics::Stats::STAT_SHIPS];
			const Uint32 numDrawBillBoards = stats.m_stats[Graphics::Stats::STAT_BILLBOARD];
			snprintf(
				fps_readout, sizeof(fps_readout),
				"%d fps (%.1f ms/f), %d phys updates, %d triangles, %.3f M tris/sec, %d glyphs/sec, %d patches/frame\n"
				"Lua mem usage: %d MB + %d KB + %d bytes (stack top: %d)\n\n"
				"Draw Calls (%u), of which were:\n Tris (%u)\n Point Sprites (%u)\n Billboards (%u)\n"
				"Buildings (%u), Cities (%u), GroundStations (%u), SpaceStations (%u), Atmospheres (%u)\n"
				"Patches (%u), Planets (%u), GasGiants (%u), Stars (%u), Ships (%u)\n"
				"Buffers Created(%u)\n",
				frame_stat, (1000.0 / frame_stat), phys_stat, Pi::statSceneTris, Pi::statSceneTris * frame_stat * 1e-6,
				Text::TextureFont::GetGlyphCount(), Pi::statNumPatches,
				lua_memMB, lua_memKB, lua_memB, lua_gettop(Lua::manager->GetLuaState()),
				numDrawCalls, numDrawTris, numDrawPointSprites, numDrawBillBoards,
				numDrawBuildings, numDrawCities, numDrawGroundStations, numDrawSpaceStations, numDrawAtmospheres,
				numDrawPatches, numDrawPlanets, numDrawGasGiants, numDrawStars, numDrawShips, numBuffersCreated);
			frame_stat = 0;
			phys_stat = 0;
			Text::TextureFont::ClearGlyphCount();
			if (SDL_GetTicks() - last_stats > 1200)
				last_stats = SDL_GetTicks();
			else
				last_stats += 1000;
		}
		Pi::statSceneTris = 0;
		Pi::statNumPatches = 0;

#ifdef PIONEER_PROFILER
		const Uint32 profTicks = SDL_GetTicks();
		if (Pi::doProfileOne || (Pi::doProfileSlow && (profTicks - newTicks) > 100)) { // slow: < ~10fps
			Output("dumping profile data\n");
			Profiler::dumphtml(profilerPath.c_str());
			Pi::doProfileOne = false;
		}
#endif

#endif

#ifdef MAKING_VIDEO
		if (SDL_GetTicks() - last_screendump > 50) {
			last_screendump = SDL_GetTicks();
			std::string fname = stringf(Lang::SCREENSHOT_FILENAME_TEMPLATE, formatarg("index", dumpnum++));
			Screendump(fname.c_str(), Graphics::GetScreenWidth(), Graphics::GetScreenHeight());
		}
#endif /* MAKING_VIDEO */

		if (isRecordingVideo && (Pi::ffmpegFile != nullptr)) {
			Graphics::ScreendumpState sd;
			Pi::renderer->FrameGrab(sd);
			fwrite(sd.pixels.get(), sizeof(uint32_t) * Pi::renderer->GetWindowWidth() * Pi::renderer->GetWindowHeight(), 1, Pi::ffmpegFile);
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
		Pi::renderer->SetGrab(true);
		Pi::ui->SetMousePointerEnabled(false);
		doingMouseGrab = true;
	} else if (doingMouseGrab && !on) {
		Pi::renderer->SetGrab(false);
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
