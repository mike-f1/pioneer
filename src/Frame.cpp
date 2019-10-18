// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Frame.h"

#include "Body.h"
#include "GameSaveError.h"
#include "JsonUtils.h"
#include "Sfx.h"
#include "Space.h"
#include "collider/collider.h"

std::list<Frame> Frame::s_frames;

Frame::Frame(const Dummy &d, Frame *parent, const char *label, unsigned int flags, double radius):
	m_sbody(nullptr),
	m_astroBody(nullptr),
	m_parent(parent),
	m_flags(flags),
	m_radius(radius),
	m_pos(vector3d(0.0)),
	m_vel(vector3d(0.0)),
	m_angSpeed(0.0),
	m_orient(matrix3x3d::Identity()),
	m_initialOrient(matrix3x3d::Identity())
{
	if (!d.madeWithFactory)
		Error("Frame ctor called directly!\n");

	ClearMovement();
	m_collisionSpace = new CollisionSpace();
	if (m_parent) m_parent->AddChild(this);
	if (label) m_label = label;
}

void Frame::ToJson(Json &frameObj, Frame *f, Space *space)
{
	frameObj["flags"] = f->m_flags;
	frameObj["radius"] = f->m_radius;
	frameObj["label"] = f->m_label;
	frameObj["pos"] = f->m_pos;
	frameObj["ang_speed"] = f->m_angSpeed;
	frameObj["init_orient"] = f->m_initialOrient;
	frameObj["index_for_system_body"] = space->GetIndexForSystemBody(f->m_sbody);
	frameObj["index_for_astro_body"] = space->GetIndexForBody(f->m_astroBody);

	Json childFrameArray = Json::array(); // Create JSON array to contain child frame data.
	for (Frame *kid : f->GetChildren()) {
		Json childFrameArrayEl = Json::object(); // Create JSON object to contain child frame.
		Frame::ToJson(childFrameArrayEl, kid, space);
		childFrameArray.push_back(childFrameArrayEl); // Append child frame object to array.
	}
	if (!childFrameArray.empty())
		frameObj["child_frames"] = childFrameArray; // Add child frame array to frame object.

	// Add sfx array to supplied object.
	SfxManager::ToJson(frameObj, f);
}

Frame *Frame::CreateFrame(Frame *parent, const char *label, unsigned int flags, double radius)
{
	Dummy dummy;
	dummy.madeWithFactory = true;

	s_frames.emplace_back(dummy, parent, label, flags, radius);
	return &s_frames.back();
}

Frame *Frame::FromJson(const Json &frameObj, Space *space, Frame *parent, double at_time)
{
	Dummy dummy;
	dummy.madeWithFactory = true;

	// Set parent to nullptr here in order to avoid this frame
	// being a child twice (due to ctor calling AddChild)
	s_frames.emplace_back(dummy, nullptr, nullptr);

	Frame *f = &s_frames.back();

	f->m_parent = parent;

	try {
		f->m_flags = frameObj["flags"];
		f->m_radius = frameObj["radius"];
		f->m_label = frameObj["label"];
		f->m_pos = frameObj["pos"];
		f->m_angSpeed = frameObj["ang_speed"];
		f->SetInitialOrient(frameObj["init_orient"], at_time);
		f->m_sbody = space->GetSystemBodyByIndex(frameObj["index_for_system_body"]);
		f->m_astroBodyIndex = frameObj["index_for_astro_body"];
		f->m_vel = vector3d(0.0); // m_vel is set to zero.

		if (frameObj.count("child_frames") && frameObj["child_frames"].is_array()) {
			Json childFrameArray = frameObj["child_frames"];
			f->m_children.reserve(childFrameArray.size());
			for (unsigned int i = 0; i < childFrameArray.size(); ++i) {
				f->m_children.push_back(FromJson(childFrameArray[i], space, f, at_time));
			}
		} else {
			f->m_children.clear();
		}
	} catch (Json::type_error &) {
		throw SavedGameCorruptException();
	}

	SfxManager::FromJson(frameObj, f);

	f->ClearMovement();
	return f;
}

void Frame::DeleteFrame(Frame *tobedeleted)
{
	tobedeleted->d.madeWithFactory = true;
	// Find Frame and delete it, let dtor delete its children
	for (std::list<Frame>::const_iterator it = s_frames.begin(); it != s_frames.end(); it++) {
		if (tobedeleted == &(*it)) {
			s_frames.erase(it);
			break;
		}
	}
	tobedeleted->d.madeWithFactory = false;
}

void Frame::PostUnserializeFixup(Frame *f, Space *space)
{
	f->UpdateRootRelativeVars();
	f->m_astroBody = space->GetBodyByIndex(f->m_astroBodyIndex);
	for (Frame *kid : f->GetChildren())
		PostUnserializeFixup(kid, space);
}

Frame::~Frame()
{
	if (!d.madeWithFactory) {
		Error("Frame instance deletion outside 'DeleteFrame'\n");
	}
	// Delete this Frame and recurse deleting children
	delete m_collisionSpace;
	for (Frame *kid : m_children) {
		DeleteFrame(kid);
	}
}

void Frame::RemoveChild(Frame *f)
{
	PROFILE_SCOPED()
	const std::vector<Frame *>::iterator it = std::find(m_children.begin(), m_children.end(), f);
	if (it != m_children.end())
		m_children.erase(it);
}

void Frame::AddGeom(Geom *g) { m_collisionSpace->AddGeom(g); }
void Frame::RemoveGeom(Geom *g) { m_collisionSpace->RemoveGeom(g); }
void Frame::AddStaticGeom(Geom *g) { m_collisionSpace->AddStaticGeom(g); }
void Frame::RemoveStaticGeom(Geom *g) { m_collisionSpace->RemoveStaticGeom(g); }
void Frame::SetPlanetGeom(double radius, Body *obj)
{
	m_collisionSpace->SetSphere(vector3d(0, 0, 0), radius, static_cast<void *>(obj));
}

// doesn't consider stasis velocity
vector3d Frame::GetVelocityRelTo(const Frame *relTo) const
{
	if (this == relTo) return vector3d(0, 0, 0); // early-out to avoid unnecessary computation
	vector3d diff = m_rootVel - relTo->m_rootVel;
	if (relTo->IsRotFrame())
		return diff * relTo->m_rootOrient;
	else
		return diff;
}

vector3d Frame::GetPositionRelTo(const Frame *relTo) const
{
	// early-outs for simple cases, required for accuracy in large systems
	if (this == relTo) return vector3d(0, 0, 0);
	if (GetParent() == relTo) return m_pos; // relative to parent
	if (relTo->GetParent() == this) { // relative to child
		if (!relTo->IsRotFrame())
			return -relTo->m_pos;
		else
			return -relTo->m_pos * relTo->m_orient;
	}
	if (relTo->GetParent() == GetParent()) { // common parent
		if (!relTo->IsRotFrame())
			return m_pos - relTo->m_pos;
		else
			return (m_pos - relTo->m_pos) * relTo->m_orient;
	}

	// use precalculated absolute position and orient
	vector3d diff = m_rootPos - relTo->m_rootPos;
	if (relTo->IsRotFrame())
		return diff * relTo->m_rootOrient;
	else
		return diff;
}

vector3d Frame::GetInterpPositionRelTo(const Frame *relTo) const
{
	// early-outs for simple cases, required for accuracy in large systems
	if (this == relTo) return vector3d(0, 0, 0);
	if (GetParent() == relTo) return m_interpPos; // relative to parent
	if (relTo->GetParent() == this) { // relative to child
		if (!relTo->IsRotFrame())
			return -relTo->m_interpPos;
		else
			return -relTo->m_interpPos * relTo->m_interpOrient;
	}
	if (relTo->GetParent() == GetParent()) { // common parent
		if (!relTo->IsRotFrame())
			return m_interpPos - relTo->m_interpPos;
		else
			return (m_interpPos - relTo->m_interpPos) * relTo->m_interpOrient;
	}

	vector3d diff = m_rootInterpPos - relTo->m_rootInterpPos;
	if (relTo->IsRotFrame())
		return diff * relTo->m_rootInterpOrient;
	else
		return diff;
}

matrix3x3d Frame::GetOrientRelTo(const Frame *relTo) const
{
	if (this == relTo) return matrix3x3d::Identity();
	return relTo->m_rootOrient.Transpose() * m_rootOrient;
}

matrix3x3d Frame::GetInterpOrientRelTo(const Frame *relTo) const
{
	if (this == relTo) return matrix3x3d::Identity();
	return relTo->m_rootInterpOrient.Transpose() * m_rootInterpOrient;
	/*	if (IsRotFrame()) {
		if (relTo->IsRotFrame()) return m_interpOrient * relTo->m_interpOrient.Transpose();
		else return m_interpOrient;
	}
	if (relTo->IsRotFrame()) return relTo->m_interpOrient.Transpose();
	else return matrix3x3d::Identity();
*/
}

void Frame::UpdateInterpTransform(double alpha)
{
	PROFILE_SCOPED()
	m_interpPos = alpha * m_pos + (1.0 - alpha) * m_oldPos;

	double len = m_oldAngDisplacement * (1.0 - alpha);
	if (!is_zero_exact(len)) { // very small values are normal here
		matrix3x3d rot = matrix3x3d::RotateY(len); // RotateY is backwards
		m_interpOrient = m_orient * rot;
	} else
		m_interpOrient = m_orient;

	if (!m_parent)
		ClearMovement();
	else {
		m_rootInterpPos = m_parent->m_rootInterpOrient * m_interpPos + m_parent->m_rootInterpPos;
		m_rootInterpOrient = m_parent->m_rootInterpOrient * m_interpOrient;
	}

	for (Frame *kid : m_children)
		kid->UpdateInterpTransform(alpha);
}

void Frame::GetFrameTransform(const Frame *fFrom, const Frame *fTo, matrix4x4d &m)
{
	matrix3x3d forient = fFrom->GetOrientRelTo(fTo);
	vector3d fpos = fFrom->GetPositionRelTo(fTo);
	m = forient;
	m.SetTranslate(fpos);
}

void Frame::ClearMovement()
{
	UpdateRootRelativeVars();
	m_rootInterpPos = m_rootPos;
	m_rootInterpOrient = m_rootOrient;
	m_oldPos = m_interpPos = m_pos;
	m_interpOrient = m_orient;
	m_oldAngDisplacement = 0.0;
}

void Frame::UpdateOrbitRails(double time, double timestep)
{
	m_oldPos = m_pos;
	m_oldAngDisplacement = m_angSpeed * timestep;

	// update frame position and velocity
	if (m_parent && m_sbody && !IsRotFrame()) {
		m_pos = m_sbody->GetOrbit().OrbitalPosAtTime(time);
		vector3d pos2 = m_sbody->GetOrbit().OrbitalPosAtTime(time + timestep);
		m_vel = (pos2 - m_pos) / timestep;
	}
	// temporary test thing
	else
		m_pos = m_pos + m_vel * timestep;

	// update frame rotation
	double ang = fmod(m_angSpeed * time, 2.0 * M_PI);
	if (!is_zero_exact(ang)) { // frequently used with e^-10 etc
		matrix3x3d rot = matrix3x3d::RotateY(-ang); // RotateY is backwards
		m_orient = m_initialOrient * rot; // angvel always +y
	}
	UpdateRootRelativeVars(); // update root-relative pos/vel/orient

	for (Frame *kid : m_children)
		kid->UpdateOrbitRails(time, timestep);
}

void Frame::SetInitialOrient(const matrix3x3d &m, double time)
{
	m_initialOrient = m;
	double ang = fmod(m_angSpeed * time, 2.0 * M_PI);
	if (!is_zero_exact(ang)) { // frequently used with e^-10 etc
		matrix3x3d rot = matrix3x3d::RotateY(-ang); // RotateY is backwards
		m_orient = m_initialOrient * rot; // angvel always +y
	} else {
		m_orient = m_initialOrient;
	}
}

void Frame::SetOrient(const matrix3x3d &m, double time)
{
	m_orient = m;
	double ang = fmod(m_angSpeed * time, 2.0 * M_PI);
	if (!is_zero_exact(ang)) { // frequently used with e^-10 etc
		matrix3x3d rot = matrix3x3d::RotateY(ang); // RotateY is backwards
		m_initialOrient = m_orient * rot; // angvel always +y
	} else {
		m_initialOrient = m_orient;
	}
}

void Frame::UpdateRootRelativeVars()
{
	// update pos & vel relative to parent frame
	if (!m_parent) {
		m_rootPos = m_rootVel = vector3d(0, 0, 0);
		m_rootOrient = matrix3x3d::Identity();
	} else {
		m_rootPos = m_parent->m_rootOrient * m_pos + m_parent->m_rootPos;
		m_rootVel = m_parent->m_rootOrient * m_vel + m_parent->m_rootVel;
		m_rootOrient = m_parent->m_rootOrient * m_orient;
	}
}
