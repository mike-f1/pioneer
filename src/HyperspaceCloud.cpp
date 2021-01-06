// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "HyperspaceCloud.h"

#include "Game.h"
#include "GameLocator.h"
#include "GameSaveError.h"
#include "Json.h"
#include "Lang.h"
#include "Player.h"
#include "Ship.h"
#include "Space.h"
#include "graphics/Material.h"
#include "graphics/RenderState.h"
#include "graphics/Renderer.h"
#include "graphics/RendererLocator.h"
#include "graphics/VertexArray.h"
#include "pi_states/PiState.h"
#include "perlin.h"

using namespace Graphics;

/** How long does a hyperspace cloud last for? 2 Days? */
constexpr float HYPERCLOUD_DURATION = (60.0 * 60.0 * 24.0 * 2.0);

HyperspaceCloud::HyperspaceCloud(Ship *s, double dueDate, bool isArrival) :
	m_isBeingKilled(false)
{
	m_flags = Body::FLAG_CAN_MOVE_FRAME |
		Body::FLAG_LABEL_HIDDEN;
	m_ship = s;
	SetPhysRadius(0.0);
	SetClipRadius(1200.0);
	m_vel = (s ? s->GetVelocity() : vector3d(0.0));
	m_birthdate = GameLocator::getGame()->GetTime();
	m_due = dueDate;
	SetIsArrival(isArrival);
	InitGraphics();
}

HyperspaceCloud::HyperspaceCloud(const Json &jsonObj, Space *space) :
	Body(jsonObj, space),
	m_isBeingKilled(false)
{
	try {
		Json hyperspaceCloudObj = jsonObj["hyperspace_cloud"];

		m_vel = hyperspaceCloudObj["vel"];
		m_birthdate = hyperspaceCloudObj["birth_date"];
		m_due = hyperspaceCloudObj["due"];
		m_isArrival = hyperspaceCloudObj["is_arrival"];

		m_ship = nullptr;
		if (hyperspaceCloudObj["ship"].is_object()) {
			Json shipObj = hyperspaceCloudObj["ship"];
			m_ship = new Ship(shipObj, space);
		}
		InitGraphics();
	} catch (Json::type_error &) {
		Output("Loading error in '%s' in function '%s' \n", __FILE__, __func__);
		throw SavedGameCorruptException();
	}
}

HyperspaceCloud::~HyperspaceCloud()
{
	if (m_ship) delete m_ship;
}

void HyperspaceCloud::SetIsArrival(bool isArrival)
{
	m_isArrival = isArrival;
	SetLabel(isArrival ? Lang::HYPERSPACE_ARRIVAL_CLOUD : Lang::HYPERSPACE_DEPARTURE_CLOUD);
}

Json HyperspaceCloud::SaveToJson(Space *space)
{
	Json jsonObj = Body::SaveToJson(space);

	Json hyperspaceCloudObj;

	hyperspaceCloudObj["vel"] = m_vel;
	hyperspaceCloudObj["birth_date"] = m_birthdate;
	hyperspaceCloudObj["due"] = m_due;
	hyperspaceCloudObj["is_arrival"] = m_isArrival;
	if (m_ship) {
		hyperspaceCloudObj["ship"] = m_ship->SaveToJson(space); // Add ship object to hyperpace cloud object.
	}

	jsonObj["hyperspace_cloud"] = hyperspaceCloudObj; // Add hyperspace cloud object to supplied object.
	return jsonObj;
}

void HyperspaceCloud::PostLoadFixup(Space *space)
{
	Body::PostLoadFixup(space);
	if (m_ship) m_ship->PostLoadFixup(space);
}

void HyperspaceCloud::TimeStepUpdate(const float timeStep)
{
	if (m_isBeingKilled)
		return;

	SetPosition(GetPosition() + m_vel * timeStep);

	if (m_isArrival && m_ship && (m_due < GameLocator::getGame()->GetTime())) {
		// spawn ship
		// XXX some overlap with Space::DoHyperspaceTo(). should probably all
		// be moved into EvictShip()
		m_ship->SetPosition(GetPosition());
		m_ship->SetVelocity(m_vel);
		m_ship->SetOrient(matrix3x3d::Identity());
		m_ship->SetFrame(GetFrame());
		GameLocator::getGame()->GetSpace()->AddBody(m_ship);

		if (GameLocator::getGame()->GetPlayer()->GetNavTarget() == this &&
			!GameLocator::getGame()->GetPlayer()->GetCombatTarget()) {
			GameLocator::getGame()->GetPlayer()->SetCombatTarget(m_ship,
				GameLocator::getGame()->GetPlayer()->GetSetSpeedTarget() == this);
		}

		m_ship->EnterSystem();

		m_ship = nullptr;
	}

	// cloud expiration
	if (m_birthdate + HYPERCLOUD_DURATION <= GameLocator::getGame()->GetTime()) {
		GameLocator::getGame()->RemoveHyperspaceCloud(this);
		GameLocator::getGame()->GetSpace()->KillBody(this);
		m_isBeingKilled = true;
	}
}

Ship *HyperspaceCloud::EvictShip()
{
	Ship *s = m_ship;
	m_ship = nullptr;
	return s;
}

static void make_circle_thing(VertexArray &va, float radius, const Color &colCenter, const Color &colEdge)
{
	va.Add(vector3f(0.f, 0.f, 0.f), colCenter);
	for (float ang = 0; ang < float(M_PI) * 2.f; ang += 0.1f) {
		va.Add(vector3f(radius * sin(ang), radius * cos(ang), 0.0f), colEdge);
	}
	va.Add(vector3f(0.f, radius, 0.f), colEdge);
}

void HyperspaceCloud::UpdateInterpTransform(double alpha)
{
	m_interpOrient = matrix3x3d::Identity();
	const vector3d oldPos = GetPosition() - m_vel * GameLocator::getGame()->GetTimeStep();
	m_interpPos = alpha * GetPosition() + (1.0 - alpha) * oldPos;
}

void HyperspaceCloud::Render(const Camera *camera, const vector3d &viewCoords, const matrix4x4d &viewTransform)
{
	if (m_isBeingKilled)
		return;

	matrix4x4d trans = matrix4x4d::Identity();
	trans.Translate(float(viewCoords.x), float(viewCoords.y), float(viewCoords.z));

	// face the camera dammit
	vector3d zaxis = viewCoords.NormalizedSafe();
	vector3d xaxis = vector3d(0, 1, 0).Cross(zaxis).Normalized();
	vector3d yaxis = zaxis.Cross(xaxis);
	matrix4x4d rot = matrix4x4d::MakeRotMatrix(xaxis, yaxis, zaxis).Inverse();
	RendererLocator::getRenderer()->SetTransform(trans * rot);

	// precise to the rendered frame (better than PHYSICS_HZ granularity)
	const double preciseTime = GameLocator::getGame()->GetTime() + MainState_::PiState::GetGameTickAlpha() * GameLocator::getGame()->GetTimeStep();

	// Flickering gradient circle, departure clouds are red and arrival clouds blue
	// XXX could just alter the scale instead of recreating the model
	const float radius = 1000.0f + 200.0f * float(noise(vector3d(10.0 * preciseTime, 0, 0)));
	m_graphic.vertices->Clear();
	Color outerColor = m_isArrival ? Color::BLUE : Color::RED;
	outerColor.a = 0;
	make_circle_thing(*m_graphic.vertices.get(), radius, Color::WHITE, outerColor);
	RendererLocator::getRenderer()->DrawTriangles(m_graphic.vertices.get(), m_graphic.renderState, m_graphic.material.get(), TRIANGLE_FAN);
}

void HyperspaceCloud::InitGraphics()
{
	m_graphic.vertices.reset(new Graphics::VertexArray(ATTRIB_POSITION | ATTRIB_DIFFUSE));

	Graphics::MaterialDescriptor desc;
	desc.vertexColors = true;
	m_graphic.material.reset(RendererLocator::getRenderer()->CreateMaterial(desc));

	Graphics::RenderStateDesc rsd;
	rsd.blendMode = BLEND_ALPHA_ONE;
	rsd.depthWrite = false;
	m_graphic.renderState = RendererLocator::getRenderer()->CreateRenderState(rsd);
}
