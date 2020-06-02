// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "ShipCpanelMultiFuncDisplays.h"

#include "Game.h"
#include "GameLocator.h"
#include "GameSaveError.h"
#include "KeyBindings.h"
#include "Lang.h"
#include "Input.h"
#include "Missile.h"
#include "Pi.h"
#include "Player.h"
#include "Space.h"
#include "graphics/Graphics.h"
#include "graphics/Renderer.h"
#include "graphics/RendererLocator.h"
#include "graphics/RenderState.h"
#include "graphics/VertexArray.h"

using namespace Graphics;

static const float RADAR_RANGE_MAX = 100000.0f;
static const float RADAR_RANGE_MIN = 1000.0f;
static const float RADAR_SCALE = 0.00001f;
//static const float RADAR_YSHRINK   = 0.95f;
//static const float RADAR_XSHRINK   = 4.0f;
static const float A_BIT = 1.1f;
static const unsigned int RADAR_STEPS = 100;

// XXX target colours should be unified throughout the game
static const Color radarNavTargetColour = Color(0, 255, 0);
static const Color radarCombatTargetColour = Color(255, 0, 0);
static const Color radarStationColour = Color(255, 255, 255);
static const Color radarShipColour = Color(243, 237, 29);
static const Color radarMissileColour = Color(240, 38, 50);
static const Color radarPlayerMissileColour = Color(243, 237, 29);
static const Color radarCargoColour = Color(166, 166, 166);
static const Color radarCloudColour = Color(128, 128, 255);

RadarWidget::RadarWidget()
{
	m_mode = RadarMode::MODE_AUTO;
	m_currentRange = m_manualRange = m_targetRange = RADAR_RANGE_MIN;

	InitObject();
}

RadarWidget::RadarWidget(const Json &jsonObj)
{
	try {
		// Radar used to be called "scanner" for Frontier-reasons
		Json radarObj = jsonObj["scanner"];

		m_mode = radarObj["mode"];
		m_currentRange = radarObj["current_range"];
		m_manualRange = radarObj["manual_range"];
		m_targetRange = radarObj["target_range"];
	} catch (Json::type_error &) {
		Output("Loading error in '%s' in function '%s' \n", __FILE__, __func__);
		throw SavedGameCorruptException();
	}

	InitObject();
}

void RadarWidget::InitObject()
{
	InitScaling();

	m_lastRange = RADAR_RANGE_MAX * 100.0f; // force regen

	GenerateBaseGeometry();

	Graphics::RenderStateDesc rsd;
	rsd.blendMode = Graphics::BLEND_ALPHA;
	rsd.depthWrite = false;
	rsd.depthTest = false;
	rsd.cullMode = CULL_NONE;
	m_renderState = RendererLocator::getRenderer()->CreateRenderState(rsd);

	GenerateRingsAndSpokes();
	RegisterInputBindings();
}

void RadarWidget::RegisterInputBindings()
{
	using namespace KeyBindings;
	using namespace std::placeholders;

	m_inputFrame = std::make_unique<InputFrame>("RadarWidget");

	BindingPage &page = Pi::input->GetBindingPage("RadarView");
	BindingGroup &group = page.GetBindingGroup("Miscellaneous");

	m_radarWidgetBindings.toggleScanMode = m_inputFrame->AddActionBinding("BindToggleScanMode", group, ActionBinding(SDLK_SLASH));
	m_radarWidgetBindings.toggleScanMode->StoreOnActionCallback(std::bind(&RadarWidget::ToggleMode, this, _1));

	m_radarWidgetBindings.changeScanRange = m_inputFrame->AddAxisBinding("BindChangeScanRange", group, AxisBinding(SDLK_RIGHTBRACKET, SDLK_LEFTBRACKET));

	// Explicitly activate InputFrame as RadarWidget is always active (for now...)
	// TODO: this is not true as the radar can be sold, thus its InputFrame should be
	// deactivated... It should be changed when IngameView will grow to something better
	m_inputFrame->SetActive(true);
}

RadarWidget::~RadarWidget()
{
}

void RadarWidget::GetSizeRequested(float size[2])
{
	size[0] = 400;
	size[1] = 62;
}

void RadarWidget::ToggleMode(bool down)
{
	if (down) return;
	if (IsVisible() && GameLocator::getGame()->GetTimeAccel() != Game::TIMEACCEL_PAUSED) {
		if (m_mode == RadarMode::MODE_AUTO) {
			m_mode = RadarMode::MODE_MANUAL;
		} else {
			m_mode = RadarMode::MODE_AUTO;
		}
	}
}

void RadarWidget::Draw()
{
	int radar_cap = 0;
	GameLocator::getGame()->GetPlayer()->Properties().Get("radar_cap", radar_cap);
	if (radar_cap <= 0) return;

	if (GameLocator::getGame()->GetPlayer()->GetFlightState() == Ship::HYPERSPACE)
		return;

	float size[2];
	GetSize(size);
	m_x = size[0] / (RADAR_XSHRINK * 2);
	m_y = size[1] * 0.5f;

	SetScissor(true);

	float rangediff = fabs(m_lastRange - m_currentRange);
	if (rangediff > 200.0 || rangediff / m_currentRange > 0.01) {
		GenerateRingsAndSpokes();
		m_lastRange = m_currentRange;
	}

	// draw objects below player (and below radar)
	if (!m_contacts.empty()) DrawBlobs(true);

	// disc
	Color green(0, 255, 0, 26);

	// XXX 2d vertices
	VertexArray va(ATTRIB_POSITION | ATTRIB_DIFFUSE, 128); //reserve some space for positions & colors
	va.Add(vector3f(RADAR_XSHRINK * m_x, m_y, 0.f), green);
	for (float a = 0; a < 2 * float(M_PI); a += float(M_PI) * 0.02f) {
		va.Add(vector3f(RADAR_XSHRINK * m_x + m_x * sin(a), m_y + RADAR_YSHRINK * m_y * cos(a), 0.f), green);
	}
	va.Add(vector3f(RADAR_XSHRINK * m_x, m_y + RADAR_YSHRINK * m_y, 0.f), green);
	RendererLocator::getRenderer()->DrawTriangles(&va, m_renderState, Graphics::vtxColorMaterial, TRIANGLE_FAN);

	// circles and spokes
	{
		Graphics::Renderer::MatrixTicket ticket(RendererLocator::getRenderer(), Graphics::MatrixMode::MODELVIEW);
		RendererLocator::getRenderer()->Translate(RADAR_XSHRINK * m_x, m_y, 0);
		RendererLocator::getRenderer()->Scale(m_x, m_y, 1.0f);
		DrawRingsAndSpokes(false);
	}

	// objects above
	if (!m_contacts.empty()) DrawBlobs(false);

	// glLineWidth(1.f);

	SetScissor(false);
}

void RadarWidget::InitScaling(void)
{
	RADAR_XSHRINK = 4.0f;
	RADAR_YSHRINK = 0.95f;
}

void RadarWidget::Update()
{
	m_contacts.clear();

	int radar_cap = 0;
	GameLocator::getGame()->GetPlayer()->Properties().Get("radar_cap", radar_cap);
	if (radar_cap <= 0) {
		m_mode = RadarMode::MODE_AUTO;
		m_currentRange = m_manualRange = m_targetRange = RADAR_RANGE_MIN;
		return;
	}

	// range priority is combat target > ship/missile > nav target > other
	enum { RANGE_MAX,
		RANGE_FAR_OTHER,
		RANGE_NAV,
		RANGE_FAR_SHIP,
		RANGE_COMBAT } range_type = RANGE_MAX;
	float combat_dist = 0, far_ship_dist = 0, nav_dist = 0, far_other_dist = 0;

	// collect the bodies to be displayed, and if AUTO, distances
	Space::BodyNearList nearby = GameLocator::getGame()->GetSpace()->GetBodiesMaybeNear(GameLocator::getGame()->GetPlayer(), RADAR_RANGE_MAX);
	for (Body *body : nearby) {
		if (body == GameLocator::getGame()->GetPlayer()) continue;

		float dist = float(body->GetPositionRelTo(GameLocator::getGame()->GetPlayer()).Length());

		Contact c;
		c.type = body->GetType();
		c.pos = body->GetPositionRelTo(GameLocator::getGame()->GetPlayer());
		c.isSpecial = false;

		switch (body->GetType()) {

		case Object::MISSILE:
			// player's own missiles are ignored for range calc but still shown
			if (static_cast<const Missile *>(body)->GetOwner() == GameLocator::getGame()->GetPlayer()) {
				c.isSpecial = true;
				break;
			}

			// else fall through

		case Object::SHIP: {
			const Ship *s = static_cast<const Ship *>(body);
			if (s->GetFlightState() != Ship::FLYING && s->GetFlightState() != Ship::LANDED)
				continue;

			if ((body) == GameLocator::getGame()->GetPlayer()->GetCombatTarget()) c.isSpecial = true;

			if (m_mode == RadarMode::MODE_AUTO && range_type != RANGE_COMBAT) {
				if (c.isSpecial == true) {
					combat_dist = dist;
					range_type = RANGE_COMBAT;
				} else if (dist > far_ship_dist) {
					far_ship_dist = dist;
					range_type = RANGE_FAR_SHIP;
				}
			}
			break;
		}

		case Object::SPACESTATION:
		case Object::CARGOBODY:
		case Object::HYPERSPACECLOUD:

			if ((body) == GameLocator::getGame()->GetPlayer()->GetNavTarget()) c.isSpecial = true;

			if (m_mode == RadarMode::MODE_AUTO && range_type < RANGE_NAV) {
				if (c.isSpecial == true) {
					nav_dist = dist;
					range_type = RANGE_NAV;
				} else if (dist > far_other_dist) {
					far_other_dist = dist;
					range_type = RANGE_FAR_OTHER;
				}
			}
			break;

		default:
			continue;
		}

		m_contacts.push_back(c);
	}

	if (m_radarWidgetBindings.changeScanRange->IsActive()) {
		if (m_radarWidgetBindings.changeScanRange->GetValue() > 0.0) {
			if (m_mode == RadarMode::MODE_AUTO) {
				m_manualRange = m_targetRange;
				m_mode = RadarMode::MODE_MANUAL;
			} else {
				m_manualRange = m_currentRange;
			}
			m_manualRange = Clamp(m_manualRange * 1.15f, RADAR_RANGE_MIN, RADAR_RANGE_MAX);
		} else if (m_radarWidgetBindings.changeScanRange->GetValue() < 0.0) {
			if (m_mode == RadarMode::MODE_AUTO) {
				m_manualRange = m_targetRange;
				m_mode = RadarMode::MODE_MANUAL;
			} else {
				m_manualRange = m_currentRange;
			}
			m_manualRange = Clamp(m_manualRange * 0.85f, RADAR_RANGE_MIN, RADAR_RANGE_MAX);
		}
	}

	if (m_mode == RadarMode::MODE_AUTO) {
		switch (range_type) {
		case RANGE_COMBAT:
			m_targetRange = Clamp(combat_dist * A_BIT, RADAR_RANGE_MIN, RADAR_RANGE_MAX);
			break;
		case RANGE_FAR_SHIP:
			m_targetRange = Clamp(far_ship_dist * A_BIT, RADAR_RANGE_MIN, RADAR_RANGE_MAX);
			break;
		case RANGE_NAV:
			m_targetRange = Clamp(nav_dist * A_BIT, RADAR_RANGE_MIN, RADAR_RANGE_MAX);
			break;
		case RANGE_FAR_OTHER:
			m_targetRange = Clamp(far_other_dist * A_BIT, RADAR_RANGE_MIN, RADAR_RANGE_MAX);
			break;
		default:
			m_targetRange = RADAR_RANGE_MAX;
			break;
		}
	}

	else
		m_targetRange = m_manualRange;
}

void RadarWidget::DrawBlobs(bool below)
{
	assert(!m_contacts.empty());

	static const uint32_t MAX_CONTACTS(100);
	std::vector<vector3f> blobs;
	std::vector<vector3f> vts;
	std::vector<Color> blobcolors;
	std::vector<Color> colors;
	blobs.reserve(MAX_CONTACTS);
	vts.reserve(MAX_CONTACTS);
	blobcolors.reserve(MAX_CONTACTS);
	colors.reserve(MAX_CONTACTS);

	for (std::list<Contact>::iterator i = m_contacts.begin(); i != m_contacts.end(); ++i) {

		const Color *color = 0;

		switch (i->type) {
		case Object::SHIP:
			if (i->isSpecial)
				color = &radarCombatTargetColour;
			else
				color = &radarShipColour;
			break;

		case Object::MISSILE:
			if (i->isSpecial)
				color = &radarPlayerMissileColour;
			else
				color = &radarMissileColour;
			break;

		case Object::SPACESTATION:
			if (i->isSpecial)
				color = &radarNavTargetColour;
			else
				color = &radarStationColour;
			break;

		case Object::CARGOBODY:
			if (i->isSpecial)
				color = &radarNavTargetColour;
			else
				color = &radarCargoColour;
			break;

		case Object::HYPERSPACECLOUD:
			if (i->isSpecial)
				color = &radarNavTargetColour;
			else
				color = &radarCloudColour;
			break;

		default:
			continue;
		}

		vector3d pos = i->pos * GameLocator::getGame()->GetPlayer()->GetOrient();
		if ((pos.y > 0) && (below)) continue;
		if ((pos.y < 0) && (!below)) continue;

		const float x = RADAR_XSHRINK * m_x + m_x * float(pos.x) * m_scale;
		// x radar widget bound check
		if (x < RADAR_XSHRINK * m_x - m_x) continue;
		if (x > RADAR_XSHRINK * m_x + m_x) continue;

		const float y_base = m_y + m_y * RADAR_YSHRINK * float(pos.z) * m_scale;
		const float y_blob = y_base - m_y * RADAR_YSHRINK * float(pos.y) * m_scale;

		// store this stalk
		vts.push_back(vector3f(x, y_base, 0.f));
		vts.push_back(vector3f(x, y_blob, 0.f));
		colors.push_back(*color);
		colors.push_back(*color);

		// blob!
		blobs.push_back(vector3f(x, y_blob, 0.f));
		blobcolors.push_back(*color);
	}

	if (!vts.empty()) {
		m_contactLines.SetData(vts.size(), &vts[0], &colors[0]);
		m_contactLines.Draw(RendererLocator::getRenderer(), m_renderState);

		m_contactBlobs.SetData(RendererLocator::getRenderer(), blobs.size(), &blobs[0], &blobcolors[0], matrix4x4f::Identity(), 3.f);
		m_contactBlobs.Draw(RendererLocator::getRenderer(), m_renderState);
	}
}

void RadarWidget::GenerateBaseGeometry()
{
	static const float circle = float(2 * M_PI);
	static const float step = circle / RADAR_STEPS;

	// circle (to be scaled and offset)
	m_circle.clear();
	m_circle.push_back(vector3f(0.0f, RADAR_YSHRINK, 0.0f));
	float a = step;
	for (unsigned int i = 1; i < RADAR_STEPS; i++, a += step) {
		vector3f v = vector3f(sin(a), RADAR_YSHRINK * cos(a), 0.0f);
		m_circle.push_back(v);
		m_circle.push_back(v);
	}
	m_circle.push_back(vector3f(0.0f, RADAR_YSHRINK, 0.0f));

	// spokes
	m_spokes.clear();
	for (float ang = 0; ang < circle; ang += float(M_PI * 0.25)) {
		m_spokes.push_back(vector3f(0.1f * sin(ang), 0.1f * RADAR_YSHRINK * cos(ang), 0.0f));
		m_spokes.push_back(vector3f(sin(ang), RADAR_YSHRINK * cos(ang), 0.0f));
	}
}

void RadarWidget::GenerateRingsAndSpokes()
{
	const int csize = m_circle.size();
	const int ssize = m_spokes.size();
	m_vts.clear();

	// inner circle
	for (int i = 0; i < csize; i++)
		m_vts.push_back(m_circle[i] * 0.1f);

	// dynamic circles
	for (int p = 0; p < 7; ++p) {
		float sz = (pow(2.0f, p) * 1000.0f) / m_currentRange;
		if (sz <= 0.1f) continue;
		if (sz >= 1.0f) break;
		for (int i = 0; i < csize; i++)
			m_vts.push_back(m_circle[i] * sz);
	}

	// spokes
	for (int i = 0; i < ssize; i++)
		m_vts.push_back(m_spokes[i]);

	// outer ring
	m_edgeVts.clear();
	m_edgeCols.clear();
	int dimstart = 2 * int(RADAR_STEPS * m_currentRange / RADAR_RANGE_MAX);
	float a = 2.0f * M_PI * m_currentRange / RADAR_RANGE_MAX;
	vector3f vn(sin(a), RADAR_YSHRINK * cos(a), 0.0f);

	// bright part
	Color col = (m_mode == RadarMode::MODE_AUTO) ? Color(0, 178, 0, 128) : Color(178, 178, 0, 128);
	for (int i = 0; i <= dimstart; i++) {
		if (i == csize) break; // whole circle bright case
		m_edgeVts.push_back(vector3f(m_circle[i].x, m_circle[i].y, 0.0f));
		m_edgeCols.push_back(col);
	}
	m_edgeVts.push_back(vn);
	m_edgeCols.push_back(col);

	// dim part
	col = Color(51, 77, 51, 128);
	m_edgeVts.push_back(vn);
	m_edgeCols.push_back(col);
	for (int i = dimstart + 1; i < csize; i++) {
		m_edgeVts.push_back(vector3f(m_circle[i].x, m_circle[i].y, 0.0f));
		m_edgeCols.push_back(col);
	}

	static const Color vtscol(0, 102, 0, 128);
	m_scanLines.SetData(m_vts.size(), &m_vts[0], vtscol);
	m_edgeLines.SetData(m_edgeVts.size(), &m_edgeVts[0], &m_edgeCols[0]);
}

void RadarWidget::DrawRingsAndSpokes(bool blend)
{
	m_scanLines.Draw(RendererLocator::getRenderer(), m_renderState);
	m_edgeLines.Draw(RendererLocator::getRenderer(), m_renderState);
}

void RadarWidget::TimeStepUpdate(float step)
{
	PROFILE_SCOPED()
	if (m_targetRange < m_currentRange)
		m_currentRange = Clamp(m_currentRange - (m_currentRange * step), m_targetRange, RADAR_RANGE_MAX);
	else if (m_targetRange > m_currentRange)
		m_currentRange = Clamp(m_currentRange + (m_currentRange * step), RADAR_RANGE_MIN, m_targetRange);

	m_scale = RADAR_SCALE * (RADAR_RANGE_MAX / m_currentRange);
}

void RadarWidget::SaveToJson(Json &jsonObj)
{
	Json radarObj({}); // Create JSON object to contain radar data.

	radarObj["mode"] = m_mode;
	radarObj["current_range"] = m_currentRange;
	radarObj["manual_range"] = m_manualRange;
	radarObj["target_range"] = m_targetRange;

	// Radar used to be called "scanner".
	jsonObj["scanner"] = radarObj; // Add radar object to supplied object.
}
