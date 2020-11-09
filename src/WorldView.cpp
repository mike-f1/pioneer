// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt
#include "buildopts.h"

#include "WorldView.h"

#include "Frame.h"
#include "Game.h"
#include "GameConfSingleton.h"
#include "GameConfig.h"
#include "GameLocator.h"
#include "GameSaveError.h"
#include "HudTrail.h"
#include "HyperspaceCloud.h"
#include "InGameViews.h"
#include "InGameViewsLocator.h"
#include "InputFrame.h"
#include "InputFwd.h"
#include "KeyBindings.h"
#include "Lang.h"
#include "Pi.h"
#include "Player.h"
#include "SectorView.h"
#include "Sensors.h"
#include "SpeedLines.h"
#include "graphics/Frustum.h"
#include "graphics/Graphics.h"
#include "graphics/Material.h"
#include "graphics/Renderer.h"
#include "graphics/RenderState.h"
#include "graphics/RendererLocator.h"
#include "graphics/VertexArray.h"
#include "graphics/VertexBuffer.h"
#include "libs/StringF.h"
#include "libs/matrix4x4.h"
#include "ship/PlayerShipController.h"
#include "sound/Sound.h"

#include <imgui/imgui.h>

#ifdef WITH_DEVKEYS
#include <sstream>
#include "galaxy/SystemBody.h"
#endif // WITH_DEVKEYS

namespace {
	static const Color s_hudTextColor(0, 255, 0, 230);
	static const float HUD_CROSSHAIR_SIZE = 8.0f;
	static const Color white(255, 255, 255, 204);
	static const Color green(0, 255, 0, 204);
	static const Color yellow(230, 230, 77, 255);
	static const Color red(255, 0, 0, 128);
} // namespace

const int screen_w = 800, screen_h = 600;

WorldView::WorldView(Game *game) :
	UIView(),
	shipView(this)
{
	InitObject(game);
}

WorldView::WorldView(const Json &jsonObj, Game *game) :
	UIView(),
	shipView(this)
{
	if (!jsonObj["world_view"].is_object()) {
		Output("Loading error in '%s' in function '%s' \n", __FILE__, __func__);
		throw SavedGameCorruptException();
	}
	Json worldViewObj = jsonObj["world_view"];

	if (!worldViewObj["cam_type"].is_number_integer()) {
		Output("Loading error in '%s' in function '%s' \n", __FILE__, __func__);
		throw SavedGameCorruptException();
	}
	shipView.m_camType = worldViewObj["cam_type"];

	InitObject(game);

	shipView.LoadFromJson(worldViewObj);
}

void WorldView::InitObject(Game *game)
{
	float size[2];
	GetSizeRequested(size);

	m_labelsOn = true;
	m_guiOn = true;
	SetTransparency(true);

	Graphics::RenderStateDesc rsd;
	rsd.blendMode = Graphics::BLEND_ALPHA;
	rsd.depthWrite = false;
	rsd.depthTest = false;
	m_blendState = RendererLocator::getRenderer()->CreateRenderState(rsd); //XXX m_renderer not set yet
	m_navTunnel = std::make_unique<NavTunnelWidget>(this, m_blendState);

	m_speedLines = std::make_unique<SpeedLines>(game->GetPlayer());

	float znear;
	float zfar;
	RendererLocator::getRenderer()->GetNearFarRange(znear, zfar);

	const float fovY = GameConfSingleton::getInstance().Float("FOVVertical");

	m_cameraContext.Reset(new CameraContext(Graphics::GetScreenWidth(), Graphics::GetScreenHeight(), fovY, znear, zfar));
	m_camera.reset(new Camera(m_cameraContext));
	shipView.Init(game->GetPlayer());

	RegisterInputBindings();

	m_onPlayerChangeTargetCon = game->GetPlayer()->onPlayerChangeTarget.connect(sigc::mem_fun(this, &WorldView::OnPlayerChangeTarget));
}

void WorldView::RegisterInputBindings()
{
	using namespace KeyBindings;
	using namespace std::placeholders;

	m_inputFrame = std::make_unique<InputFrame>("WorldView");

	BindingPage &page = m_inputFrame->GetBindingPage("General");
	BindingGroup &group = page.GetBindingGroup("Miscellaneous");

	m_wviewBindings.toggleHudMode = m_inputFrame->AddActionBinding("BindToggleHudMode", group, ActionBinding(SDLK_TAB));
	m_wviewBindings.toggleHudMode->StoreOnActionCallback(std::bind(&WorldView::OnToggleLabels, this, _1));
	m_wviewBindings.increaseTimeAcceleration = m_inputFrame->AddActionBinding("BindIncreaseTimeAcceleration", group, ActionBinding(SDLK_PAGEUP));
	m_wviewBindings.increaseTimeAcceleration->StoreOnActionCallback(std::bind(&WorldView::OnRequestTimeAccelInc, this, _1));
	m_wviewBindings.decreaseTimeAcceleration = m_inputFrame->AddActionBinding("BindDecreaseTimeAcceleration", group, ActionBinding(SDLK_PAGEDOWN));
	m_wviewBindings.decreaseTimeAcceleration->StoreOnActionCallback(std::bind(&WorldView::OnRequestTimeAccelDec, this, _1));
}

WorldView::~WorldView()
{
	m_onPlayerChangeTargetCon.disconnect();
}

void WorldView::SaveToJson(Json &jsonObj)
{
	Json worldViewObj = Json::object(); // Create JSON object to contain world view data.

	shipView.SaveToJson(worldViewObj);

	jsonObj["world_view"] = worldViewObj; // Add world view object to supplied object.
}

void WorldView::OnRequestTimeAccelInc(bool down)
{
	if (down) return;
	// requests an increase in time acceleration
	GameLocator::getGame()->RequestTimeAccelInc();
}

void WorldView::OnRequestTimeAccelDec(bool down)
{
	if (down) return;
	// requests a decrease in time acceleration
	GameLocator::getGame()->RequestTimeAccelDec();
}

void WorldView::OnToggleLabels(bool down)
{
	if (down) return;
	if (InGameViewsLocator::getInGameViews()->IsWorldView()) {
		if (m_guiOn && m_labelsOn) {
			m_labelsOn = false;
		} else if (m_guiOn && !m_labelsOn) {
			m_guiOn = false;
		} else if (!m_guiOn) {
			m_guiOn = true;
			m_labelsOn = true;
		}
	}
}

void WorldView::ShowAll()
{
	View::ShowAll(); // by default, just delegate back to View
}

void WorldView::Update(const float frameTime)
{
	PROFILE_SCOPED()
	assert(GameLocator::getGame());
	assert(GameLocator::getGame()->GetPlayer());
	assert(!GameLocator::getGame()->GetPlayer()->IsDead());

	shipView.Update(frameTime);

	m_cameraContext->BeginFrame();
	m_camera->Update();

	UpdateProjectedObjects();

	FrameId playerFrameId = GameLocator::getGame()->GetPlayer()->GetFrame();
	FrameId camFrameId = m_cameraContext->GetCamFrame();

	//speedlines and contact trails need camFrame for transform, so they
	//must be updated here
	if (GameConfSingleton::AreSpeedLinesDisplayed()) {
		m_speedLines->Update(GameLocator::getGame()->GetTimeStep());

		matrix4x4d trans = Frame::GetFrameTransform(playerFrameId, camFrameId);

		if (m_speedLines.get() && GameConfSingleton::AreSpeedLinesDisplayed()) {
			m_speedLines->Update(GameLocator::getGame()->GetTimeStep());

			trans[12] = trans[13] = trans[14] = 0.0;
			trans[15] = 1.0;
			m_speedLines->SetTransform(trans);
		}
	}

	if (GameConfSingleton::AreHudTrailsDisplayed()) {
		matrix4x4d trans = Frame::GetFrameTransform(playerFrameId, camFrameId);

		for (auto &item : GameLocator::getGame()->GetPlayer()->GetSensors()->GetContacts())
			item.trail->SetTransform(trans);
	} else {
		for (auto &item : GameLocator::getGame()->GetPlayer()->GetSensors()->GetContacts())
			item.trail->Reset(playerFrameId);
	}

	UIView::Update(frameTime);
}

void WorldView::Draw3D()
{
	PROFILE_SCOPED()
	assert(GameLocator::getGame());
	assert(GameLocator::getGame()->GetPlayer());
	assert(!GameLocator::getGame()->GetPlayer()->IsDead());

	m_cameraContext->ApplyDrawTransforms();

	Body *excludeBody = nullptr;
	ShipCockpit *cockpit = nullptr;
	if (shipView.GetCamType() == ShipViewController::CAM_INTERNAL) {
		excludeBody = GameLocator::getGame()->GetPlayer();
		if (shipView.m_internalCameraController->GetMode() == InternalCameraController::MODE_FRONT)
			cockpit = GameLocator::getGame()->GetPlayer()->GetCockpit();
	}
	m_camera->Draw(excludeBody, cockpit);

	// Speed lines
	if (GameConfSingleton::AreSpeedLinesDisplayed())
		m_speedLines->Render();

	// Contact trails
	if (GameConfSingleton::AreHudTrailsDisplayed()) {
		for (auto &contact : GameLocator::getGame()->GetPlayer()->GetSensors()->GetContacts())
			contact.trail->Render();
	}

	m_cameraContext->EndFrame();

	UIView::Draw3D();
}

void WorldView::Draw()
{
	assert(GameLocator::getGame());
	assert(GameLocator::getGame()->GetPlayer());

	RendererLocator::getRenderer()->ClearDepthBuffer();

	View::Draw();

	if (GameConfSingleton::IsNavTunnelDisplayed())
		m_navTunnel->Draw();

	// don't draw crosshairs etc in hyperspace
	if (GameLocator::getGame()->GetPlayer()->GetFlightState() == Ship::HYPERSPACE) return;

	// combat target indicator
	DrawCombatTargetIndicator(m_combatTargetIndicator, m_targetLeadIndicator, red);

	// glLineWidth(1.0f);
	RendererLocator::getRenderer()->CheckRenderErrors(__FUNCTION__, __LINE__);
}

void WorldView::DrawUI(const float frameTime)
{
	if (Pi::IsConsoleActive()) return;

	if (!GameLocator::getGame()->IsPaused()) return;
	int32_t viewport[4];
	RendererLocator::getRenderer()->GetCurrentViewport(&viewport[0]);
	ImVec2 pos(0.5, 0.85);
	pos.x = pos.x * viewport[2] + viewport[0];
	pos.y = pos.y * viewport[3] + viewport[1];
	pos.y = RendererLocator::getRenderer()->GetWindowHeight() - pos.y;

	ImGuiStyle& style = ImGui::GetStyle();
	ImGui::SetNextWindowBgAlpha(0.7f);
	ImGui::Begin("pause", nullptr, ImGuiWindowFlags_NoTitleBar
				| ImGuiWindowFlags_NoResize
				| ImGuiWindowFlags_NoMove
				| ImGuiWindowFlags_NoScrollbar
				| ImGuiWindowFlags_NoCollapse
				| ImGuiWindowFlags_NoSavedSettings
				| ImGuiWindowFlags_NoFocusOnAppearing
				| ImGuiWindowFlags_NoBringToFrontOnFocus
				);
	std::string label = Lang::PAUSED;
	ImVec2 size = ImGui::CalcTextSize(label.c_str());
	size.x += style.WindowPadding.x * 2;
	size.y += style.WindowPadding.y * 2;

	pos.x -= size.x / 2.0; // ...and add something to make it depend on zoom
	pos.y -= size.y / 2.0;
	ImGui::SetWindowPos(pos);
	ImGui::SetWindowSize(size);
	ImVec4 color(1.0f, 0.5f, 0.5f, 1.0);
	ImGui::PushStyleColor(ImGuiCol_Text, color);
	ImGui::TextUnformatted(label.c_str());
	ImGui::PopStyleColor(1);
	ImGui::End();
}

void WorldView::BuildUI(UI::Single *container)
{
}

void WorldView::OnSwitchTo()
{
	UIView::OnSwitchTo();
	shipView.Activated();

	m_inputFrame->SetActive(true);
}

void WorldView::OnSwitchFrom()
{
	shipView.Deactivated();
	m_guiOn = true;

	m_inputFrame->SetActive(false);
}

// XXX paying fine remotely can't really be done until crime and
// worldview are in Lua. I'm leaving this code here so its not
// forgotten
/*
static void PlayerPayFine()
{
	int64_t crime, fine;
	Polit::GetCrime(&crime, &fine);
	if (GameLocator::getGame()->GetPlayer()->GetMoney() == 0) {
		GameLocator::getGame()->log->Add(Lang::YOU_NO_MONEY);
	} else if (fine > GameLocator::getGame()->GetPlayer()->GetMoney()) {
		Polit::AddCrime(0, -GameLocator::getGame()->GetPlayer()->GetMoney());
		Polit::GetCrime(&crime, &fine);
		GameLocator::getGame()->log->Add(stringf(
			Lang::FINE_PAID_N_BUT_N_REMAINING,
				formatarg("paid", format_money(GameLocator::getGame()->GetPlayer()->GetMoney())),
				formatarg("fine", format_money(fine))));
		GameLocator::getGame()->GetPlayer()->SetMoney(0);
	} else {
		GameLocator::getGame()->GetPlayer()->SetMoney(GameLocator::getGame()->GetPlayer()->GetMoney() - fine);
		GameLocator::getGame()->log->Add(stringf(Lang::FINE_PAID_N,
				formatarg("fine", format_money(fine))));
		Polit::AddCrime(0, -fine);
	}
}
*/

void WorldView::OnPlayerChangeTarget()
{
	Body *b = GameLocator::getGame()->GetPlayer()->GetNavTarget();
	if (b) {
		Sound::PlaySfx("OK");
		Ship *s = b->IsType(Object::HYPERSPACECLOUD) ? static_cast<HyperspaceCloud *>(b)->GetShip() : 0;
		if (!s || !InGameViewsLocator::getInGameViews()->GetSectorView()->GetHyperspaceTarget().IsSameSystem(s->GetHyperspaceDest()))
			InGameViewsLocator::getInGameViews()->GetSectorView()->FloatHyperspaceTarget();
	}
}

int WorldView::GetActiveWeapon() const
{
	using CamType = ShipViewController::CamType;
	switch (shipView.GetCamType()) {
	case CamType::CAM_INTERNAL:
		return shipView.m_internalCameraController->GetMode() == InternalCameraController::MODE_REAR ? 1 : 0;

	case CamType::CAM_EXTERNAL:
	case CamType::CAM_SIDEREAL:
	case CamType::CAM_FLYBY:
	default:
		return 0;
	}
}

static inline bool project_to_screen(const vector3d &in, vector3d &out, const Graphics::Frustum &frustum, const int guiSize[2])
{
	if (!frustum.ProjectPoint(in, out)) return false;
	out.x *= guiSize[0];
	out.y = screen_h - out.y * guiSize[1];
	return true;
}

void WorldView::UpdateProjectedObjects()
{
	const Frame *cam_frame = Frame::GetFrame(m_cameraContext->GetCamFrame());
	matrix3x3d cam_rot = cam_frame->GetOrient();

	// later we might want non-ship enemies (e.g., for assaults on military bases)
	assert(!GameLocator::getGame()->GetPlayer()->GetCombatTarget() || GameLocator::getGame()->GetPlayer()->GetCombatTarget()->IsType(Object::SHIP));

	// update combat HUD
	Ship *enemy = static_cast<Ship *>(GameLocator::getGame()->GetPlayer()->GetCombatTarget());
	if (enemy) {
		const vector3d targpos = enemy->GetInterpPositionRelTo(GameLocator::getGame()->GetPlayer()) * cam_rot;
		const vector3d targScreenPos = enemy->GetInterpPositionRelTo(cam_frame->GetId());

		UpdateIndicator(m_combatTargetIndicator, targScreenPos);

		// calculate firing solution and relative velocity along our z axis
		int laser = -1;
		double projspeed = 0;
		if (shipView.GetCamType() == ShipViewController::CAM_INTERNAL) {
			switch (shipView.m_internalCameraController->GetMode()) {
			case InternalCameraController::MODE_FRONT: laser = 0; break;
			case InternalCameraController::MODE_REAR: laser = 1; break;
			default: break;
			}
		}

		for (unsigned i = 0; i < GameLocator::getGame()->GetPlayer()->GetMountedGunsNum(); i++) {
			// Pick speed of first gun
			if (GameLocator::getGame()->GetPlayer()->GetActivationStateOfGun(i) == false) continue;
			if (laser == 0 && GameLocator::getGame()->GetPlayer()->IsFront(i) == GunDir::GUN_FRONT) {
				projspeed = GameLocator::getGame()->GetPlayer()->GetProjSpeed(i);
				break;
			} else if (laser == 1 && GameLocator::getGame()->GetPlayer()->IsFront(i) == GunDir::GUN_REAR) {
				projspeed = GameLocator::getGame()->GetPlayer()->GetProjSpeed(i);
				break;
			}
		}
		if (projspeed > 0) { // only display target lead position on views with lasers
			const vector3d targvel = enemy->GetVelocityRelTo(GameLocator::getGame()->GetPlayer()) * cam_rot;
			vector3d leadpos = targpos + targvel * (targpos.Length() / projspeed);
			leadpos = targpos + targvel * (leadpos.Length() / projspeed); // second order approx

			UpdateIndicator(m_targetLeadIndicator, leadpos);

			if ((m_targetLeadIndicator.side != INDICATOR_ONSCREEN) || (m_combatTargetIndicator.side != INDICATOR_ONSCREEN))
				HideIndicator(m_targetLeadIndicator);

			// if the lead indicator is very close to the position indicator
			// try (just a little) to keep the labels from interfering with one another
			if (m_targetLeadIndicator.side == INDICATOR_ONSCREEN) {
				assert(m_combatTargetIndicator.side == INDICATOR_ONSCREEN);
			}
		} else
			HideIndicator(m_targetLeadIndicator);
	} else {
		HideIndicator(m_combatTargetIndicator);
		HideIndicator(m_targetLeadIndicator);
	}
}

void WorldView::UpdateIndicator(Indicator &indicator, const vector3d &cameraSpacePos)
{
	const int guiSize[2] = { screen_w, screen_h };
	const Graphics::Frustum &frustum = m_cameraContext->GetFrustum();

	const float BORDER = 10.0;
	const float BORDER_BOTTOM = 90.0;
	// XXX BORDER_BOTTOM is 10+the control panel height and shouldn't be needed at all

	if (cameraSpacePos.LengthSqr() < 1e-6) { // length < 1e-3
		indicator.pos.x = screen_w / 2.0f;
		indicator.pos.y = screen_h / 2.0f;
		indicator.side = INDICATOR_ONSCREEN;
	} else {
		vector3d proj;
		bool success = project_to_screen(cameraSpacePos, proj, frustum, guiSize);
		if (!success)
			proj = vector3d(screen_w / 2.0, screen_h / 2.0, 0.0);

		indicator.realpos.x = int(proj.x);
		indicator.realpos.y = int(proj.y);

		bool onscreen =
			(cameraSpacePos.z < 0.0) &&
			(proj.x >= BORDER) && (proj.x < screen_w - BORDER) &&
			(proj.y >= BORDER) && (proj.y < screen_h - BORDER_BOTTOM);

		if (onscreen) {
			indicator.pos.x = int(proj.x);
			indicator.pos.y = int(proj.y);
			indicator.side = INDICATOR_ONSCREEN;
		} else {
			// homogeneous 2D points and lines are really useful
			const vector3d ptCentre(screen_w / 2.0, screen_h / 2.0, 1.0);
			const vector3d ptProj(proj.x, proj.y, 1.0);
			const vector3d lnDir = ptProj.Cross(ptCentre);

			indicator.side = INDICATOR_TOP;

			// this fallback is used if direction is close to (0, 0, +ve)
			indicator.pos.x = screen_w / 2.0;
			indicator.pos.y = BORDER;

			if (cameraSpacePos.x < -1e-3) {
				vector3d ptLeft = lnDir.Cross(vector3d(-1.0, 0.0, BORDER));
				ptLeft /= ptLeft.z;
				if (ptLeft.y >= BORDER && ptLeft.y < screen_h - BORDER_BOTTOM) {
					indicator.pos.x = ptLeft.x;
					indicator.pos.y = ptLeft.y;
					indicator.side = INDICATOR_LEFT;
				}
			} else if (cameraSpacePos.x > 1e-3) {
				vector3d ptRight = lnDir.Cross(vector3d(-1.0, 0.0, screen_w - BORDER));
				ptRight /= ptRight.z;
				if (ptRight.y >= BORDER && ptRight.y < screen_h - BORDER_BOTTOM) {
					indicator.pos.x = ptRight.x;
					indicator.pos.y = ptRight.y;
					indicator.side = INDICATOR_RIGHT;
				}
			}

			if (cameraSpacePos.y < -1e-3) {
				vector3d ptBottom = lnDir.Cross(vector3d(0.0, -1.0, screen_h - BORDER_BOTTOM));
				ptBottom /= ptBottom.z;
				if (ptBottom.x >= BORDER && ptBottom.x < screen_w - BORDER) {
					indicator.pos.x = ptBottom.x;
					indicator.pos.y = ptBottom.y;
					indicator.side = INDICATOR_BOTTOM;
				}
			} else if (cameraSpacePos.y > 1e-3) {
				vector3d ptTop = lnDir.Cross(vector3d(0.0, -1.0, BORDER));
				ptTop /= ptTop.z;
				if (ptTop.x >= BORDER && ptTop.x < screen_w - BORDER) {
					indicator.pos.x = ptTop.x;
					indicator.pos.y = ptTop.y;
					indicator.side = INDICATOR_TOP;
				}
			}
		}
	}

	// update the label position
	if (indicator.side != INDICATOR_HIDDEN) {
		float labelSize[2] = { 500.0f, 500.0f };

		int pos[2] = { 0, 0 };
		switch (indicator.side) {
		case INDICATOR_HIDDEN: break;
		case INDICATOR_ONSCREEN: // when onscreen, default to label-below unless it would clamp to be on top of the marker
			pos[0] = -(labelSize[0] / 2.0f);
			if (indicator.pos.y + pos[1] + labelSize[1] + HUD_CROSSHAIR_SIZE + 2.0f > screen_h - BORDER_BOTTOM)
				pos[1] = -(labelSize[1] + HUD_CROSSHAIR_SIZE + 2.0f);
			else
				pos[1] = HUD_CROSSHAIR_SIZE + 2.0f;
			break;
		case INDICATOR_TOP:
			pos[0] = -(labelSize[0] / 2.0f);
			pos[1] = HUD_CROSSHAIR_SIZE + 2.0f;
			break;
		case INDICATOR_LEFT:
			pos[0] = HUD_CROSSHAIR_SIZE + 2.0f;
			pos[1] = -(labelSize[1] / 2.0f);
			break;
		case INDICATOR_RIGHT:
			pos[0] = -(labelSize[0] + HUD_CROSSHAIR_SIZE + 2.0f);
			pos[1] = -(labelSize[1] / 2.0f);
			break;
		case INDICATOR_BOTTOM:
			pos[0] = -(labelSize[0] / 2.0f);
			pos[1] = -(labelSize[1] + HUD_CROSSHAIR_SIZE + 2.0f);
			break;
		}

		pos[0] = Clamp(pos[0] + indicator.pos.x, BORDER, screen_w - BORDER - labelSize[0]);
		pos[1] = Clamp(pos[1] + indicator.pos.y, BORDER, screen_h - BORDER_BOTTOM - labelSize[1]);
	}
}

void WorldView::HideIndicator(Indicator &indicator)
{
	indicator.side = INDICATOR_HIDDEN;
	indicator.pos = vector2f(0.0f, 0.0f);
}

void WorldView::DrawCombatTargetIndicator(const Indicator &target, const Indicator &lead, const Color &c)
{
	if (target.side == INDICATOR_HIDDEN) return;

	if (target.side == INDICATOR_ONSCREEN) {
		const float x1 = target.pos.x, y1 = target.pos.y;
		const float x2 = lead.pos.x, y2 = lead.pos.y;

		float xd = x2 - x1, yd = y2 - y1;
		if (lead.side != INDICATOR_ONSCREEN) {
			xd = 1.0f;
			yd = 0.0f;
		} else {
			float len = xd * xd + yd * yd;
			if (len < 1e-6) {
				xd = 1.0f;
				yd = 0.0f;
			} else {
				len = sqrt(len);
				xd /= len;
				yd /= len;
			}
		}

		const vector3f vts[] = {
			// target crosshairs
			vector3f(x1 + 10 * xd, y1 + 10 * yd, 0.0f),
			vector3f(x1 + 20 * xd, y1 + 20 * yd, 0.0f),
			vector3f(x1 - 10 * xd, y1 - 10 * yd, 0.0f),
			vector3f(x1 - 20 * xd, y1 - 20 * yd, 0.0f),
			vector3f(x1 - 10 * yd, y1 + 10 * xd, 0.0f),
			vector3f(x1 - 20 * yd, y1 + 20 * xd, 0.0f),
			vector3f(x1 + 10 * yd, y1 - 10 * xd, 0.0f),
			vector3f(x1 + 20 * yd, y1 - 20 * xd, 0.0f),

			// lead crosshairs
			vector3f(x2 - 10 * xd, y2 - 10 * yd, 0.0f),
			vector3f(x2 + 10 * xd, y2 + 10 * yd, 0.0f),
			vector3f(x2 - 10 * yd, y2 + 10 * xd, 0.0f),
			vector3f(x2 + 10 * yd, y2 - 10 * xd, 0.0f),

			// line between crosshairs
			vector3f(x1 + 20 * xd, y1 + 20 * yd, 0.0f),
			vector3f(x2 - 10 * xd, y2 - 10 * yd, 0.0f)
		};
		if (lead.side == INDICATOR_ONSCREEN) {
			m_indicator.SetData(14, vts, c);
		} else {
			m_indicator.SetData(8, vts, c);
		}
		m_indicator.Draw(RendererLocator::getRenderer(), m_blendState);
	} else {
		DrawEdgeMarker(target, c);
	}
}

void WorldView::DrawEdgeMarker(const Indicator &marker, const Color &c)
{
	const vector2f screenCentre(screen_w / 2.0f, screen_h / 2.0f);
	vector2f dir = screenCentre - marker.pos;
	float len = dir.Length();
	dir *= HUD_CROSSHAIR_SIZE / len;
	m_edgeMarker.SetColor(c);
	m_edgeMarker.SetStart(vector3f(marker.pos, 0.0f));
	m_edgeMarker.SetEnd(vector3f(marker.pos + dir, 0.0f));
	m_edgeMarker.Draw(RendererLocator::getRenderer(), m_blendState);
}

// project vector vec onto plane (normal must be normalized)
static vector3d projectVecOntoPlane(const vector3d &vec, const vector3d &normal)
{
	return (vec - vec.Dot(normal) * normal);
}

static double wrapAngleToPositive(const double theta)
{
	return (theta >= 0.0 ? theta : M_PI * 2 + theta);
}

vector3d WorldView::CalculateHeadingPitchRoll(PlaneType pt)
{
	FrameId frameId = GameLocator::getGame()->GetPlayer()->GetFrame();

	if (pt == PlaneType::ROTATIONAL)
		frameId = Frame::GetFrame(frameId)->GetRotFrame();
	else if (pt == PlaneType::PARENT)
		frameId = Frame::GetFrame(frameId)->GetNonRotFrame();

	// construct a frame of reference aligned with the ground plane
	// and with lines of longitude and latitude
	const vector3d up = GameLocator::getGame()->GetPlayer()->GetPositionRelTo(frameId).NormalizedSafe();
	const vector3d north = projectVecOntoPlane(vector3d(0, 1, 0), up).NormalizedSafe();
	const vector3d east = north.Cross(up);

	// find the direction that the ship is facing
	const auto shpRot = GameLocator::getGame()->GetPlayer()->GetOrientRelTo(frameId);
	const vector3d hed = -shpRot.VectorZ();
	const vector3d right = shpRot.VectorX();
	const vector3d groundHed = projectVecOntoPlane(hed, up).NormalizedSafe();

	const double pitch = asin(up.Dot(hed));

	const double hedNorth = groundHed.Dot(north);
	const double hedEast = groundHed.Dot(east);
	const double heading = wrapAngleToPositive(atan2(hedEast, hedNorth));
	const double roll = (acos(right.Dot(up.Cross(hed).Normalized())) - M_PI) * (right.Dot(up) >= 0 ? -1 : 1);

	return vector3d(
		std::isnan(heading) ? 0.0 : heading,
		std::isnan(pitch) ? 0.0 : pitch,
		std::isnan(roll) ? 0.0 : roll);
}

static vector3d projectToScreenSpace(const vector3d &pos, RefCountedPtr<CameraContext> cameraContext, const bool adjustZ = true)
{
	const Graphics::Frustum &frustum = cameraContext->GetFrustum();
	const float h = Graphics::GetScreenHeight();
	const float w = Graphics::GetScreenWidth();
	vector3d proj;
	if (!frustum.ProjectPoint(pos, proj)) {
		return vector3d(w / 2, h / 2, 0);
	}
	proj.x *= w;
	proj.y = h - proj.y * h;
	// set z to -1 if in front of camera, 1 else
	if (adjustZ)
		proj.z = pos.z < 0 ? -1 : 1;
	return proj;
}

// needs to run inside m_cameraContext->Begin/EndFrame();
vector3d WorldView::WorldSpaceToScreenSpace(const Body *body) const
{
	if (body->IsType(Object::PLAYER) && shipView.GetCamType() == ShipViewController::CAM_INTERNAL)
		return vector3d(0, 0, 0);
	const FrameId cam_frame = m_cameraContext->GetCamFrame();
	vector3d pos = body->GetInterpPositionRelTo(cam_frame);
	return projectToScreenSpace(pos, m_cameraContext);
}

// needs to run inside m_cameraContext->Begin/EndFrame();
vector3d WorldView::WorldSpaceToScreenSpace(const vector3d &position) const
{
	const Frame *cam_frame = Frame::GetFrame(m_cameraContext->GetCamFrame());
	matrix3x3d cam_rot = cam_frame->GetInterpOrient();
	vector3d pos = position * cam_rot;
	return projectToScreenSpace(pos, m_cameraContext);
}

// needs to run inside m_cameraContext->Begin/EndFrame();
vector3d WorldView::ShipSpaceToScreenSpace(const vector3d &pos) const
{
	matrix3x3d orient = GameLocator::getGame()->GetPlayer()->GetInterpOrient();
	const Frame *cam_frame = Frame::GetFrame(m_cameraContext->GetCamFrame());
	matrix3x3d cam_rot = cam_frame->GetInterpOrient();
	vector3d camspace = orient * pos * cam_rot;
	return projectToScreenSpace(camspace, m_cameraContext, false);
}

// needs to run inside m_cameraContext->Begin/EndFrame();
vector3d WorldView::CameraSpaceToScreenSpace(const vector3d &pos) const
{
	return projectToScreenSpace(pos, m_cameraContext);
}

// needs to run inside m_cameraContext->Begin/EndFrame();
vector3d WorldView::GetTargetIndicatorScreenPosition(const Body *body) const
{
	if (body->IsType(Object::PLAYER) && shipView.GetCamType() == ShipViewController::CAM_INTERNAL)
		return vector3d(0, 0, 0);
	FrameId cam_frame = m_cameraContext->GetCamFrame();
	vector3d pos = body->GetTargetIndicatorPosition(cam_frame);
	return projectToScreenSpace(pos, m_cameraContext);
}

// needs to run inside m_cameraContext->Begin/EndFrame();
vector3d WorldView::GetMouseDirection() const
{
	// orientation according to mouse
	const Frame *cam_frame = Frame::GetFrame(m_cameraContext->GetCamFrame());
	matrix3x3d cam_rot = cam_frame->GetInterpOrient();
	vector3d mouseDir = GameLocator::getGame()->GetPlayer()->GetPlayerController()->GetMouseDir() * cam_rot;
	if (shipView.GetCamType() == ShipViewController::CAM_INTERNAL && shipView.m_internalCameraController->GetMode() == InternalCameraController::MODE_REAR)
		mouseDir = -mouseDir;
	return (GameLocator::getGame()->GetPlayer()->GetPhysRadius() * 1.5) * mouseDir;
}

NavTunnelWidget::NavTunnelWidget(WorldView *worldview, Graphics::RenderState *rs) :
	m_worldView(worldview),
	m_renderState(rs)
{
	CreateVertexBuffer(8);
}

static double getSquareDistance(double initialDist, double scalingFactor, int num)
{
	return pow(scalingFactor, num - 1) * num * initialDist;
}

static double getSquareHeight(double distance, double angle)
{
	return distance * tan(angle);
}

void NavTunnelWidget::Draw()
{
	Body *navtarget = GameLocator::getGame()->GetPlayer()->GetNavTarget();
	if (navtarget) {
		const vector3d navpos = navtarget->GetPositionRelTo(GameLocator::getGame()->GetPlayer());
		const matrix3x3d &rotmat = GameLocator::getGame()->GetPlayer()->GetOrient();
		const vector3d eyevec = rotmat * m_worldView->shipView.GetCameraController()->GetOrient().VectorZ();
		if (eyevec.Dot(navpos) >= 0.0) {
			return;
		}
		const double distToDest = GameLocator::getGame()->GetPlayer()->GetPositionRelTo(navtarget).Length();

		const int maxSquareHeight = std::max(screen_w, screen_h) / 2;
		const double angle = atan(maxSquareHeight / distToDest);
		// ECRAVEN: TODO not the ideal way to handle Begin/EndCameraFrame here :-/
		m_worldView->BeginCameraFrame();
		const vector3d nav_screen = m_worldView->WorldSpaceToScreenSpace(navtarget);
		m_worldView->EndCameraFrame();
		const vector2f tpos(vector2f(nav_screen.x / Graphics::GetScreenWidth() * screen_w, nav_screen.y / Graphics::GetScreenHeight() * screen_h));
		const vector2f distDiff(tpos - vector2f(screen_w / 2.0f, screen_h / 2.0f));

		double dist = 0.0;
		const double scalingFactor = 1.6; // scales distance between squares: closer to 1.0, more squares
		for (int squareNum = 1;; squareNum++) {
			dist = getSquareDistance(10.0, scalingFactor, squareNum);
			if (dist > distToDest)
				break;

			const double sqh = getSquareHeight(dist, angle);
			if (sqh >= 10) {
				const vector2f off = distDiff * (dist / distToDest);
				const vector2f sqpos(tpos - off);
				DrawTargetGuideSquare(sqpos, sqh, green);
			}
		}
	}
}

void NavTunnelWidget::DrawTargetGuideSquare(const vector2f &pos, const float size, const Color &c)
{
	const float x1 = pos.x - size;
	const float x2 = pos.x + size;
	const float y1 = pos.y - size;
	const float y2 = pos.y + size;

	Color black(c);
	black.a = c.a / 6;
	Graphics::VertexArray va(Graphics::ATTRIB_POSITION | Graphics::ATTRIB_DIFFUSE, 8);
	va.Add(vector3f(x1, y1, 0.f), c);
	va.Add(vector3f(pos.x, y1, 0.f), black);
	va.Add(vector3f(x2, y1, 0.f), c);
	va.Add(vector3f(x2, pos.y, 0.f), black);
	va.Add(vector3f(x2, y2, 0.f), c);
	va.Add(vector3f(pos.x, y2, 0.f), black);
	va.Add(vector3f(x1, y2, 0.f), c);
	va.Add(vector3f(x1, pos.y, 0.f), black);

	m_vbuffer->Populate(va);

	RendererLocator::getRenderer()->DrawBuffer(m_vbuffer.get(), m_renderState, m_material.Get(), Graphics::LINE_LOOP);
}

void NavTunnelWidget::CreateVertexBuffer(const uint32_t size)
{
	Graphics::Renderer *r = RendererLocator::getRenderer();

	Graphics::MaterialDescriptor desc;
	desc.vertexColors = true;
	m_material.Reset(r->CreateMaterial(desc));

	Graphics::VertexBufferDesc vbd;
	vbd.attrib[0].semantic = Graphics::ATTRIB_POSITION;
	vbd.attrib[0].format = Graphics::ATTRIB_FORMAT_FLOAT3;
	vbd.attrib[1].semantic = Graphics::ATTRIB_DIFFUSE;
	vbd.attrib[1].format = Graphics::ATTRIB_FORMAT_UBYTE4;
	vbd.usage = Graphics::BUFFER_USAGE_DYNAMIC;
	vbd.numVertices = size;
	m_vbuffer.reset(r->CreateVertexBuffer(vbd));
}
