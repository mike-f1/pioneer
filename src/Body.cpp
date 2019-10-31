// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Body.h"

#include "CargoBody.h"
#include "Frame.h"
#include "GameSaveError.h"
#include "HyperspaceCloud.h"
#include "LuaEvent.h"
#include "Missile.h"
#include "Planet.h"
#include "Player.h"
#include "Projectile.h"
#include "Ship.h"
#include "Space.h"
#include "SpaceStation.h"
#include "Star.h"

Body::Body() :
	PropertiedObject(Lua::manager),
	m_flags(0),
	m_interpPos(0.0),
	m_interpOrient(matrix3x3d::Identity()),
	m_pos(0.0),
	m_orient(matrix3x3d::Identity()),
	m_frame(nullptr),
	m_dead(false),
	m_clipRadius(0.0),
	m_physRadius(0.0)
{
	Properties().Set("label", m_label);
	for (int i = 0; i < Feature::MAX_FEATURE; i++)
		m_features[i] = false;
}

Body::Body(const Json &jsonObj, Space *space) :
	PropertiedObject(Lua::manager),
	m_flags(0),
	m_interpPos(0.0),
	m_interpOrient(matrix3x3d::Identity()),
	m_frame(nullptr)
{
	for (int i = 0; i < Feature::MAX_FEATURE; i++)
		m_features[i] = false;

	try {
		Json bodyObj = jsonObj["body"];

		Properties().LoadFromJson(bodyObj);
		m_frame = Frame::FindFrame(bodyObj["index_for_frame"]);
		m_label = bodyObj["label"];
		Properties().Set("label", m_label);
		m_dead = bodyObj["dead"];

		m_pos = bodyObj["pos"];
		m_orient = bodyObj["orient"];
		m_physRadius = bodyObj["phys_radius"];
		m_clipRadius = bodyObj["clip_radius"];
	} catch (Json::type_error &) {
		throw SavedGameCorruptException();
	}
}

Body::~Body()
{
}

void Body::SaveToJson(Json &jsonObj, Space *space)
{
	Json bodyObj = Json::object(); // Create JSON object to contain body data.

	Properties().SaveToJson(bodyObj);
	bodyObj["index_for_frame"] = (m_frame != nullptr ? m_frame->GetId() : noFrameId);
	bodyObj["label"] = m_label;
	bodyObj["dead"] = m_dead;

	bodyObj["pos"] = m_pos;
	bodyObj["orient"] = m_orient;
	bodyObj["phys_radius"] = m_physRadius;
	bodyObj["clip_radius"] = m_clipRadius;

	jsonObj["body"] = bodyObj; // Add body object to supplied object.
}

void Body::ToJson(Json &jsonObj, Space *space)
{
	jsonObj["body_type"] = int(GetType());

	switch (GetType()) {
	case Object::STAR:
	case Object::PLANET:
	case Object::SPACESTATION:
	case Object::SHIP:
	case Object::PLAYER:
	case Object::MISSILE:
	case Object::CARGOBODY:
	case Object::PROJECTILE:
	case Object::HYPERSPACECLOUD:
		SaveToJson(jsonObj, space);
		break;
	default:
		assert(0);
	}
}

Body *Body::FromJson(const Json &jsonObj, Space *space)
{
	if (!jsonObj["body_type"].is_number_integer())
		throw SavedGameCorruptException();

	Object::Type type = Object::Type(jsonObj["body_type"]);
	switch (type) {
	case Object::STAR:
		return new Star(jsonObj, space);
	case Object::PLANET:
		return new Planet(jsonObj, space);
	case Object::SPACESTATION:
		return new SpaceStation(jsonObj, space);
	case Object::SHIP: {
		Ship *s = new Ship(jsonObj, space);
		// Here because of comments in Ship.cpp on following function
		s->UpdateLuaStats();
		return static_cast<Body *>(s);
	}
	case Object::PLAYER: {
		Player *p = new Player(jsonObj, space);
		// Read comments in Ship.cpp on following function
		p->UpdateLuaStats();
		return static_cast<Body *>(p);
	}
	case Object::MISSILE:
		return new Missile(jsonObj, space);
	case Object::PROJECTILE:
		return new Projectile(jsonObj, space);
	case Object::CARGOBODY:
		return new CargoBody(jsonObj, space);
	case Object::HYPERSPACECLOUD:
		return new HyperspaceCloud(jsonObj, space);
	default:
		assert(0);
	}

	return nullptr;
}

vector3d Body::GetPositionRelTo(const Frame *relTo) const
{
	vector3d fpos = m_frame->GetPositionRelTo(relTo);
	matrix3x3d forient = m_frame->GetOrientRelTo(relTo);
	return forient * GetPosition() + fpos;
}

vector3d Body::GetInterpPositionRelTo(const Frame *relTo) const
{
	vector3d fpos = m_frame->GetInterpPositionRelTo(relTo);
	matrix3x3d forient = m_frame->GetInterpOrientRelTo(relTo);
	return forient * GetInterpPosition() + fpos;
}

vector3d Body::GetPositionRelTo(const Body *relTo) const
{
	return GetPositionRelTo(relTo->m_frame) - relTo->GetPosition();
}

vector3d Body::GetInterpPositionRelTo(const Body *relTo) const
{
	return GetInterpPositionRelTo(relTo->m_frame) - relTo->GetInterpPosition();
}

matrix3x3d Body::GetOrientRelTo(const Frame *relTo) const
{
	matrix3x3d forient = m_frame->GetOrientRelTo(relTo);
	return forient * GetOrient();
}

matrix3x3d Body::GetInterpOrientRelTo(const Frame *relTo) const
{
	matrix3x3d forient = m_frame->GetInterpOrientRelTo(relTo);
	return forient * GetInterpOrient();
}

vector3d Body::GetVelocityRelTo(const Frame *relTo) const
{
	matrix3x3d forient = m_frame->GetOrientRelTo(relTo);
	vector3d vel = GetVelocity();
	if (m_frame != relTo) vel -= m_frame->GetStasisVelocity(GetPosition());
	return forient * vel + m_frame->GetVelocityRelTo(relTo);
}

vector3d Body::GetVelocityRelTo(const Body *relTo) const
{
	return GetVelocityRelTo(relTo->m_frame) - relTo->GetVelocityRelTo(relTo->m_frame);
}

void Body::OrientOnSurface(double radius, double latitude, double longitude)
{
	vector3d up = vector3d(cos(latitude) * cos(longitude), sin(latitude) * cos(longitude), sin(longitude));
	SetPosition(radius * up);

	vector3d right = up.Cross(vector3d(0, 0, 1)).Normalized();
	SetOrient(matrix3x3d::FromVectors(right, up));
}

void Body::SwitchToFrame(Frame *newFrame)
{
	const vector3d vel = GetVelocityRelTo(newFrame); // do this first because it uses position
	const vector3d fpos = m_frame->GetPositionRelTo(newFrame);
	const matrix3x3d forient = m_frame->GetOrientRelTo(newFrame);
	SetPosition(forient * GetPosition() + fpos);
	SetOrient(forient * GetOrient());
	SetVelocity(vel + newFrame->GetStasisVelocity(GetPosition()));
	SetFrame(newFrame);

	LuaEvent::Queue("onFrameChanged", this);
}

void Body::UpdateFrame()
{
	if (!(m_flags & FLAG_CAN_MOVE_FRAME)) return;

	// falling out of frames
	if (m_frame->GetRadius() < GetPosition().Length()) {
		Frame *newFrame = GetFrame()->GetParent();
		if (newFrame) { // don't fall out of root frame
			Output("%s leaves frame %s\n", GetLabel().c_str(), GetFrame()->GetLabel().c_str());
			SwitchToFrame(newFrame);
			return;
		}
	}

	// entering into frames
	for (Frame *kid : m_frame->GetChildren()) {
		const vector3d pos = GetPositionRelTo(kid);
		if (pos.Length() >= kid->GetRadius()) continue;
		SwitchToFrame(kid);
		Output("%s enters frame %s\n", GetLabel().c_str(), kid->GetLabel().c_str());
		break;
	}
}

vector3d Body::GetTargetIndicatorPosition(const Frame *relTo) const
{
	return GetInterpPositionRelTo(relTo);
}

void Body::SetLabel(const std::string &label)
{
	m_label = label;
	Properties().Set("label", label);
}
