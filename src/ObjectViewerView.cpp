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
#include "KeyBindings.h"
#include "Input.h"
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

#include <imgui/imgui.h>

#include <sstream>

const matrix4x4d INITIAL_CAM_ANGLES = matrix4x4d::RotateXMatrix(DEG2RAD(-30.0)) * matrix4x4d::RotateYMatrix(DEG2RAD(-15.0));
constexpr const float VIEW_START_DIST = 1000.0;
constexpr const float LIGHT_START_ANGLE = M_PI / 4.0;

constexpr const float MOVEMENT_SPEED = 0.5;
constexpr const float WHEEL_SENSITIVITY = .03f; // Should be a variable in user settings.

ObjectViewerView::ObjectViewerView() :
	UIView(),
	m_viewingDist(VIEW_START_DIST),
	m_lastTarget(nullptr),
	m_newTarget(nullptr),
	m_camRot(INITIAL_CAM_ANGLES),
	m_camTwist(matrix3x3d::Identity()),
	m_lightAngle(LIGHT_START_ANGLE),
	m_lightRotate(RotateLight::NONE),
	m_zoomChange(Zooming::NONE)
{
	SetTransparency(true);

	float znear;
	float zfar;
	RendererLocator::getRenderer()->GetNearFarRange(znear, zfar);

	const float fovY = GameConfSingleton::getInstance().Float("FOVVertical");
	RefCountedPtr<CameraContext> cameraContext;
	cameraContext.Reset(new CameraContext(Graphics::GetScreenWidth(), Graphics::GetScreenHeight(), fovY, znear, zfar));
	m_camera.reset(new Camera(cameraContext));

	cameraContext->SetCameraFrame(Frame::GetRootFrameId());
	cameraContext->SetCameraPosition(vector3d(0.0));
	cameraContext->SetCameraOrient(matrix3x3d::Identity());

	RegisterInputBindings();
}

ObjectViewerView::~ObjectViewerView()
{
	Pi::input->RemoveInputFrame(m_inputFrame.get());
}

void ObjectViewerView::RegisterInputBindings()
{
	using namespace KeyBindings;
	using namespace std::placeholders;

	m_inputFrame.reset(new InputFrame("ObjectViewer"));

	auto &page = Pi::input->GetBindingPage("ObjectViewer");
	page.shouldBeTranslated = false;

	auto &groupMisce = page.GetBindingGroup("Miscellaneous");

	m_objectViewerBindings.resetZoom = m_inputFrame->AddActionBinding("ResetZoom", groupMisce, ActionBinding(SDLK_SPACE));

	auto &groupVMC = page.GetBindingGroup("ViewMovementControls");
	m_objectViewerBindings.zoom = m_inputFrame->AddAxisBinding("Zoom", groupVMC, AxisBinding(SDLK_KP_PLUS, SDLK_KP_MINUS));
	m_objectViewerBindings.rotateLeftRight = m_inputFrame->AddAxisBinding("RotateLeftRight", groupVMC, AxisBinding(SDLK_LEFT, SDLK_RIGHT));
	m_objectViewerBindings.rotateUpDown = m_inputFrame->AddAxisBinding("RotateUpDown", groupVMC, AxisBinding(SDLK_DOWN, SDLK_UP));

	m_objectViewerBindings.rotateLightLeft = m_inputFrame->AddActionBinding("RotateLightLeft", groupVMC, ActionBinding(SDLK_r));
	m_objectViewerBindings.rotateLightRight = m_inputFrame->AddActionBinding("RotateLightRight", groupVMC, ActionBinding(SDLK_f));

	Pi::input->PushInputFrame(m_inputFrame.get());
}

void ObjectViewerView::OnSwitchTo()
{
	// rotate X is vertical
	// rotate Y is horizontal
	m_camRot = INITIAL_CAM_ANGLES;
	m_lightAngle = LIGHT_START_ANGLE;

	if (m_bindingLock) m_bindingLock.reset();

	m_bindingLock = Pi::input->DisableAllInputFrameExcept(m_inputFrame.get());

	m_inputFrame->SetActive(true);

	UIView::OnSwitchTo();
}

void ObjectViewerView::OnSwitchFrom()
{
	m_bindingLock.reset();

	m_inputFrame->SetActive(false);

	UIView::OnSwitchFrom();
}

Body *getATarget() {
	Body *body = GameLocator::getGame()->GetPlayer()->GetNavTarget();
	if (body == nullptr) body = GameLocator::getGame()->GetPlayer()->GetCombatTarget();
	if (body == nullptr) body = GameLocator::getGame()->GetPlayer();
	return body;
}

void ObjectViewerView::Update(const float frameTime)
{
	if (GameLocator::getGame() != nullptr) {
		// Make refactor easier when a target will be independent from game
		m_newTarget = getATarget();
	}
	if (m_newTarget != m_lastTarget) {
		m_lastTarget = m_newTarget;
		// Reset view parameter for new target.
		OnResetViewParams();
		// Update planet parameters for new target
		OnReloadSBData();
	}
	if (m_lastTarget == nullptr) return;

	const float moveSpeed = MOVEMENT_SPEED * WHEEL_SENSITIVITY * Pi::input->GetMoveSpeedShiftModifier();
	float move = moveSpeed * frameTime;

	if (m_objectViewerBindings.zoom->IsActive()) {
		if (m_objectViewerBindings.zoom->GetValue() > 0) {
			m_zoomChange = Zooming::IN;
		} else {
			m_zoomChange = Zooming::OUT;
		}
	}

	float zoom_change = 1.0;
	switch (m_zoomChange) {
	case Zooming::IN : zoom_change += move; break;
	case Zooming::OUT : zoom_change -= move; break;
	default: break;
	}
	m_viewingDist *= zoom_change;

	float min_distance = 10.0;
	if (m_lastTarget->IsType(Object::TERRAINBODY)) {
        TerrainBody *tb = static_cast<TerrainBody *>(m_lastTarget);
        min_distance = tb->GetSystemBodyRadius();
	} else {
		min_distance = m_lastTarget->GetClipRadius() * 0.5;
	}
	m_viewingDist = Clamp(m_viewingDist, min_distance, 1e12f);
	m_zoomChange = Zooming::NONE;

	if (m_objectViewerBindings.resetZoom->IsActive() && m_lastTarget != nullptr) {
		OnResetViewParams();
	}

	if (m_objectViewerBindings.rotateLightLeft->IsActive()) {
		m_lightRotate = RotateLight::LEFT;
	}
	if (m_objectViewerBindings.rotateLightRight->IsActive()) {
		m_lightRotate = RotateLight::RIGHT;
	}

	switch (m_lightRotate) {
	case RotateLight::LEFT : m_lightAngle += move * 30.0; break;
	case RotateLight::RIGHT : m_lightAngle -= move * 30.0; break;
	default: break;
	}
	m_lightAngle = Clamp(m_lightAngle, float(-DEG2RAD(180.0)), float(DEG2RAD(180.0)));
	m_lightRotate = RotateLight::NONE;

	if (m_objectViewerBindings.rotateUpDown->IsActive()) {
		m_camRot = matrix4x4d::RotateXMatrix(m_objectViewerBindings.rotateUpDown->GetValue() * move * 5.0) * m_camRot;
	}
	if (m_objectViewerBindings.rotateLeftRight->IsActive()) {
		m_camRot = matrix4x4d::RotateYMatrix(m_objectViewerBindings.rotateLeftRight->GetValue() * move * 5.0) * m_camRot;
	}

	auto motion = Pi::input->GetMouseMotion(MouseMotionBehaviour::Rotate);
	if (std::get<0>(motion)) {
		m_camRot = matrix4x4d::RotateXMatrix(-0.002 * std::get<2>(motion)) *
			matrix4x4d::RotateYMatrix(-0.002 * std::get<1>(motion)) * m_camRot;
	} else {
		motion = Pi::input->GetMouseMotion(MouseMotionBehaviour::DriveShip);
		if (std::get<0>(motion)) {
			m_camTwist = matrix3x3d::RotateX(-0.002 * std::get<2>(motion)) *
				matrix3x3d::RotateY(-0.002 * std::get<1>(motion)) * m_camTwist;
			//m_camTwist.Print();
		}
	}
	UIView::Update(frameTime);
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

	vector3d camPos = vector3d(0., 0., -m_viewingDist);
	RefCountedPtr<CameraContext> cameraContext = m_camera->GetContext();
	cameraContext->SetCameraOrient(m_camTwist);
	cameraContext->SetCameraPosition(camPos);
	cameraContext->BeginFrame();

	if (m_lastTarget) {
		if (m_lastTarget->IsType(Object::STAR)) {
			light.SetPosition(vector3f(0.f));
		} else {
			vector3f pos = vector3f(m_camTwist * vector3d(std::sin(m_lightAngle), 1.0, std::cos(m_lightAngle)));
			light.SetPosition(pos);
		}
		RendererLocator::getRenderer()->SetLights(1, &light);

		matrix4x4d twistMat(m_camTwist);
		m_lastTarget->Render(m_camera.get(), m_camTwist * camPos, twistMat * m_camRot);

		// industry-standard red/green/blue XYZ axis indiactor
		RendererLocator::getRenderer()->SetTransform(twistMat * matrix4x4d::Translation(camPos) * m_camRot * matrix4x4d::ScaleMatrix(m_lastTarget->GetClipRadius() * 2.0));
		Graphics::Drawables::GetAxes3DDrawable(RendererLocator::getRenderer())->Draw(RendererLocator::getRenderer());
	}

	UIView::Draw3D();

	cameraContext->EndFrame();
}

void ObjectViewerView::DrawUI(const float frameTime)
{
	if (Pi::IsConsoleActive()) return;

	std::ostringstream pathStr;
	if (m_lastTarget) {
		// fill in pathStr from sp values and sys->GetName()
		static const std::string comma(", ");
		const SystemBody *psb = m_lastTarget->GetSystemBody();
		if (psb) {
			const SystemPath sp = psb->GetPath();
			pathStr << m_lastTarget->GetSystemBody()->GetName() << " (" << sp.sectorX << comma << sp.sectorY << comma << sp.sectorZ << comma << sp.systemIndex << comma << sp.bodyIndex << ")";
		} else {
			pathStr << "<unknown>";
		}
	} else {
		pathStr << "<no object>";
	}

	char buf[128];
	snprintf(buf, sizeof(buf), "View dist: %s     Object: %s\nSystemPath: %s",
		format_distance(m_viewingDist).c_str(), (m_lastTarget ? m_lastTarget->GetLabel().c_str() : "<none>"),
		pathStr.str().c_str());

	vector2f screen(Graphics::GetScreenWidth(), Graphics::GetScreenHeight());
	if (!(screen == m_screen)) {
		m_screen = screen;
		vector2f upperLeft(screen.x * 2.0 / 3.0, screen.y);
		vector2f bottomRight(screen.x, 0);
		ImGui::SetNextWindowPos(ImVec2(0.0, 0.0), 0, ImVec2(0.0, 0.0));
	}

	ImGui::SetNextWindowBgAlpha(0.7f);
	ImGui::Begin("ObjectViewer", nullptr, ImGuiWindowFlags_NoScrollbar
					| ImGuiWindowFlags_NoCollapse
					| ImGuiWindowFlags_AlwaysAutoResize
					| ImGuiWindowFlags_NoSavedSettings
					| ImGuiWindowFlags_NoFocusOnAppearing
					| ImGuiWindowFlags_NoBringToFrontOnFocus
					);

	ImGui::TextUnformatted(buf);

	if (m_lastTarget) {
		ImGui::Separator();
		switch (m_lastTarget->GetType()) {
			case Object::CARGOBODY: ImGui::TextUnformatted("Type is CargoBody"); break;
			case Object::PLAYER:
			case Object::SHIP:
			{
				const ShipType *st = static_cast<Ship *>(m_lastTarget)->GetShipType();
				if (st != nullptr) {
					ImGui::Text("Ship model %s", st->id.c_str());
				}
			}
			break;
			case Object::SPACESTATION: ImGui::TextUnformatted("Type is SpaceStation"); break;
			case Object::MISSILE: ImGui::TextUnformatted("Type is Missile"); break;
			case Object::CITYONPLANET: ImGui::TextUnformatted("Type is CityOnPlanet"); break;
			case Object::HYPERSPACECLOUD: ImGui::TextUnformatted("Type is HyperspaceCloud"); break;
			case Object::PLANET:
			case Object::STAR:
			case Object::TERRAINBODY:
			{
				ImGui::InputFloat("Mass (earths)", &m_sbMass, 0.01f, 1.0f, "%.4f");
				ImGui::InputFloat("Radius (earths)", &m_sbRadius, 0.01f, 1.0f, "%.4f");
				const int step = 1, step_fast = 10;
				ImGui::InputScalar("Integer seed", ImGuiDataType_U32, &m_sbSeed, &step, &step_fast, "%u");
				ImGui::InputFloat("Volatile gases (>= 0)", &m_sbVolatileGas, 0.01f, 0.1f, "%.4f");
				m_sbVolatileGas = Clamp(m_sbVolatileGas, 0.0f, 10.0f);
				ImGui::InputFloat("Volatile liquid (0-1)", &m_sbVolatileLiquid, 0.01f, 0.1f, "%.4f");
				m_sbVolatileLiquid = Clamp(m_sbVolatileLiquid, 0.0f, 1.0f);
				ImGui::InputFloat("Volatile ices (0-1)", &m_sbVolatileIces, 0.01f, 0.1f, "%.4f");
				m_sbVolatileIces = Clamp(m_sbVolatileIces, 0.0f, 1.0f);
				ImGui::InputFloat("Life (0-1)", &m_sbLife, 0.01f, 0.1f, "%.4f");
				m_sbLife = Clamp(m_sbLife, 0.0f, 1.0f);
				ImGui::InputFloat("Volcanicity (0-1)", &m_sbVolcanicity, 0.01f, 1.0f, "%.4f");
				m_sbVolcanicity = Clamp(m_sbVolcanicity, 0.0f, 1.0f);
				ImGui::InputFloat("Crust metallicity (0-1)", &m_sbMetallicity, 0.01f, 1.0f, "%.4f");
				m_sbMetallicity = Clamp(m_sbMetallicity, 0.0f, 1.0f);

				ImGui::Button("Prev Seed");
				if (ImGui::IsItemClicked(0)) OnPrevSeed();
				ImGui::SameLine();
				ImGui::Button("Random Seed");
				if (ImGui::IsItemClicked(0)) OnRandomSeed();
				ImGui::SameLine();
				ImGui::Button("Next Seed");
				if (ImGui::IsItemClicked(0)) OnNextSeed();
				ImGui::Button("Reset Changes");
				if (ImGui::IsItemClicked(0)) OnReloadSBData();
				ImGui::SameLine();
				ImGui::Button("Apply Changes");
				if (ImGui::IsItemClicked(0)) OnChangeTerrain();
			}
			break;
			case Object::PROJECTILE:
			default: break;
		};
	}

	ImGui::Separator();
	ImGui::SliderAngle("Light Angle", &m_lightAngle, -180.0f, +180.0f);
	ImGui::Button("Reset View\nParameters");
	if (ImGui::IsItemClicked(0)) OnResetViewParams();
	ImGui::SameLine();
	ImGui::Button("Reset Twist\nParameters");
	if (ImGui::IsItemClicked(0)) OnResetTwistMatrix();
	ImGui::End();
}

void ObjectViewerView::OnResetViewParams()
{
	if (m_lastTarget != nullptr) {
		m_viewingDist = m_lastTarget->GetClipRadius() * 2.0f;
	} else {
		m_viewingDist = VIEW_START_DIST;
	}
	m_lightAngle = LIGHT_START_ANGLE;
	m_camTwist = matrix3x3d::Identity();
	m_camRot = INITIAL_CAM_ANGLES;
}

void ObjectViewerView::OnResetTwistMatrix()
{
	// TODO: m_camTwist is not well integrated, however it works
	// for roles it is introduced for, which is to look at planet in perspective.
	// All of that "crap" is needed because of Camera doesn't do its job very well,
	// which seems to be another big TODO...
	m_camTwist = matrix3x3d::Identity();
}

void ObjectViewerView::OnReloadSBData()
{
	if (m_lastTarget->IsType(Object::TERRAINBODY)) {
		TerrainBody *tbody = static_cast<TerrainBody *>(m_lastTarget);
		const SystemBody *sbody = tbody->GetSystemBody();
		m_sbMass = sbody->GetMassAsFixed().ToFloat();
		m_sbRadius = sbody->GetRadiusAsFixed().ToFloat();
		m_sbSeed = sbody->GetSeed();
		m_sbVolatileGas = sbody->GetVolatileGas();
		m_sbVolatileLiquid = sbody->GetVolatileLiquid();
		m_sbVolatileIces = sbody->GetVolatileIces();
		m_sbLife = sbody->GetLife();
		m_sbVolcanicity = sbody->GetVolcanicity();
		m_sbMetallicity = sbody->GetMetallicity();
	}
}

void ObjectViewerView::OnChangeTerrain()
{
	if (!m_lastTarget) return;

	TerrainBody *tbody = static_cast<TerrainBody *>(m_lastTarget);
	SystemBody *sbody = const_cast<SystemBody *>(tbody->GetSystemBody());

	if (!sbody) return;

	sbody->m_seed = m_sbSeed;
	sbody->m_radius = fixed(65536.0 * m_sbRadius, 65536);
	sbody->m_mass = fixed(65536.0 * m_sbMass, 65536);
	sbody->m_metallicity = fixed(65536.0 * m_sbMetallicity, 65536);
	sbody->m_volatileGas = fixed(65536.0 * m_sbVolatileGas, 65536);
	sbody->m_volatileLiquid = fixed(65536.0 * m_sbVolatileLiquid, 65536);
	sbody->m_volatileIces = fixed(65536.0 * m_sbVolatileIces, 65536);
	sbody->m_volcanicity = fixed(65536.0 * m_sbVolcanicity, 65536);
	sbody->m_life = m_sbLife;

	// force reload
	TerrainBody::OnChangeDetailLevel(GameConfSingleton::getDetail().planets);
}

void ObjectViewerView::OnRandomSeed()
{
	m_sbSeed = RandomSingleton::getInstance().Int32();
}

void ObjectViewerView::OnNextSeed()
{
	m_sbSeed++;
}

void ObjectViewerView::OnPrevSeed()
{
	m_sbSeed--;
}

void ObjectViewerView::OnLightRotateLeft()
{
	m_lightRotate = RotateLight::LEFT;
}

void ObjectViewerView::OnLightRotateRight()
{
	m_lightRotate = RotateLight::RIGHT;
}

#endif
