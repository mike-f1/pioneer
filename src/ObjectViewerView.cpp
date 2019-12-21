// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "buildopts.h"

#if WITH_OBJECTVIEWER

#include "ObjectViewerView.h"

#include "Camera.h"
#include "Frame.h"
#include "Game.h"
#include "GameConfig.h"
#include "GameConfSingleton.h"
#include "GameLocator.h"
#include "Pi.h"
#include "Player.h"
#include "Random.h"
#include "RandomSingleton.h"
#include "StringF.h"
#include "TerrainBody.h"
#include "galaxy/SystemBody.h"
#include "graphics/Drawables.h"
#include "graphics/Light.h"
#include "graphics/Renderer.h"
#include "graphics/RendererLocator.h"
#include "terrain/Terrain.h"

#include "gui/GuiBox.h"
#include "gui/GuiButton.h"
#include "gui/GuiLabel.h"
#include "gui/GuiScreen.h"
#include "gui/GuiTextEntry.h"

#include <sstream>

constexpr const float VIEW_DIST = 1000.0;

ObjectViewerView::ObjectViewerView(Game *game) :
	UIView()
{
	SetTransparency(true);
	m_viewingDist = VIEW_DIST;
	m_camRot = matrix4x4d::Identity();

	float size[2];
	GetSizeRequested(size);

	SetTransparency(true);

	float znear;
	float zfar;
	RendererLocator::getRenderer()->GetNearFarRange(znear, zfar);

	const float fovY = GameConfSingleton::getInstance().Float("FOVVertical");
	m_cameraContext.Reset(new CameraContext(Graphics::GetScreenWidth(), Graphics::GetScreenHeight(), fovY, znear, zfar));
	m_camera.reset(new Camera(m_cameraContext));

	m_cameraContext->SetCameraFrame(game->GetPlayer()->GetFrame());
	m_cameraContext->SetCameraPosition(game->GetPlayer()->GetInterpPosition() + vector3d(0, 0, m_viewingDist));
	m_cameraContext->SetCameraOrient(matrix3x3d::Identity());

	m_infoLabel = new Gui::Label("");
	Add(m_infoLabel, 2, Gui::Screen::GetHeight() - 66 - Gui::Screen::GetFontHeight());

	m_vbox = new Gui::VBox();
	Add(m_vbox, 580, 2);

	m_vbox->PackEnd(new Gui::Label("Mass (earths):"));
	m_sbodyMass = new Gui::TextEntry();
	m_vbox->PackEnd(m_sbodyMass);

	m_vbox->PackEnd(new Gui::Label("Radius (earths):"));
	m_sbodyRadius = new Gui::TextEntry();
	m_vbox->PackEnd(m_sbodyRadius);

	m_vbox->PackEnd(new Gui::Label("Integer seed:"));
	m_sbodySeed = new Gui::TextEntry();
	m_vbox->PackEnd(m_sbodySeed);

	m_vbox->PackEnd(new Gui::Label("Volatile gases (>= 0):"));
	m_sbodyVolatileGas = new Gui::TextEntry();
	m_vbox->PackEnd(m_sbodyVolatileGas);

	m_vbox->PackEnd(new Gui::Label("Volatile liquid (0-1):"));
	m_sbodyVolatileLiquid = new Gui::TextEntry();
	m_vbox->PackEnd(m_sbodyVolatileLiquid);

	m_vbox->PackEnd(new Gui::Label("Volatile ices (0-1):"));
	m_sbodyVolatileIces = new Gui::TextEntry();
	m_vbox->PackEnd(m_sbodyVolatileIces);

	m_vbox->PackEnd(new Gui::Label("Life (0-1):"));
	m_sbodyLife = new Gui::TextEntry();
	m_vbox->PackEnd(m_sbodyLife);

	m_vbox->PackEnd(new Gui::Label("Volcanicity (0-1):"));
	m_sbodyVolcanicity = new Gui::TextEntry();
	m_vbox->PackEnd(m_sbodyVolcanicity);

	m_vbox->PackEnd(new Gui::Label("Crust metallicity (0-1):"));
	m_sbodyMetallicity = new Gui::TextEntry();
	m_vbox->PackEnd(m_sbodyMetallicity);

	Gui::LabelButton *b = new Gui::LabelButton(new Gui::Label("Change planet terrain type"));
	b->onClick.connect(sigc::mem_fun(this, &ObjectViewerView::OnChangeTerrain));
	m_vbox->PackEnd(b);

	Gui::HBox *hbox = new Gui::HBox();

	b = new Gui::LabelButton(new Gui::Label("Prev Seed"));
	b->onClick.connect(sigc::mem_fun(this, &ObjectViewerView::OnPrevSeed));
	hbox->PackEnd(b);

	b = new Gui::LabelButton(new Gui::Label("Random Seed"));
	b->onClick.connect(sigc::mem_fun(this, &ObjectViewerView::OnRandomSeed));
	hbox->PackEnd(b);

	b = new Gui::LabelButton(new Gui::Label("Next Seed"));
	b->onClick.connect(sigc::mem_fun(this, &ObjectViewerView::OnNextSeed));
	hbox->PackEnd(b);

	m_vbox->PackEnd(hbox);
}

void ObjectViewerView::Draw3D()
{
	PROFILE_SCOPED()
	RendererLocator::getRenderer()->ClearScreen();
	float znear, zfar;
	RendererLocator::getRenderer()->GetNearFarRange(znear, zfar);
	RendererLocator::getRenderer()->SetPerspectiveProjection(75.f, RendererLocator::getRenderer()->GetDisplayAspect(), znear, zfar);
	RendererLocator::getRenderer()->SetTransform(matrix4x4f::Identity());

	Graphics::Light light;
	light.SetType(Graphics::Light::LIGHT_DIRECTIONAL);

	const int btnState = Pi::input.MouseButtonState(SDL_BUTTON_RIGHT);
	if (btnState) {
		int m[2];
		Pi::input.GetMouseMotion(m);
		m_camRot = matrix4x4d::RotateXMatrix(-0.002 * m[1]) *
			matrix4x4d::RotateYMatrix(-0.002 * m[0]) * m_camRot;
		m_cameraContext->SetCameraPosition(GameLocator::getGame()->GetPlayer()->GetInterpPosition() + vector3d(0, 0, m_viewingDist));
		m_cameraContext->BeginFrame();
		m_camera->Update();
	}

	if (m_lastTarget) {
		if (m_lastTarget->IsType(Object::STAR))
			light.SetPosition(vector3f(0.f));
		else {
			light.SetPosition(vector3f(0.577f));
		}
		RendererLocator::getRenderer()->SetLights(1, &light);

		m_lastTarget->Render(m_camera.get(), vector3d(0, 0, -m_viewingDist), m_camRot);

		// industry-standard red/green/blue XYZ axis indiactor
		RendererLocator::getRenderer()->SetTransform(matrix4x4d::Translation(vector3d(0, 0, -m_viewingDist)) * m_camRot * matrix4x4d::ScaleMatrix(m_lastTarget->GetClipRadius() * 2.0));
		Graphics::Drawables::GetAxes3DDrawable(RendererLocator::getRenderer())->Draw(RendererLocator::getRenderer());
	}

	UIView::Draw3D();
	if (btnState) {
		m_cameraContext->EndFrame();
	}
}

void ObjectViewerView::OnSwitchTo()
{
	// rotate X is vertical
	// rotate Y is horizontal
	m_camRot = matrix4x4d::RotateXMatrix(DEG2RAD(-30.0)) * matrix4x4d::RotateYMatrix(DEG2RAD(-15.0));
	UIView::OnSwitchTo();
}

void ObjectViewerView::Update(const float frameTime)
{
	if (Pi::input.KeyState(SDLK_PERIOD)) m_viewingDist *= 0.99f;
	if (Pi::input.KeyState(SDLK_MINUS)) m_viewingDist *= 1.01f;
	if (Pi::input.KeyState(SDLK_SPACE) && m_lastTarget != nullptr) {
		m_viewingDist = m_lastTarget->GetClipRadius() * 2.0f;
	}
	m_viewingDist = Clamp(m_viewingDist, 10.0f, 1e12f);

	char buf[128];
	Body *body = GameLocator::getGame()->GetPlayer()->GetNavTarget();
	if (body == nullptr) body = GameLocator::getGame()->GetPlayer()->GetCombatTarget();

	if (body && (body != m_lastTarget)) {
		// Reset view distance for new target.
		m_viewingDist = body->GetClipRadius() * 2.0f;
		m_lastTarget = body;

		if (body->IsType(Object::TERRAINBODY)) {
			TerrainBody *tbody = static_cast<TerrainBody *>(body);
			const SystemBody *sbody = tbody->GetSystemBody();
			m_sbodyVolatileGas->SetText(stringf("%0{f.3}", sbody->GetVolatileGas()));
			m_sbodyVolatileLiquid->SetText(stringf("%0{f.3}", sbody->GetVolatileLiquid()));
			m_sbodyVolatileIces->SetText(stringf("%0{f.3}", sbody->GetVolatileIces()));
			m_sbodyLife->SetText(stringf("%0{f.3}", sbody->GetLife()));
			m_sbodyVolcanicity->SetText(stringf("%0{f.3}", sbody->GetVolcanicity()));
			m_sbodyMetallicity->SetText(stringf("%0{f.3}", sbody->GetMetallicity()));
			m_sbodySeed->SetText(stringf("%0{i}", int(sbody->GetSeed())));
			m_sbodyMass->SetText(stringf("%0{f}", sbody->GetMassAsFixed().ToFloat()));
			m_sbodyRadius->SetText(stringf("%0{f}", sbody->GetRadiusAsFixed().ToFloat()));
		}
	}

	std::ostringstream pathStr;
	if (body) {
		// fill in pathStr from sp values and sys->GetName()
		static const std::string comma(", ");
		const SystemBody *psb = body->GetSystemBody();
		if (psb) {
			const SystemPath sp = psb->GetPath();
			pathStr << body->GetSystemBody()->GetName() << " (" << sp.sectorX << comma << sp.sectorY << comma << sp.sectorZ << comma << sp.systemIndex << comma << sp.bodyIndex << ")";
		} else {
			pathStr << "<unknown>";
		}
	}

	snprintf(buf, sizeof(buf), "View dist: %s     Object: %s\nSystemPath: %s",
		format_distance(m_viewingDist).c_str(), (body ? body->GetLabel().c_str() : "<none>"),
		pathStr.str().c_str());
	m_infoLabel->SetText(buf);

	if (body && body->IsType(Object::TERRAINBODY))
		m_vbox->ShowAll();
	else
		m_vbox->HideAll();

	UIView::Update(frameTime);
}

void ObjectViewerView::OnChangeTerrain()
{
	const fixed volatileGas = fixed(65536.0 * atof(m_sbodyVolatileGas->GetText().c_str()), 65536);
	const fixed volatileLiquid = fixed(65536.0 * atof(m_sbodyVolatileLiquid->GetText().c_str()), 65536);
	const fixed volatileIces = fixed(65536.0 * atof(m_sbodyVolatileIces->GetText().c_str()), 65536);
	const fixed life = fixed(65536.0 * atof(m_sbodyLife->GetText().c_str()), 65536);
	const fixed volcanicity = fixed(65536.0 * atof(m_sbodyVolcanicity->GetText().c_str()), 65536);
	const fixed metallicity = fixed(65536.0 * atof(m_sbodyMetallicity->GetText().c_str()), 65536);
	const fixed mass = fixed(65536.0 * atof(m_sbodyMass->GetText().c_str()), 65536);
	const fixed radius = fixed(65536.0 * atof(m_sbodyRadius->GetText().c_str()), 65536);

	// XXX this is horrendous, but probably safe for the moment. all bodies,
	// terrain, whatever else holds a const pointer to the same toplevel
	// sbody. one day objectviewer should be far more contained and not
	// actually modify the space
	Body *body = GameLocator::getGame()->GetPlayer()->GetNavTarget();
	SystemBody *sbody = const_cast<SystemBody *>(body->GetSystemBody());

	sbody->m_seed = atoi(m_sbodySeed->GetText().c_str());
	sbody->m_radius = radius;
	sbody->m_mass = mass;
	sbody->m_metallicity = metallicity;
	sbody->m_volatileGas = volatileGas;
	sbody->m_volatileLiquid = volatileLiquid;
	sbody->m_volatileIces = volatileIces;
	sbody->m_volcanicity = volcanicity;
	sbody->m_life = life;

	// force reload
	TerrainBody::OnChangeDetailLevel(GameConfSingleton::getDetail().planets);
}

void ObjectViewerView::OnRandomSeed()
{
	m_sbodySeed->SetText(stringf("%0{i}", int(RandomSingleton::getInstance().Int32())));
	OnChangeTerrain();
}

void ObjectViewerView::OnNextSeed()
{
	m_sbodySeed->SetText(stringf("%0{i}", atoi(m_sbodySeed->GetText().c_str()) + 1));
	OnChangeTerrain();
}

void ObjectViewerView::OnPrevSeed()
{
	m_sbodySeed->SetText(stringf("%0{i}", atoi(m_sbodySeed->GetText().c_str()) - 1));
	OnChangeTerrain();
}

#endif
