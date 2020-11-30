// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "SectorView.h"

#include "Game.h"
#include "GameConfSingleton.h"
#include "GameConfig.h"
#include "GameLocator.h"
#include "GameSaveError.h"
#include "input/InputFrame.h"
#include "input/KeyBindings.h"
#include "LuaConstants.h"
#include "LuaObject.h"
#include "Player.h"
#include "Space.h"
#include "galaxy/Faction.h"
#include "galaxy/Galaxy.h"
#include "galaxy/GalaxyCache.h"
#include "galaxy/Sector.h"
#include "galaxy/StarSystem.h"
#include "graphics/Frustum.h"
#include "graphics/Graphics.h"
#include "graphics/Material.h"
#include "graphics/RenderState.h"
#include "graphics/Renderer.h"
#include "graphics/RendererLocator.h"
#include "graphics/VertexArray.h"
#include "graphics/VertexBuffer.h"
#include "libs/MathUtil.h"
#include "libs/StringF.h"
#include <algorithm>
#include <sstream>
#include <unordered_set>

#include <imgui/imgui.h>

using namespace Graphics;

constexpr int DRAW_RAD = 5;
#define INNER_RADIUS (Sector::SIZE * 1.5f)
#define OUTER_RADIUS (Sector::SIZE * float(DRAW_RAD))
constexpr float FAR_THRESHOLD = 10.0f;
constexpr float FAR_LIMIT = 38.f;
constexpr float FAR_MAX = 46.f;

constexpr float ZOOM_SPEED = 25;
constexpr float WHEEL_SENSITIVITY = .03f; // Should be a variable in user settings.
constexpr float ROTATION_SPEED_FACTOR = 0.3f;

SectorView::SectorView(const SystemPath &path, RefCountedPtr<Galaxy> galaxy, unsigned int cacheRadius) :
	UIView(),
	m_galaxy(galaxy),
	m_farMode(false)
{
	InitDefaults();

	m_rotX = m_rotXMovingTo = m_rotXDefault;
	m_rotZ = m_rotZMovingTo = m_rotZDefault;
	m_zoom = m_zoomMovingTo = m_zoomDefault;

	// XXX I have no idea if this is correct,
	// I just copied it from the one other place m_zoomClamped is set
	m_zoomClamped = Clamp(m_zoom, 1.f, FAR_LIMIT);

	m_inSystem = true;

	// XXX Simplify?
	RefCountedPtr<StarSystem> system = galaxy->GetStarSystem(path);
	m_current = system->GetPath();
	assert(!m_current.IsSectorPath());
	m_current = m_current.SystemOnly();
	m_selected = m_hyperspaceTarget = system->GetStars()[0]->GetPath(); // XXX This always selects the first star of the system

	m_matchTargetToSelection = true;
	m_automaticSystemSelection = true;
	m_drawUninhabitedLabels = false;
	m_drawVerticalLines = true;
	m_drawOutRangeLabels = false;
	m_showFactionColor = false;

	m_rebuildFarSector = false;

	InitObject(cacheRadius);

	GotoSystem(m_current);

	m_pos = m_posMovingTo;
}

SectorView::SectorView(const Json &jsonObj, RefCountedPtr<Galaxy> galaxy, unsigned int cacheRadius) :
	UIView(),
	m_galaxy(galaxy)
{
	InitDefaults();

	try {
		Json sectorViewObj = jsonObj["sector_view"];

		m_pos.x = m_posMovingTo.x = sectorViewObj["pos_x"];
		m_pos.y = m_posMovingTo.y = sectorViewObj["pos_y"];
		m_pos.z = m_posMovingTo.z = sectorViewObj["pos_z"];
		m_rotX = m_rotXMovingTo = sectorViewObj["rot_x"];
		m_rotZ = m_rotZMovingTo = sectorViewObj["rot_z"];
		m_zoom = m_zoomMovingTo = sectorViewObj["zoom"];
		// XXX I have no idea if this is correct,
		// I just copied it from the one other place m_zoomClamped is set
		m_zoomClamped = Clamp(m_zoom, 1.f, FAR_LIMIT);
		m_inSystem = sectorViewObj["in_system"];
		m_current = SystemPath::FromJson(sectorViewObj["current"]);
		m_selected = SystemPath::FromJson(sectorViewObj["selected"]);
		m_hyperspaceTarget = SystemPath::FromJson(sectorViewObj["hyperspace"]);
		m_matchTargetToSelection = sectorViewObj["match_target_to_selection"];
		m_automaticSystemSelection = sectorViewObj["automatic_system_selection"];
		m_drawUninhabitedLabels = sectorViewObj["draw_uninhabited_labels"];
		m_drawVerticalLines = sectorViewObj["draw_vertical_lines"];
		m_drawOutRangeLabels = sectorViewObj["draw_out_of_range_labels"];
		m_showFactionColor = sectorViewObj["show_faction_color"];

	} catch (Json::type_error &) {
		Output("Loading error in '%s' in function '%s' \n", __FILE__, __func__);
		throw SavedGameCorruptException();
	}

	InitObject(cacheRadius);
}

void SectorView::RegisterInputBindings()
{
	using namespace KeyBindings;
	using namespace std::placeholders;

	m_inputFrame = std::make_unique<InputFrame>("GeneralPanRotateZoom");

	m_sectorBindings.mapViewShiftForwardBackward = m_inputFrame->GetAxisBinding("BindMapViewShiftForwardBackward");
	m_sectorBindings.mapViewShiftLeftRight = m_inputFrame->GetAxisBinding("BindMapViewShiftLeftRight");
	m_sectorBindings.mapViewShiftUpDown = m_inputFrame->GetAxisBinding("BindMapViewShiftUpDown");

	m_sectorBindings.mapViewZoom = m_inputFrame->GetAxisBinding("BindMapViewZoom");

	m_sectorBindings.mapViewRotateLeftRight = m_inputFrame->GetAxisBinding("BindMapViewRotateLeftRight");
	m_sectorBindings.mapViewRotateUpDown = m_inputFrame->GetAxisBinding("BindMapViewRotateUpDown");

	m_sectorFrame = std::make_unique<InputFrame>("SectorView");

	auto &page2 = InputFWD::GetBindingPage("SectorView");

	auto &groupMisc = page2.GetBindingGroup("Miscellaneous");

	m_sectorBindings.mapLockHyperspaceTarget = m_sectorFrame->AddActionBinding("BindMapLockHyperspaceTarget", groupMisc, ActionBinding(SDLK_SPACE));
	m_sectorFrame->AddCallbackFunction("BindMapLockHyperspaceTarget", std::bind(&SectorView::OnMapLockHyperspaceToggle, this, _1));

	m_sectorBindings.mapToggleSelectionFollowView = m_sectorFrame->AddActionBinding("BindMapToggleSelectionFollowView", groupMisc, ActionBinding(SDLK_RETURN, SDLK_KP_ENTER));
	m_sectorFrame->AddCallbackFunction("BindMapToggleSelectionFollowView", std::bind(&SectorView::OnToggleSelectionFollowView, this, _1));

	m_sectorBindings.mapWarpToCurrent = m_sectorFrame->AddActionBinding("BindMapWarpToCurrent", groupMisc, ActionBinding(SDLK_c));
	m_sectorBindings.mapWarpToSelected = m_sectorFrame->AddActionBinding("BindMapWarpToSelection", groupMisc, ActionBinding(SDLK_g));
	m_sectorBindings.mapWarpToHyperspaceTarget = m_sectorFrame->AddActionBinding("BindMapWarpToHyperspaceTarget", groupMisc, ActionBinding(SDLK_h));
	m_sectorBindings.mapViewReset = m_sectorFrame->AddActionBinding("BindMapViewReset", groupMisc, ActionBinding(SDLK_t));
}

void SectorView::InitDefaults()
{
	m_rotXDefault = GameConfSingleton::getInstance().Float("SectorViewXRotation");
	m_rotZDefault = GameConfSingleton::getInstance().Float("SectorViewZRotation");
	m_zoomDefault = GameConfSingleton::getInstance().Float("SectorViewZoom");
	m_rotXDefault = Clamp(m_rotXDefault, -170.0f, -10.0f);
	m_zoomDefault = Clamp(m_zoomDefault, 0.1f, 5.0f);
	m_previousSearch = "";

	m_secPosFar = vector3f(INT_MAX, INT_MAX, INT_MAX);
	m_radiusFar = 0;
	m_cacheXMin = 0;
	m_cacheXMax = 0;
	m_cacheYMin = 0;
	m_cacheYMax = 0;
	m_cacheYMin = 0;
	m_cacheYMax = 0;

	m_drawRouteLines = true; // where should this go?!
	m_route = std::vector<SystemPath>();

}

void SectorView::InitObject(unsigned int cacheRadius)
{
	SetTransparency(true);

	m_lineVerts.reset(new Graphics::VertexArray(Graphics::ATTRIB_POSITION | Graphics::ATTRIB_DIFFUSE, 500));
	m_secLineVerts.reset(new Graphics::VertexArray(Graphics::ATTRIB_POSITION | Graphics::ATTRIB_DIFFUSE, 500));
	m_starVerts.reset(new Graphics::VertexArray(
		Graphics::ATTRIB_POSITION | Graphics::ATTRIB_DIFFUSE | Graphics::ATTRIB_UV0, 500));

	Graphics::RenderStateDesc rsd;
	m_solidState = RendererLocator::getRenderer()->CreateRenderState(rsd);

	rsd.blendMode = Graphics::BLEND_ALPHA;
	rsd.depthWrite = false;
	rsd.cullMode = CULL_NONE;
	m_alphaBlendState = RendererLocator::getRenderer()->CreateRenderState(rsd);

	Graphics::MaterialDescriptor bbMatDesc;
	bbMatDesc.effect = Graphics::EffectType::SPHEREIMPOSTOR;
	m_starMaterial.Reset(RendererLocator::getRenderer()->CreateMaterial(bbMatDesc));

	m_disk.reset(new Graphics::Drawables::Disk(RendererLocator::getRenderer(), m_solidState, Color::WHITE, 0.2f));

	m_sectorCache = m_galaxy->NewSectorSlaveCache();
    size_t filled = m_galaxy->FillSectorCache(m_sectorCache, m_current, cacheRadius);
    Output("SectorView cache pre-filled with %lu entries\n", filled);

	RegisterInputBindings();
}

SectorView::~SectorView()
{
}

void SectorView::SaveToJson(Json &jsonObj)
{
	Json sectorViewObj({}); // Create JSON object to contain sector view data.

	sectorViewObj["pos_x"] = m_pos.x;
	sectorViewObj["pos_y"] = m_pos.y;
	sectorViewObj["pos_z"] = m_pos.z;
	sectorViewObj["rot_x"] = m_rotX;
	sectorViewObj["rot_z"] = m_rotZ;
	sectorViewObj["zoom"] = m_zoom;
	sectorViewObj["in_system"] = m_inSystem;

	Json currentSystemObj({}); // Create JSON object to contain current system data.
	m_current.ToJson(currentSystemObj);
	sectorViewObj["current"] = currentSystemObj; // Add current system object to sector view object.

	Json selectedSystemObj({}); // Create JSON object to contain selected system data.
	m_selected.ToJson(selectedSystemObj);
	sectorViewObj["selected"] = selectedSystemObj; // Add selected system object to sector view object.

	Json hyperspaceSystemObj({}); // Create JSON object to contain hyperspace system data.
	m_hyperspaceTarget.ToJson(hyperspaceSystemObj);
	sectorViewObj["hyperspace"] = hyperspaceSystemObj; // Add hyperspace system object to sector view object.

	sectorViewObj["match_target_to_selection"] = m_matchTargetToSelection;
	sectorViewObj["automatic_system_selection"] = m_automaticSystemSelection;
	sectorViewObj["draw_uninhabited_labels"] = m_drawUninhabitedLabels;
	sectorViewObj["draw_vertical_lines"] = m_drawVerticalLines;
	sectorViewObj["draw_out_of_range_labels"] = m_drawOutRangeLabels;
	sectorViewObj["show_faction_color"] = m_showFactionColor;

	jsonObj["sector_view"] = sectorViewObj; // Add sector view object to supplied object.
}

#define FFRAC(_x) ((_x)-floor(_x))

void SectorView::Draw3D()
{
	PROFILE_SCOPED()

	m_lineVerts->Clear();
	m_secLineVerts->Clear();
	m_starVerts->Clear();

	m_farMode = m_zoomClamped > FAR_THRESHOLD;

	if (m_farMode)
		RendererLocator::getRenderer()->SetPerspectiveProjection(40.f, RendererLocator::getRenderer()->GetDisplayAspect(), 1.f, 600.f);
	else
		RendererLocator::getRenderer()->SetPerspectiveProjection(40.f, RendererLocator::getRenderer()->GetDisplayAspect(), 1.f, 300.f);

	RendererLocator::getRenderer()->ClearScreen();

	// units are lightyears, my friend
	matrix4x4f modelview = matrix4x4f::Identity();
	modelview.Translate(0.f, 0.f, -10.f - 10.f * m_zoom); // not zoomClamped, let us zoom out a bit beyond what we're drawing
	modelview.Rotate(DEG2RAD(m_rotX), 1.f, 0.f, 0.f);
	modelview.Rotate(DEG2RAD(m_rotZ), 0.f, 0.f, 1.f);
	modelview.Translate(-FFRAC(m_pos.x) * Sector::SIZE, -FFRAC(m_pos.y) * Sector::SIZE, -FFRAC(m_pos.z) * Sector::SIZE);

	Graphics::Renderer::MatrixTicket ticket(RendererLocator::getRenderer(), Graphics::MatrixMode::MODELVIEW);
	RendererLocator::getRenderer()->SetTransform(modelview);

	RefCountedPtr<const Sector> playerSec = m_sectorCache->GetCached(m_current);
	const vector3f playerPos = Sector::SIZE * vector3f(float(m_current.sectorX), float(m_current.sectorY), float(m_current.sectorZ)) + playerSec->m_systems[m_current.systemIndex].GetPosition();

	m_systems.clear();

	if (m_farMode) {
		DrawFarSectors(modelview);

		Graphics::Renderer *r = RendererLocator::getRenderer();
		Graphics::Renderer::StateTicket ticket2(r);
		r->SetOrthographicProjection(0, r->GetWindowWidth(), r->GetWindowHeight(), 0, -1, 1);
		r->SetTransform(matrix4x4f::Identity());

		PutDiamonds(m_systems);
	} else {
		DrawNearSectors(modelview);
	}

	RendererLocator::getRenderer()->SetTransform(matrix4x4f::Identity());

	// not quite the same as modelview in regard of the translation...
	matrix4x4f trans = matrix4x4f::Identity();
	trans.Translate(0.f, 0.f, -10.f - 10.f * m_zoom);
	trans.Rotate(DEG2RAD(m_rotX), 1.f, 0.f, 0.f);
	trans.Rotate(DEG2RAD(m_rotZ), 0.f, 0.f, 1.f);
	trans.Translate(-(m_pos.x) * Sector::SIZE, -(m_pos.y) * Sector::SIZE, -(m_pos.z) * Sector::SIZE);

	PrepareRouteLines(playerPos, trans);

	//draw star billboards in one go
	RendererLocator::getRenderer()->SetAmbientColor(Color(30, 30, 30));
	RendererLocator::getRenderer()->DrawTriangles(m_starVerts.get(), m_solidState, m_starMaterial.Get());

	//draw sector legs in one go
	if (!m_lineVerts->IsEmpty()) {
		m_lines.SetData(m_lineVerts->GetNumVerts(), &m_lineVerts->position[0], &m_lineVerts->diffuse[0]);
		m_lines.Draw(RendererLocator::getRenderer(), m_alphaBlendState);
	}

	//draw sector grid in one go
	if (!m_secLineVerts->IsEmpty()) {
		m_sectorlines.SetData(m_secLineVerts->GetNumVerts(), &m_secLineVerts->position[0], &m_secLineVerts->diffuse[0]);
		m_sectorlines.Draw(RendererLocator::getRenderer(), m_alphaBlendState);
	}

	//draw jump shere
	if (m_jumpSphere && m_playerHyperspaceRange > 0.0f) {
		Graphics::Renderer *r = RendererLocator::getRenderer();
		Graphics::Renderer::StateTicket ticket2(r);
		matrix4x4f trans2 = trans;
		trans2.Translate(playerPos);

		RendererLocator::getRenderer()->SetTransform(trans2 * matrix4x4f::ScaleMatrix(m_playerHyperspaceRange));
		m_jumpSphere->Draw(RendererLocator::getRenderer());
	}

	UIView::Draw3D();

}

void SectorView::DrawUI(const float frameTime)
{
	/*
	This will open a window with a list of displayed systems... It may be useful later
	ImGui::Begin("Systems Labelled");
	for (auto element : m_systems) {
		ImGui::Text("%s", element.first->GetName().c_str());
		if (ImGui::IsItemClicked(0)) {
			SystemPath path = SystemPath((*element.first).sx, (*element.first).sy, (*element.first).sz, (*element.first).idx);
			OnClickSystem(path);
		}
    }
    ImGui::End();
	*/
	// FIXME: Find a way to zoom in/out when hovering a window/label without having
	// an entire overlay in ImGui or having that overlay but not blocking input
	// FIXME: Sort labels basing on distance from point of view
	ImGuiStyle& style = ImGui::GetStyle();
	for (auto element : m_systems) {
		ImVec2 center(element.second.x, element.second.y);
		ImGui::SetNextWindowBgAlpha(0.7f);
		ImGui::Begin(element.first->GetName().c_str(), nullptr, ImGuiWindowFlags_NoTitleBar
					| ImGuiWindowFlags_NoResize
					| ImGuiWindowFlags_NoMove
					| ImGuiWindowFlags_NoScrollbar
					| ImGuiWindowFlags_NoCollapse
					| ImGuiWindowFlags_NoSavedSettings
					| ImGuiWindowFlags_NoFocusOnAppearing
					| ImGuiWindowFlags_NoBringToFrontOnFocus
					);
		std::string label;
		if (m_farMode) {
			label = element.first->GetName() + "\n" + element.first->GetFaction()->name;
		} else {
			label = element.first->GetName();
		}
		ImVec2 size = ImGui::CalcTextSize(label.c_str());
		size.x += style.WindowPadding.x * 2;
		size.y += style.WindowPadding.y * 2;

		center.x += 10; // ...and add something to make it depend on zoom
		center.y -= size.y / 2.0;
		ImGui::SetWindowPos(center);
		ImGui::SetWindowSize(size);
		// TODO May I should have used GetAdjoustedColor?
		Color labelColor = element.first->GetFaction()->colour;
		ImVec4 color(labelColor.r / 255., labelColor.g / 255., labelColor.b / 255., 1.0);
		ImGui::PushStyleColor(ImGuiCol_Text, color);
		ImGui::TextUnformatted(label.c_str());
		if (ImGui::IsItemClicked(0)) { // TODO: Replace with InputFrames, when they are mature enought
			SystemPath path = SystemPath((*element.first).sx, (*element.first).sy, (*element.first).sz, (*element.first).idx);
			OnClickSystem(path);
		}
		ImGui::PopStyleColor(1);
		ImGui::End();
	}
}

void SectorView::SetHyperspaceTarget(const SystemPath &path)
{
	m_hyperspaceTarget = path;
	m_matchTargetToSelection = false;
	onHyperspaceTargetChanged.emit();
}

void SectorView::FloatHyperspaceTarget()
{
	m_matchTargetToSelection = true;
}

void SectorView::ResetHyperspaceTarget()
{
	SystemPath old = m_hyperspaceTarget;
	m_hyperspaceTarget = m_selected;
	FloatHyperspaceTarget();

	if (!old.IsSameSystem(m_hyperspaceTarget)) {
		onHyperspaceTargetChanged.emit();
	}
}

void SectorView::GotoSector(const SystemPath &path)
{
	m_posMovingTo = vector3f(path.sectorX, path.sectorY, path.sectorZ);

	// for performance don't animate the travel if we're Far Zoomed
	if (m_zoomClamped > FAR_THRESHOLD) {
		m_pos = m_posMovingTo;
	}
}

void SectorView::GotoSystem(const SystemPath &path)
{
	RefCountedPtr<Sector> ps = m_sectorCache->GetCached(path);
	const vector3f &p = ps->m_systems[path.systemIndex].GetPosition();
	m_posMovingTo.x = path.sectorX + p.x / Sector::SIZE;
	m_posMovingTo.y = path.sectorY + p.y / Sector::SIZE;
	m_posMovingTo.z = path.sectorZ + p.z / Sector::SIZE;

	// for performance don't animate the travel if we're Far Zoomed
	if (m_zoomClamped > FAR_THRESHOLD) {
		m_pos = m_posMovingTo;
	}
}

void SectorView::SetSelected(const SystemPath &path)
{
	m_selected = path;

	if (m_matchTargetToSelection && m_selected != m_current) {
		m_hyperspaceTarget = m_selected;
		onHyperspaceTargetChanged.emit();
	}
}

void SectorView::SwapSelectedHyperspaceTarget()
{
	SystemPath tmpTarget = GetHyperspaceTarget();
	SetHyperspaceTarget(GetSelected());
	if (m_automaticSystemSelection) {
		GotoSystem(tmpTarget);
	} else {
		RefCountedPtr<StarSystem> system = m_galaxy->GetStarSystem(tmpTarget);
		SetSelected(system->GetStars()[0]->GetPath());
	}
}

void SectorView::OnClickSystem(const SystemPath &path)
{
	if (path.IsSameSystem(m_selected)) {
		RefCountedPtr<StarSystem> system = m_galaxy->GetStarSystem(path);
		if (system->GetNumStars() > 1 && m_selected.IsBodyPath()) {
			unsigned i;
			for (i = 0; i < system->GetNumStars(); ++i)
				if (system->GetStars()[i]->GetPath() == m_selected) break;
			if (i >= system->GetNumStars() - 1)
				SetSelected(system->GetStars()[0]->GetPath());
			else
				SetSelected(system->GetStars()[i + 1]->GetPath());
		} else {
			SetSelected(system->GetStars()[0]->GetPath());
		}
	} else {
		if (m_automaticSystemSelection) {
			GotoSystem(path);
		} else {
			RefCountedPtr<StarSystem> system = m_galaxy->GetStarSystem(path);
			SetSelected(system->GetStars()[0]->GetPath());
		}
	}
}

void SectorView::CollectSystems(RefCountedPtr<Sector> sec, const vector3f &origin, int drawRadius, SectorView::t_systemsAndPosVector &systems)
{
	PROFILE_SCOPED()

	// Although handled outside this function, if a growth is needed it is better to let it be consistent
	// but not to use std growth strategies as doubling which can be a waste of spaces
	systems.reserve(systems.size() + sec->m_systems.size() * 5);

	const matrix4x4f &m = RendererLocator::getRenderer()->GetCurrentModelView();
	const matrix4x4f &p = RendererLocator::getRenderer()->GetCurrentProjection();

	matrix4x4d model_mat, proj_mat;
	matrix4x4ftod(m, model_mat);
	matrix4x4ftod(p, proj_mat);
	Frustum frustum(model_mat, proj_mat);

	int32_t viewport[4];
	RendererLocator::getRenderer()->GetCurrentViewport(&viewport[0]);

	uint32_t sysIdx = 0;
	for (std::vector<Sector::System>::iterator sys = sec->m_systems.begin(); sys != sec->m_systems.end(); ++sys, ++sysIdx) {
		// skip the system if it doesn't fall within the sphere we're viewing.
		if ((m_pos * Sector::SIZE - (*sys).GetFullPosition()).Length() > drawRadius) continue;

		// if the system is the current system or target we can't skip it
		bool can_skip = !sys->IsSameSystem(m_selected) && !sys->IsSameSystem(m_hyperspaceTarget) && !sys->IsSameSystem(m_current);

		// skip if we have no population and won't drawn uninhabited systems
		if (can_skip && (sys->GetPopulation() <= 0 && !m_drawUninhabitedLabels)) continue;

		// skip the system if it belongs to a Faction we've toggled off and we can skip it
		if (can_skip && m_hiddenFactions.find(sys->GetFaction()) != m_hiddenFactions.end()) continue;

		// determine if system in hyperjump range or not
		RefCountedPtr<const Sector> playerSec = m_sectorCache->GetCached(m_current);
		float dist = Sector::DistanceBetween(sec, sysIdx, playerSec, m_current.systemIndex);
		bool inRange = dist <= m_playerHyperspaceRange;

		// skip if we're out of rangen and won't draw out of range systems systems
		if (can_skip && (!inRange && !m_drawOutRangeLabels)) continue;

		if (!(((inRange || m_drawOutRangeLabels) && (sys->GetPopulation() > 0 || m_drawUninhabitedLabels)) || !can_skip)) continue;

		// place the label
		vector3d pos;
		if (frustum.ProjectPoint(vector3d(sys->GetFullPosition() - origin), pos)) {
			// ok, need to use these formula to project correctly from Renderer to ImGui
			if (pos.z > 1.0f) continue;

			pos.x = pos.x * viewport[2] + viewport[0];
			pos.y = pos.y * viewport[3] + viewport[1];

			pos.y = RendererLocator::getRenderer()->GetWindowHeight() - pos.y;

			systems.push_back(std::make_pair(&(*sys), std::move(pos)));
		}
	}
}

SectorView::t_systemsAndPosVector SectorView::CollectHomeworlds(const vector3f &origin)
{
	PROFILE_SCOPED()

	SectorView::t_systemsAndPosVector homeworlds;
	homeworlds.reserve(m_visibleFactions.size());

	const matrix4x4f &m = RendererLocator::getRenderer()->GetCurrentModelView();
	const matrix4x4f &p = RendererLocator::getRenderer()->GetCurrentProjection();

	matrix4x4d model_mat, proj_mat;
	matrix4x4ftod(m, model_mat);
	matrix4x4ftod(p, proj_mat);
	Frustum frustum(model_mat, proj_mat);

	int32_t viewport[4];
	RendererLocator::getRenderer()->GetCurrentViewport(&viewport[0]);

	for (auto it = m_visibleFactions.begin(); it != m_visibleFactions.end(); ++it) {
		if ((*it)->hasHomeworld && m_hiddenFactions.find((*it)) == m_hiddenFactions.end()) {
			const Sector::System *sys = &m_sectorCache->GetCached((*it)->homeworld)->m_systems[(*it)->homeworld.systemIndex];
			if ((m_pos * Sector::SIZE - sys->GetFullPosition()).Length() > (m_zoomClamped / FAR_THRESHOLD) * OUTER_RADIUS) continue;

			vector3d pos;
			if (frustum.ProjectPoint(vector3d(sys->GetFullPosition() - origin), pos)) {
				if (pos.z > 1.0f) continue;

				// ok, need to use these formula to project correctly from Renderer to ImGui
				pos.x = pos.x * viewport[2] + viewport[0];
				pos.y = pos.y * viewport[3] + viewport[1];
				pos.y = RendererLocator::getRenderer()->GetWindowHeight() - pos.y;

				homeworlds.emplace_back(std::make_pair(sys, std::move(pos)));
			}
		}
	}
	return homeworlds;
}

void SectorView::PutDiamonds(const t_systemsAndPosVector &homeworlds)
{
	PROFILE_SCOPED()

	if (!m_material)
		m_material.Reset(RendererLocator::getRenderer()->CreateMaterial(Graphics::MaterialDescriptor()));

	for (auto element : homeworlds) {
		const vector3d &pos = element.second;

		// draw a big diamond for the location of the star
		constexpr float STARSIZE = 5;
		Graphics::VertexArray outline(Graphics::ATTRIB_POSITION, 4);
		outline.Add(vector3f(pos.x - STARSIZE - 1.f, pos.y, 0.f));
		outline.Add(vector3f(pos.x, pos.y + STARSIZE + 1.f, 0.f));
		outline.Add(vector3f(pos.x, pos.y - STARSIZE - 1.f, 0.f));
		outline.Add(vector3f(pos.x + STARSIZE + 1.f, pos.y, 0.f));
		m_material->diffuse = { 0, 0, 0, 255 };
		RendererLocator::getRenderer()->DrawTriangles(&outline, m_alphaBlendState, m_material.Get(), Graphics::TRIANGLE_STRIP);

		Graphics::VertexArray marker(Graphics::ATTRIB_POSITION, 4);
		marker.Add(vector3f(pos.x - STARSIZE, pos.y, 0.f));
		marker.Add(vector3f(pos.x, pos.y + STARSIZE, 0.f));
		marker.Add(vector3f(pos.x, pos.y - STARSIZE, 0.f));
		marker.Add(vector3f(pos.x + STARSIZE, pos.y, 0.f));
		if (m_showFactionColor) {
			m_material->diffuse = element.first->GetFaction()->colour;
		} else {
			m_material->diffuse = GalaxyEnums::starColors[element.first->GetStarType(0)];
		}
		RendererLocator::getRenderer()->DrawTriangles(&marker, m_alphaBlendState, m_material.Get(), Graphics::TRIANGLE_STRIP);
	}
}

void SectorView::AddStarBillboard(const matrix4x4f &trans, const vector3f &pos, const Color &col, float size)
{
	const matrix3x3f rot = trans.GetOrient().Transpose();

	const vector3f offset = trans * pos;

	const vector3f rotv1 = rot * vector3f(size / 2.f, -size / 2.f, 0.0f);
	const vector3f rotv2 = rot * vector3f(size / 2.f, size / 2.f, 0.0f);

	Graphics::VertexArray &va = *m_starVerts;
	va.Add(offset - rotv1, col, vector2f(0.f, 0.f)); //top left
	va.Add(offset - rotv2, col, vector2f(0.f, 1.f)); //bottom left
	va.Add(offset + rotv2, col, vector2f(1.f, 0.f)); //top right

	va.Add(offset + rotv2, col, vector2f(1.f, 0.f)); //top right
	va.Add(offset - rotv2, col, vector2f(0.f, 1.f)); //bottom left
	va.Add(offset + rotv1, col, vector2f(1.f, 1.f)); //bottom right
}

void SectorView::PrepareLegs(const matrix4x4f &trans, const vector3f &pos, int z_diff)
{
	const Color light(128, 128, 128);
	const Color dark(51, 51, 51);

	if (m_lineVerts->position.size() + 8 >= m_lineVerts->position.capacity()) {
		constexpr int grow_qty = 50;
		m_lineVerts->position.reserve(m_lineVerts->GetNumVerts() + 8 * grow_qty);
		m_lineVerts->diffuse.reserve(m_lineVerts->GetNumVerts() + 8 * grow_qty);
	}

	// draw system "leg"
	float z = -pos.z;
	if (z_diff >= 0) {
		z += abs(z_diff) * Sector::SIZE;
	} else {
		z -= abs(z_diff) * Sector::SIZE;
	}
	m_lineVerts->Add(trans * vector3f(0.f, 0.f, z), light);
	m_lineVerts->Add(trans * vector3f(0.f, 0.f, z * 0.5f), dark);
	m_lineVerts->Add(trans * vector3f(0.f, 0.f, z * 0.5f), dark);
	m_lineVerts->Add(trans * vector3f(0.f, 0.f, 0.f), light);

	//cross at other end
	m_lineVerts->Add(trans * vector3f(-0.1f, -0.1f, z), light);
	m_lineVerts->Add(trans * vector3f(0.1f, 0.1f, z), light);
	m_lineVerts->Add(trans * vector3f(-0.1f, 0.1f, z), light);
	m_lineVerts->Add(trans * vector3f(0.1f, -0.1f, z), light);
}

void SectorView::PrepareGrid(const matrix4x4f &trans, int radius)
{
	const Color darkgreen(0, 51, 0, 255);

	// TODO: Sure there's a better way but it's too much I'm stuck here :P
	double fractpart, intpart;
	fractpart = std::modf(m_pos.z, &intpart);
	int offset;
	if (fractpart > 0.5) offset = 1;
	else if (fractpart > 0.0 ) offset = 0;
	else if (fractpart > -0.5) offset = 1;
	else offset = 0;

	const size_t newNum = m_secLineVerts->GetNumVerts() + 4 * (2 * radius + 1);
	m_secLineVerts->position.reserve(newNum);
	m_secLineVerts->diffuse.reserve(newNum);
	for (int sx = -radius; sx <= radius; sx++) {
		// Draw lines in y direction:
		vector3f a = trans * vector3f(Sector::SIZE * sx, -Sector::SIZE * radius, Sector::SIZE * offset);
		vector3f b = trans * vector3f(Sector::SIZE * sx, Sector::SIZE * radius, Sector::SIZE * offset);
		m_secLineVerts->Add(a, darkgreen);
		m_secLineVerts->Add(b, darkgreen);
	}
	for (int sy = -radius; sy <= radius; sy++) {
		// Draw lines in x direction:
		vector3f a = trans * vector3f(Sector::SIZE * radius, Sector::SIZE * sy, Sector::SIZE * offset);
		vector3f b = trans * vector3f(-Sector::SIZE * radius, Sector::SIZE * sy, Sector::SIZE * offset);
		m_secLineVerts->Add(a, darkgreen);
		m_secLineVerts->Add(b, darkgreen);
	}
}

void SectorView::DrawNearSectors(const matrix4x4f &modelview)
{
	PROFILE_SCOPED()
	m_visibleFactions.clear();

	RefCountedPtr<const Sector> playerSec = m_sectorCache->GetCached(m_current);
	const vector3f playerPos = Sector::SIZE * vector3f(float(m_current.sectorX), float(m_current.sectorY), float(m_current.sectorZ)) + playerSec->m_systems[m_current.systemIndex].GetPosition();

	PrepareGrid(modelview, DRAW_RAD);

	for (int sx = -DRAW_RAD; sx <= DRAW_RAD; sx++) {
		for (int sy = -DRAW_RAD; sy <= DRAW_RAD; sy++) {
			for (int sz = -DRAW_RAD; sz <= DRAW_RAD; sz++) {
				matrix4x4f translation = matrix4x4f::Translation(Sector::SIZE * sx, Sector::SIZE * sy, Sector::SIZE * sz);
				DrawNearSector(int(floorf(m_pos.x)) + sx, int(floorf(m_pos.y)) + sy, int(floorf(m_pos.z)) + sz, playerPos,
					modelview * translation);
			}
		}
	}

	// ...then switch and do all the labels
	const vector3f secOrigin = vector3f(int(floorf(m_pos.x)), int(floorf(m_pos.y)), int(floorf(m_pos.z)));

	RendererLocator::getRenderer()->SetTransform(modelview);
	RendererLocator::getRenderer()->SetDepthRange(0, 1);
	m_systems.reserve(DRAW_RAD * DRAW_RAD * DRAW_RAD * 15);
	for (int sx = -DRAW_RAD; sx <= DRAW_RAD; sx++) {
		for (int sy = -DRAW_RAD; sy <= DRAW_RAD; sy++) {
			for (int sz = -DRAW_RAD; sz <= DRAW_RAD; sz++) {
				CollectSystems(m_sectorCache->GetCached(SystemPath(sx + secOrigin.x, sy + secOrigin.y, sz + secOrigin.z)), Sector::SIZE * secOrigin, Sector::SIZE * DRAW_RAD, m_systems);
			}
		}
	}
}

bool SectorView::MoveRouteItemUp(const std::vector<SystemPath>::size_type element)
{
	if (element == 0 || element >= m_route.size()) return false;

	std::swap(m_route[element - 1], m_route[element]);

	return true;
}

bool SectorView::MoveRouteItemDown(const std::vector<SystemPath>::size_type element)
{
	if (element >= m_route.size() - 1) return false;

	std::swap(m_route[element + 1], m_route[element]);

	return true;
}

void SectorView::AddToRoute(const SystemPath &path)
{
	m_route.push_back(path);
}

bool SectorView::RemoveRouteItem(const std::vector<SystemPath>::size_type element)
{
	if (element < m_route.size()) {
		m_route.erase(m_route.begin() + element);
		return true;
	} else {
		return false;
	}
}

void SectorView::ClearRoute()
{
	m_route.clear();
}

std::vector<SystemPath> SectorView::GetRoute()
{
	return m_route;
}

void SectorView::AutoRoute(const SystemPath &start, const SystemPath &target, std::vector<SystemPath> &outRoute) const
{
	const RefCountedPtr<const Sector> start_sec = m_galaxy->GetSector(start);
	const RefCountedPtr<const Sector> target_sec = m_galaxy->GetSector(target);

	// Get the player's hyperdrive from Lua, later used to calculate the duration between systems
	const ScopedTable hyperdrive = ScopedTable(LuaObject<Player>::CallMethod<LuaRef>(GameLocator::getGame()->GetPlayer(), "GetEquip", "engine", 1));
	// Cache max range so it doesn't get recalculated every time we call GetDuration
	const float max_range = hyperdrive.CallMethod<float>("GetMaximumRange", GameLocator::getGame()->GetPlayer());

	const float dist = Sector::DistanceBetween(start_sec, start.systemIndex, target_sec, target.systemIndex);

	// nodes[0] is always start
	std::vector<SystemPath> nodes;
	nodes.push_back(start);

	const int32_t minX = std::min(start.sectorX, target.sectorX) - 2, maxX = std::max(start.sectorX, target.sectorX) + 2;
	const int32_t minY = std::min(start.sectorY, target.sectorY) - 2, maxY = std::max(start.sectorY, target.sectorY) + 2;
	const int32_t minZ = std::min(start.sectorZ, target.sectorZ) - 2, maxZ = std::max(start.sectorZ, target.sectorZ) + 2;
	const vector3f start_pos = start_sec->m_systems[start.systemIndex].GetFullPosition();
	const vector3f target_pos = target_sec->m_systems[target.systemIndex].GetFullPosition();

	// go sector by sector for the minimum cube of sectors and add systems
	// if they are within 110% of dist of both start and target
	for (int32_t sx = minX; sx <= maxX; sx++) {
		for (int32_t sy = minY; sy <= maxY; sy++) {
			for (int32_t sz = minZ; sz < maxZ; sz++) {
				const SystemPath sec_path = SystemPath(sx, sy, sz);
				RefCountedPtr<const Sector> sec = m_galaxy->GetSector(sec_path);
				for (std::vector<Sector::System>::size_type s = 0; s < sec->m_systems.size(); s++) {
					if (start.IsSameSystem(sec->m_systems[s].GetPath()))
						continue; // start is already nodes[0]

					const float lineDist = MathUtil::DistanceFromLine(start_pos, target_pos, sec->m_systems[s].GetFullPosition());

					if (Sector::DistanceBetween(start_sec, start.systemIndex, sec, sec->m_systems[s].idx) <= dist * 1.10 &&
						Sector::DistanceBetween(target_sec, target.systemIndex, sec, sec->m_systems[s].idx) <= dist * 1.10 &&
						lineDist < (Sector::SIZE * 3)) {
						nodes.push_back(sec->m_systems[s].GetPath());
					}
				}
			}
		}
	}
	Output("SectorView::AutoRoute, nodes to search = %lu\n", nodes.size());

	// setup inital values and set everything as unvisited
	std::vector<float> path_dist; // distance from source to node
	std::vector<std::vector<SystemPath>::size_type> path_prev; // previous node in optimal path
	std::unordered_set<std::vector<SystemPath>::size_type> unvisited;
	for (std::vector<SystemPath>::size_type i = 0; i < nodes.size(); i++) {
		path_dist.push_back(INFINITY);
		path_prev.push_back(0);
		unvisited.insert(i);
	}

	// distance to the start is 0
	path_dist[0] = 0.f;

	size_t totalSkipped = 0u;
	while (unvisited.size() > 0) {
		// find the closest node (for the first loop this will be start)
		std::vector<SystemPath>::size_type closest_i = *unvisited.begin();
		for (auto it : unvisited) {
			if (path_dist[it] < path_dist[closest_i])
				closest_i = it;
		}

		// mark it as visited
		unvisited.erase(closest_i);

		// if this is the target then we have found the route
		const SystemPath &closest = nodes[closest_i];
		if (closest.IsSameSystem(target))
			break;

		RefCountedPtr<const Sector> closest_sec = m_galaxy->GetSector(closest);

		// if not, loop through all unvisited nodes
		// since every system is technically reachable from every other system
		// everything is a neighbor :)
		for (auto it : unvisited) {
			const SystemPath &v = nodes[it];
			// everything is a neighbor isn't quite true as the ship has a max_range for each jump!
			if ((SystemPath::SectorDistance(closest, v) * Sector::SIZE) > max_range) {
				++totalSkipped;
				continue;
			}

			RefCountedPtr<const Sector> v_sec = m_galaxy->GetSector(v); // this causes it to generate a sector (slooooooow)

			const float v_dist_ly = Sector::DistanceBetween(closest_sec, closest.systemIndex, v_sec, v.systemIndex);

			// in this case, duration is used for the distance since that's what we are optimizing
			float v_dist = hyperdrive.CallMethod<float>("GetDuration", GameLocator::getGame()->GetPlayer(), v_dist_ly, max_range);

			v_dist += path_dist[closest_i]; // we want the total duration from start to this node
			if (v_dist < path_dist[it]) {
				// if our calculated duration is less than a previous value, this path is more efficent
				// so store/override it
				path_dist[it] = v_dist;
				path_prev[it] = closest_i;
			}
		}
	}
	Output("SectorView::AutoRoute, total times that nodes were skipped = %lu\n", totalSkipped);

	bool foundRoute = false;
	std::vector<SystemPath>::size_type u = 0;

	// find the index of our target
	for (std::vector<SystemPath>::size_type i = 0, numNodes = nodes.size(); i < numNodes; i++) {
		if (target.IsSameSystem(nodes[i])) {
			u = i;
			foundRoute = true;
			break;
		}
	}

	// It's posible that there is no valid route
	if (foundRoute) {
		outRoute.reserve(nodes.size());
		// Build the route, in reverse starting with the target
		while (u != 0) {
			outRoute.push_back(nodes[u]);
			u = path_prev[u];
		}
		std::reverse(std::begin(outRoute), std::end(outRoute));
	}
}

void SectorView::PrepareRouteLines(const vector3f &playerAbsPos, const matrix4x4f &trans)
{
	const size_t currentSize = m_secLineVerts->GetNumVerts();
	m_secLineVerts->position.reserve(currentSize + 2 * m_route.size());
	m_secLineVerts->diffuse.reserve(currentSize + 2 * m_route.size());

	for (std::vector<SystemPath>::size_type i = 0; i < m_route.size(); ++i) {
		RefCountedPtr<const Sector> jumpSec = m_galaxy->GetSector(m_route[i]);
		const Sector::System &jumpSecSys = jumpSec->m_systems[m_route[i].systemIndex];
		const vector3f jumpAbsPos = Sector::SIZE * vector3f(float(jumpSec->sx), float(jumpSec->sy), float(jumpSec->sz)) + jumpSecSys.GetPosition();

		vector3f startPos;
		if (i == 0) {
			startPos = playerAbsPos;
		} else {
			RefCountedPtr<const Sector> prevSec = m_galaxy->GetSector(m_route[i - 1]);
			const Sector::System &prevSecSys = prevSec->m_systems[m_route[i - 1].systemIndex];
			const vector3f prevAbsPos = Sector::SIZE * vector3f(float(prevSec->sx), float(prevSec->sy), float(prevSec->sz)) + prevSecSys.GetPosition();
			startPos = prevAbsPos;
		}

		m_secLineVerts->Add(trans * startPos, Color(20, 20, 0, 127));
		m_secLineVerts->Add(trans * jumpAbsPos, Color(255, 255, 0, 255));
	}
}

void SectorView::DrawNearSector(const int sx, const int sy, const int sz, const vector3f &playerAbsPos, const matrix4x4f &trans)
{
	PROFILE_SCOPED()
	RendererLocator::getRenderer()->SetTransform(trans);
	RefCountedPtr<Sector> ps = m_sectorCache->GetCached(SystemPath(sx, sy, sz));

	uint32_t sysIdx = 0;
	for (std::vector<Sector::System>::iterator i = ps->m_systems.begin(); i != ps->m_systems.end(); ++i, ++sysIdx) {
		// calculate where the system is in relation the centre of the view...
		const vector3f sysAbsPos = Sector::SIZE * vector3f(float(sx), float(sy), float(sz)) + i->GetPosition();
		const vector3f toCentreOfView = m_pos * Sector::SIZE - sysAbsPos;

		// ...and skip the system if it doesn't fall within the sphere we're viewing.
		if (toCentreOfView.Length() > OUTER_RADIUS) continue;

		const bool bIsCurrentSystem = i->IsSameSystem(m_current);

		// if the system is the current system or target we can't skip it
		bool can_skip = !i->IsSameSystem(m_selected) && !i->IsSameSystem(m_hyperspaceTarget) && !bIsCurrentSystem;

		// if the system belongs to a faction we've chosen to temporarily hide
		// then skip it if we can
		m_visibleFactions.insert(i->GetFaction());
		if (can_skip && m_hiddenFactions.find(i->GetFaction()) != m_hiddenFactions.end()) continue;

		// determine if system in hyperjump range or not
		RefCountedPtr<const Sector> playerSec = m_sectorCache->GetCached(m_current);
		float dist = Sector::DistanceBetween(ps, sysIdx, playerSec, m_current.systemIndex);
		bool inRange = dist <= m_playerHyperspaceRange;

		// don't worry about looking for inhabited systems if they're
		// unexplored (same calculation as in StarSystem.cpp) or we've
		// already retrieved their population.
		if (i->GetPopulation() < 0 && isqrt(1 + sx * sx + sy * sy + sz * sz) <= 90) {
			// only do this once we've pretty much stopped moving.
			vector3f diff = vector3f(
				fabs(m_posMovingTo.x - m_pos.x),
				fabs(m_posMovingTo.y - m_pos.y),
				fabs(m_posMovingTo.z - m_pos.z));

			// Ideally, since this takes so f'ing long, it wants to be done as a threaded job but haven't written that yet.
			if ((diff.x < 0.001f && diff.y < 0.001f && diff.z < 0.001f)) {
				SystemPath current = SystemPath(sx, sy, sz, sysIdx);
				RefCountedPtr<StarSystem> pSS = m_galaxy->GetStarSystem(current);
				i->SetPopulation(pSS->GetTotalPop());
			}
		}

		matrix4x4f systrans = trans * matrix4x4f::Translation(i->GetPosition().x, i->GetPosition().y, i->GetPosition().z);

		const int cz = int(floor(m_pos.z + 0.5f));
		// for out-of-range systems draw leg only if we draw label
		if ((m_drawVerticalLines && (inRange || m_drawOutRangeLabels) && (i->GetPopulation() > 0 || m_drawUninhabitedLabels)) || !can_skip) {
			PrepareLegs(systrans, i->GetPosition(), cz - sz);
		}

		RendererLocator::getRenderer()->SetTransform(systrans);

		if (i->IsSameSystem(m_selected)) {
			if (m_selected != m_current) {
				m_selectedLine.SetStart(vector3f(0.f, 0.f, 0.f));
				m_selectedLine.SetEnd(playerAbsPos - sysAbsPos);
				m_selectedLine.Draw(RendererLocator::getRenderer(), m_solidState);
			} else {
			}
			if (m_selected != m_hyperspaceTarget) {
				RefCountedPtr<Sector> hyperSec = m_sectorCache->GetCached(m_hyperspaceTarget);
				const vector3f hyperAbsPos =
					Sector::SIZE * vector3f(m_hyperspaceTarget.sectorX, m_hyperspaceTarget.sectorY, m_hyperspaceTarget.sectorZ) + hyperSec->m_systems[m_hyperspaceTarget.systemIndex].GetPosition();
				if (m_selected != m_current) {
					m_secondLine.SetStart(vector3f(0.f, 0.f, 0.f));
					m_secondLine.SetEnd(hyperAbsPos - sysAbsPos);
					m_secondLine.Draw(RendererLocator::getRenderer(), m_solidState);
				}

				if (m_hyperspaceTarget != m_current) {
					// FIXME: Draw when drawing hyperjump target or current system
					m_jumpLine.SetStart(hyperAbsPos - sysAbsPos);
					m_jumpLine.SetEnd(playerAbsPos - sysAbsPos);
					m_jumpLine.Draw(RendererLocator::getRenderer(), m_solidState);
				}
			} else {
			}
		}

		// draw star blob itself
		systrans.Rotate(DEG2RAD(-m_rotZ), 0, 0, 1);
		systrans.Rotate(DEG2RAD(-m_rotX), 1, 0, 0);
		systrans.Scale((GalaxyEnums::starScale[(*i).GetStarType(0)]));
		RendererLocator::getRenderer()->SetTransform(systrans);

		const uint8_t *col;
		if (m_showFactionColor) {
			col = (*i).GetFaction()->colour;
		} else {
			col = GalaxyEnums::starColors[(*i).GetStarType(0)];
		}
		AddStarBillboard(systrans, vector3f(0.f), Color(col[0], col[1], col[2], 255), 0.5f);

		// player location indicator
		if (m_inSystem && bIsCurrentSystem) {
			RendererLocator::getRenderer()->SetDepthRange(0.2, 1.0);
			m_disk->SetColor(Color(0, 0, 204));
			RendererLocator::getRenderer()->SetTransform(systrans * matrix4x4f::ScaleMatrix(3.f));
			m_disk->Draw(RendererLocator::getRenderer());
		}
		// selected indicator
		if (bIsCurrentSystem) {
			RendererLocator::getRenderer()->SetDepthRange(0.1, 1.0);
			m_disk->SetColor(Color(0, 204, 0));
			RendererLocator::getRenderer()->SetTransform(systrans * matrix4x4f::ScaleMatrix(2.f));
			m_disk->Draw(RendererLocator::getRenderer());
		}
		// hyperspace target indicator (if different from selection)
		if (i->IsSameSystem(m_hyperspaceTarget) && m_hyperspaceTarget != m_selected && (!m_inSystem || m_hyperspaceTarget != m_current)) {
			RendererLocator::getRenderer()->SetDepthRange(0.1, 1.0);
			m_disk->SetColor(Color(77, 77, 77));
			RendererLocator::getRenderer()->SetTransform(systrans * matrix4x4f::ScaleMatrix(2.f));
			m_disk->Draw(RendererLocator::getRenderer());
		}
	}
}

void SectorView::DrawFarSectors(const matrix4x4f &modelview)
{
	PROFILE_SCOPED()
	int buildRadius = ceilf((m_zoomClamped / FAR_THRESHOLD) * 3);
	if (buildRadius <= DRAW_RAD) buildRadius = DRAW_RAD;

	const vector3f secOrigin = vector3f(int(floorf(m_pos.x)), int(floorf(m_pos.y)), int(floorf(m_pos.z)));

	PrepareGrid(modelview, buildRadius + 3);

	// build vertex and colour arrays for all the stars we want to see, if we don't already have them
	if (m_rebuildFarSector || buildRadius != m_radiusFar || !secOrigin.ExactlyEqual(m_secPosFar)) {
		m_farstars.clear();
		m_farstarsColor.clear();
		m_visibleFactions.clear();

		for (int sx = secOrigin.x - buildRadius; sx <= secOrigin.x + buildRadius; sx++) {
			for (int sy = secOrigin.y - buildRadius; sy <= secOrigin.y + buildRadius; sy++) {
				for (int sz = secOrigin.z - buildRadius; sz <= secOrigin.z + buildRadius; sz++) {
					if ((vector3f(sx, sy, sz) - secOrigin).Length() <= buildRadius) {
						BuildFarSector(m_sectorCache->GetCached(SystemPath(sx, sy, sz)), Sector::SIZE * secOrigin, m_farstars, m_farstarsColor);
					}
				}
			}
		}

		m_secPosFar = secOrigin;
		m_radiusFar = buildRadius;
		m_rebuildFarSector = false;
	}

	// always draw the stars, slightly altering their size for different different resolutions, so they still look okay
	if (m_farstars.size() > 0) {
		m_farstarsPoints.SetData(RendererLocator::getRenderer(), m_farstars.size(), &m_farstars[0], &m_farstarsColor[0], modelview, 0.25f * (Graphics::GetScreenHeight() / 720.f));
		m_farstarsPoints.Draw(RendererLocator::getRenderer(), m_alphaBlendState);
	}

	m_systems = CollectHomeworlds(Sector::SIZE * secOrigin);
}

void SectorView::BuildFarSector(RefCountedPtr<Sector> sec, const vector3f &origin, std::vector<vector3f> &points, std::vector<Color> &colors)
{
	PROFILE_SCOPED()
	Color starColor;
	for (const auto sys : sec->m_systems) { //std::vector<Sector::System>::iterator i = sec->m_systems.begin(); i != sec->m_systems.end(); ++i) {
		// skip the system if it doesn't fall within the sphere we're viewing.
		if ((m_pos * Sector::SIZE - sys.GetFullPosition()).Length() > (m_zoomClamped / FAR_THRESHOLD) * OUTER_RADIUS) continue;

		if (!sys.IsExplored()) {
			points.push_back(sys.GetFullPosition() - origin);
			colors.push_back({ 100, 100, 100, 155 }); // flat gray for unexplored systems
			continue;
		}

		// if the system belongs to a faction we've chosen to hide also skip it, if it's not selectd in some way
		m_visibleFactions.insert(sys.GetFaction());
		if (m_hiddenFactions.find(sys.GetFaction()) != m_hiddenFactions.end() && !sys.IsSameSystem(m_selected) && !sys.IsSameSystem(m_hyperspaceTarget) && !sys.IsSameSystem(m_current)) continue;

		// otherwise add the system's position (origin must be m_pos's *sector* or we get judder)
		// and faction color to the list to draw
		if (m_showFactionColor) {
			starColor = sys.GetFaction()->colour;
		} else {
			starColor = GalaxyEnums::starColors[sys.GetStarType(0)];
		}
		starColor.a = 120;

		points.push_back(sys.GetFullPosition() - origin);
		colors.push_back(starColor);
	}
}

void SectorView::OnSwitchTo()
{
	RendererLocator::getRenderer()->SetViewport(0, 0, Graphics::GetScreenWidth(), Graphics::GetScreenHeight());

	m_inputFrame->SetActive(true);
	m_sectorFrame->SetActive(true);

	UIView::OnSwitchTo();

	Update(0.);
}

void SectorView::OnSwitchFrom()
{
	m_inputFrame->SetActive(false);
	m_sectorFrame->SetActive(false);
}

void SectorView::OnToggleSelectionFollowView(bool down) {
	if (down) return;
	m_automaticSystemSelection = !m_automaticSystemSelection;
}

void SectorView::OnMapLockHyperspaceToggle(bool down) {
	if (down) return;
	// space "locks" (or unlocks) the hyperspace target to the selected system
	if ((m_matchTargetToSelection || m_hyperspaceTarget != m_selected) && !m_selected.IsSameSystem(m_current)) {
		SetHyperspaceTarget(m_selected);
	} else {
		ResetHyperspaceTarget();
	}
}

void SectorView::UpdateBindings()
{
	bool reset_view = false;

	// fast move selection to current player system or hyperspace target
	const bool shifted = InputFWD::GetMoveSpeedShiftModifier() >= 1.0 ? true : false;
	if (m_sectorFrame->IsActive(m_sectorBindings.mapWarpToCurrent)) {
		GotoSystem(m_current);
		reset_view = shifted;
	} else if (m_sectorFrame->IsActive(m_sectorBindings.mapWarpToSelected)) {
		GotoSystem(m_selected);
		reset_view = shifted;
	} else if (m_sectorFrame->IsActive(m_sectorBindings.mapWarpToHyperspaceTarget)) {
		GotoSystem(m_hyperspaceTarget);
		reset_view = shifted;
	}

	// reset rotation and zoom
	if (reset_view || m_sectorFrame->IsActive(m_sectorBindings.mapViewReset)) {
		while (m_rotZ < -180.0f)
			m_rotZ += 360.0f;
		while (m_rotZ > 180.0f)
			m_rotZ -= 360.0f;
		m_rotXMovingTo = m_rotXDefault;
		m_rotZMovingTo = m_rotZDefault;
		m_zoomMovingTo = m_zoomDefault;
	}
}

void SectorView::Update(const float frameTime)
{
	PROFILE_SCOPED()

	// Cache frame time for use in ZoomIn/ZoomOut
	m_lastFrameTime = frameTime;

	SystemPath last_current = m_current;

	if (GameLocator::getGame()->IsNormalSpace()) {
		m_inSystem = true;
		m_current = GameLocator::getGame()->GetSpace()->GetStarSystem()->GetPath();
	} else {
		m_inSystem = false;
		m_current = GameLocator::getGame()->GetPlayer()->GetHyperspaceDest();
	}

	matrix4x4f rot = matrix4x4f::Identity();
	rot.RotateX(DEG2RAD(-m_rotX));
	rot.RotateZ(DEG2RAD(-m_rotZ));

	// don't check raw keypresses if the search box is active
	UpdateBindings();

	const float moveSpeed = InputFWD::GetMoveSpeedShiftModifier();
	float move = moveSpeed * frameTime;
	vector3f shift(0.0f);
	if (m_inputFrame->IsActive(m_sectorBindings.mapViewShiftLeftRight)) {
		shift.x -= m_inputFrame->GetValue(m_sectorBindings.mapViewShiftLeftRight) * move;
	}
	if (m_inputFrame->IsActive(m_sectorBindings.mapViewShiftUpDown)) {
		shift.y -= -m_inputFrame->GetValue(m_sectorBindings.mapViewShiftUpDown) * move;
	}
	if (m_inputFrame->IsActive(m_sectorBindings.mapViewShiftForwardBackward)) {
		shift.z += m_inputFrame->GetValue(m_sectorBindings.mapViewShiftForwardBackward) * move;
	}

	m_posMovingTo += shift * rot;

	if (m_inputFrame->IsActive(m_sectorBindings.mapViewZoom)) {
		m_zoomMovingTo -= m_inputFrame->GetValue(m_sectorBindings.mapViewZoom) * move * 5.0f;
	}
	m_zoomMovingTo = Clamp(m_zoomMovingTo, 0.1f, FAR_MAX);

	if (m_inputFrame->IsActive(m_sectorBindings.mapViewRotateLeftRight)) {
		if (m_inputFrame->GetValue(m_sectorBindings.mapViewRotateLeftRight) < 0.0) m_rotZMovingTo -= ROTATION_SPEED_FACTOR * moveSpeed;
		if (m_inputFrame->GetValue(m_sectorBindings.mapViewRotateLeftRight) > 0.0) m_rotZMovingTo += ROTATION_SPEED_FACTOR * moveSpeed;
	}
	if (m_inputFrame->IsActive(m_sectorBindings.mapViewRotateUpDown)) {
		if (m_inputFrame->GetValue(m_sectorBindings.mapViewRotateUpDown) > 0.0) m_rotXMovingTo -= ROTATION_SPEED_FACTOR * moveSpeed;
		if (m_inputFrame->GetValue(m_sectorBindings.mapViewRotateUpDown) < 0.0) m_rotXMovingTo += ROTATION_SPEED_FACTOR * moveSpeed;
	}

	auto motion = InputFWD::GetMouseMotion(MouseMotionBehaviour::Rotate);
	m_rotXMovingTo += ROTATION_SPEED_FACTOR * float(std::get<2>(motion));
	m_rotZMovingTo += ROTATION_SPEED_FACTOR * float(std::get<1>(motion));

	m_rotXMovingTo = Clamp(m_rotXMovingTo, -170.0f, -10.0f);

	{
		vector3f diffPos = m_posMovingTo - m_pos;
		vector3f travelPos = diffPos * 10.0f * frameTime;
		if (travelPos.Length() > diffPos.Length())
			m_pos = m_posMovingTo;
		else
			m_pos = m_pos + travelPos;

		float diffX = m_rotXMovingTo - m_rotX;
		float travelX = diffX * 10.0f * frameTime;
		if (fabs(travelX) > fabs(diffX))
			m_rotX = m_rotXMovingTo;
		else
			m_rotX = m_rotX + travelX;

		float diffZ = m_rotZMovingTo - m_rotZ;
		float travelZ = diffZ * 10.0f * frameTime;
		if (fabs(travelZ) > fabs(diffZ))
			m_rotZ = m_rotZMovingTo;
		else
			m_rotZ = m_rotZ + travelZ;

		float diffZoom = m_zoomMovingTo - m_zoom;
		float travelZoom = diffZoom * ZOOM_SPEED * frameTime;
		if (fabs(travelZoom) > fabs(diffZoom))
			m_zoom = m_zoomMovingTo;
		else
			m_zoom = m_zoom + travelZoom;
		m_zoomClamped = Clamp(m_zoom, 1.f, FAR_LIMIT);
	}

	if (m_automaticSystemSelection) {
		SystemPath new_selected = SystemPath(int(floor(m_pos.x)), int(floor(m_pos.y)), int(floor(m_pos.z)), 0);

		RefCountedPtr<Sector> ps = m_sectorCache->GetCached(new_selected);
		if (ps->m_systems.size()) {
			float px = FFRAC(m_pos.x) * Sector::SIZE;
			float py = FFRAC(m_pos.y) * Sector::SIZE;
			float pz = FFRAC(m_pos.z) * Sector::SIZE;

			float min_dist = FLT_MAX;
			for (unsigned int i = 0; i < ps->m_systems.size(); i++) {
				Sector::System *ss = &ps->m_systems[i];
				float dx = px - ss->GetPosition().x;
				float dy = py - ss->GetPosition().y;
				float dz = pz - ss->GetPosition().z;
				float dist = sqrtf(dx * dx + dy * dy + dz * dz);
				if (dist < min_dist) {
					min_dist = dist;
					new_selected.systemIndex = i;
				}
			}

			if (!m_selected.IsSameSystem(new_selected)) {
				RefCountedPtr<StarSystem> system = m_galaxy->GetStarSystem(new_selected);
				SetSelected(system->GetStars()[0]->GetPath());
			}
		}
	}

	ShrinkCache();

	m_playerHyperspaceRange = LuaObject<Player>::CallMethod<float>(GameLocator::getGame()->GetPlayer(), "GetHyperspaceRange");

	if (!m_jumpSphere) {
		Graphics::RenderStateDesc rsd;
		rsd.blendMode = Graphics::BLEND_ALPHA;
		rsd.depthTest = false;
		rsd.depthWrite = false;
		rsd.cullMode = Graphics::CULL_NONE;
		m_jumpSphereState = RendererLocator::getRenderer()->CreateRenderState(rsd);

		Graphics::MaterialDescriptor matdesc;
		matdesc.effect = Graphics::EffectType::FRESNEL_SPHERE;
		m_fresnelMat.Reset(RendererLocator::getRenderer()->CreateMaterial(matdesc));
		m_fresnelMat->diffuse = Color::WHITE;
		m_jumpSphere.reset(new Graphics::Drawables::Sphere3D(RendererLocator::getRenderer(), m_fresnelMat, m_jumpSphereState, 4, 1.0f));
	}

	UIView::Update(frameTime);
}

void SectorView::ShrinkCache()
{
	PROFILE_SCOPED()
	// we're going to use these to determine if our sectors are within the range that we'll ever render
	const int drawRadius = (m_zoomClamped <= FAR_THRESHOLD) ? DRAW_RAD : ceilf((m_zoomClamped / FAR_THRESHOLD) * DRAW_RAD);

	const int xmin = int(floorf(m_pos.x)) - drawRadius;
	const int xmax = int(floorf(m_pos.x)) + drawRadius;
	const int ymin = int(floorf(m_pos.y)) - drawRadius;
	const int ymax = int(floorf(m_pos.y)) + drawRadius;
	const int zmin = int(floorf(m_pos.z)) - drawRadius;
	const int zmax = int(floorf(m_pos.z)) + drawRadius;

	if (xmin != m_cacheXMin || xmax != m_cacheXMax || ymin != m_cacheYMin || ymax != m_cacheYMax || zmin != m_cacheZMin || zmax != m_cacheZMax) {
		SystemPath center(int(floorf(m_pos.x)), int(floorf(m_pos.y)), int(floorf(m_pos.z)));
		// TODO: Check also for systems in 'm_route':
		// HINT: build a synced sector only helper vector
		// with unique sectors and check against it
		m_sectorCache->ShrinkCache(center, drawRadius, m_current);

		m_cacheXMin = xmin;
		m_cacheXMax = xmax;
		m_cacheYMin = ymin;
		m_cacheYMax = ymax;
		m_cacheZMin = zmin;
		m_cacheZMax = zmax;
	}
}

double SectorView::GetZoomLevel() const
{
	return ((m_zoomClamped / FAR_THRESHOLD) * (OUTER_RADIUS)) + 0.5 * Sector::SIZE;
}

void SectorView::ZoomIn()
{
	const float moveSpeed = InputFWD::GetMoveSpeedShiftModifier();
	float move = moveSpeed * m_lastFrameTime;
	m_zoomMovingTo -= move;
	m_zoomMovingTo = Clamp(m_zoomMovingTo, 0.1f, FAR_MAX);
}

void SectorView::ZoomOut()
{
	const float moveSpeed = InputFWD::GetMoveSpeedShiftModifier();
	float move = moveSpeed * m_lastFrameTime;
	m_zoomMovingTo += move;
	m_zoomMovingTo = Clamp(m_zoomMovingTo, 0.1f, FAR_MAX);
}

vector3f SectorView::GetCenterSector()
{
	return m_pos;
}

double SectorView::GetCenterDistance()
{
	if (m_inSystem) {
		vector3f dv = vector3f(floorf(m_pos.x) - m_current.sectorX, floorf(m_pos.y) - m_current.sectorY, floorf(m_pos.z) - m_current.sectorZ) * Sector::SIZE;
		return dv.Length();
	} else {
		return 0.0;
	}
}

void SectorView::LockHyperspaceTarget(bool lock)
{
	if (lock) {
		SetHyperspaceTarget(GetSelected());
	} else {
		FloatHyperspaceTarget();
	}
}

std::vector<SystemPath> SectorView::GetNearbyStarSystemsByName(std::string pattern)
{
	return m_sectorCache->SearchPattern(pattern);
}

void SectorView::SetFactionVisible(const Faction *faction, bool visible)
{
	if (visible)
		m_hiddenFactions.erase(faction);
	else
		m_hiddenFactions.insert(faction);
	m_rebuildFarSector = true;
}
