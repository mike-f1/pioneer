// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Body.h"

#include "Frame.h"
#include "GameSaveError.h"
#include "JsonUtils.h"
#include "LuaEvent.h"

Body::Body() :
	PropertiedObject(Lua::manager),
	m_flags(0),
	m_interpPos(0.0),
	m_interpOrient(matrix3x3d::Identity()),
	m_pos(0.0),
	m_orient(matrix3x3d::Identity()),
	m_frame(FrameId::Invalid),
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
	m_frame(FrameId::Invalid)
{
	for (int i = 0; i < Feature::MAX_FEATURE; i++)
		m_features[i] = false;

	try {
		Json bodyObj = jsonObj["body"];

		Properties().LoadFromJson(bodyObj);
		m_frame = bodyObj["index_for_frame"];
		m_label = bodyObj["label"];
		Properties().Set("label", m_label);
		m_dead = bodyObj["dead"];

		m_pos = bodyObj["pos"];
		m_orient = bodyObj["orient"];
		m_physRadius = bodyObj["phys_radius"];
		m_clipRadius = bodyObj["clip_radius"];
	} catch (Json::type_error &) {
		Output("Loading error in '%s' in function '%s' \n", __FILE__, __func__);
		throw SavedGameCorruptException();
	}
}

Body::~Body()
{
}

Json Body::SaveToJson(Space *space)
{
	Json bodyObj = Json::object(); // Create JSON object to contain body data.

	Properties().SaveToJson(bodyObj);
	bodyObj["index_for_frame"] = m_frame.id();
	bodyObj["label"] = m_label;
	bodyObj["dead"] = m_dead;

	bodyObj["pos"] = m_pos;
	bodyObj["orient"] = m_orient;
	bodyObj["phys_radius"] = m_physRadius;
	bodyObj["clip_radius"] = m_clipRadius;

	Json jsonObj;
	jsonObj["body"] = bodyObj;
	return jsonObj; // Add body object to supplied object.
}

vector3d Body::GetPositionRelTo(FrameId relToId) const
{
	Frame *frame = Frame::GetFrame(m_frame);

	vector3d fpos = frame->GetPositionRelTo(relToId);
	matrix3x3d forient = frame->GetOrientRelTo(relToId);
	return forient * GetPosition() + fpos;
}

vector3d Body::GetInterpPositionRelTo(FrameId relToId) const
{
	Frame *frame = Frame::GetFrame(m_frame);

	vector3d fpos = frame->GetInterpPositionRelTo(relToId);
	matrix3x3d forient = frame->GetInterpOrientRelTo(relToId);
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

matrix3x3d Body::GetOrientRelTo(FrameId relToId) const
{
	Frame *frame = Frame::GetFrame(m_frame);

	matrix3x3d forient = frame->GetOrientRelTo(relToId);
	return forient * GetOrient();
}

matrix3x3d Body::GetInterpOrientRelTo(FrameId relToId) const
{
	Frame *frame = Frame::GetFrame(m_frame);

	matrix3x3d forient = frame->GetInterpOrientRelTo(relToId);
	return forient * GetInterpOrient();
}

vector3d Body::GetVelocityRelTo(FrameId relToId) const
{
	Frame *frame = Frame::GetFrame(m_frame);

	matrix3x3d forient = frame->GetOrientRelTo(relToId);
	vector3d vel = GetVelocity();
	if (m_frame != relToId) vel -= frame->GetStasisVelocity(GetPosition());
	return forient * vel + frame->GetVelocityRelTo(relToId);
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

void Body::SwitchToFrame(FrameId newFrameId)
{
	const Frame *newFrame = Frame::GetFrame(newFrameId);
	const Frame *frame = Frame::GetFrame(m_frame);

	const vector3d vel = GetVelocityRelTo(newFrameId); // do this first because it uses position
	const vector3d fpos = frame->GetPositionRelTo(newFrameId);
	const matrix3x3d forient = frame->GetOrientRelTo(newFrameId);
	SetPosition(forient * GetPosition() + fpos);
	SetOrient(forient * GetOrient());
	SetVelocity(vel + newFrame->GetStasisVelocity(GetPosition()));
	SetFrame(newFrameId);

	LuaEvent::Queue("onFrameChanged", this);
}

void Body::UpdateFrame()
{
	if (!(m_flags & FLAG_CAN_MOVE_FRAME)) return;

	const Frame *frame = Frame::GetFrame(m_frame);

	// falling out of frames
	if (frame->GetRadius() < GetPosition().Length()) {
		FrameId parent = frame->GetParent();
		Frame *newFrame = Frame::GetFrame(parent);
		if (newFrame) { // don't fall out of root frame
			Output("%s leaves frame %s\n", GetLabel().c_str(), frame->GetLabel().c_str());
			SwitchToFrame(parent);
			return;
		}
	}

	// entering into frames
	for (FrameId kid : frame->GetChildren()) {
		Frame *kid_frame = Frame::GetFrame(kid);
		const vector3d pos = GetPositionRelTo(kid);
		if (pos.Length() >= kid_frame->GetRadius()) continue;
		SwitchToFrame(kid);
		Output("%s enters frame %s\n", GetLabel().c_str(), kid_frame->GetLabel().c_str());
		break;
	}
}

vector3d Body::GetTargetIndicatorPosition(FrameId relToId) const
{
	return GetInterpPositionRelTo(relToId);
}

void Body::SetLabel(const std::string &label)
{
	m_label = label;
	Properties().Set("label", label);
}
