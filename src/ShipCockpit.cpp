// Copyright © 2013-14 Meteoric Games Ltd
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "ShipCockpit.h"

#include "CameraController.h"
#include "InGameViews.h"
#include "InGameViewsLocator.h"
#include "ModelCache.h"
#include "Player.h"
#include "WorldView.h"
#include "graphics/Renderer.h"
#include "graphics/RendererLocator.h"
#include "libs/Easing.h"

static const char *DEFAULT_COCKPIT_NAME = "default_cockpit";

static float calculateSignedForwardVelocity(const vector3d &normalized_forward, const vector3d &velocity)
{
	float velz_cos = velocity.Dot(normalized_forward);
	return (velz_cos * normalized_forward).Length() * (velz_cos < 0.0 ? -1.0 : 1.0);
}

ShipCockpit::ShipCockpit(const std::string &modelName) :
	m_shipDir(0.0),
	m_shipYaw(0.0),
	m_dir(0.0),
	m_yaw(0.0),
	m_rotInterp(0.f),
	m_transInterp(0.f),
	m_gForce(0.f),
	m_offset(0.f),
	m_shipVel(0.f),
	m_translate(0.0),
	m_transform(matrix4x4d::Identity())
{
	SceneGraph::Model *m = nullptr;
	if (!modelName.empty())	m = ModelCache::FindModel(modelName, false);

	if (m) {
		SetModel(modelName.c_str());
	} else {
		Output("No cockpit model '%s', use default\n", modelName.c_str());
		SetModel(DEFAULT_COCKPIT_NAME);
	}

	assert(GetModel());
	SetColliding(false);
	m_icc = nullptr;
}

ShipCockpit::~ShipCockpit()
{
}

void ShipCockpit::Render(const Camera *camera, const vector3d &viewCoords, const matrix4x4d &viewTransform)
{
	PROFILE_SCOPED()
	RenderModel(camera, viewCoords, viewTransform);
}

inline void ShipCockpit::resetInternalCameraController()
{
	m_icc = static_cast<InternalCameraController *>(InGameViewsLocator::getInGameViews()
		->GetWorldView()->shipView.GetCameraController());
}

void ShipCockpit::Update(const Player *player, float timeStep)
{
	m_transform = matrix4x4d::Identity();

	if (m_icc == nullptr) {
		// I don't know where to put this
		resetInternalCameraController();
	}

	double rotX;
	double rotY;
	m_icc->getRots(rotX, rotY);
	m_transform.RotateX(rotX);
	m_transform.RotateY(rotY);

	vector3d cur_dir = player->GetOrient().VectorZ().Normalized();
	if (cur_dir.Dot(m_shipDir) < 1.0f) {
		m_rotInterp = 0.0f;
		m_shipDir = cur_dir;
	}

	//---------------------------------------- Acceleration
	float cur_vel = calculateSignedForwardVelocity(-cur_dir, player->GetVelocity()); // Forward is -Z
	float gforce = Clamp(floorf(((fabs(cur_vel) - m_shipVel) / timeStep) / 9.8f), -COCKPIT_MAX_GFORCE, COCKPIT_MAX_GFORCE);
	if (fabs(cur_vel) > 500000.0f || // Limit gforce measurement so we don't get astronomical fluctuations
		fabs(gforce - m_gForce) > 100.0) { // Smooth out gforce one frame spikes, sometimes happens when hitting max speed due to the thrust limiters
		gforce = 0.0f;
	}
	if (fabs(gforce - m_gForce) > 100.0) {
		gforce = 0.0f;
	}
	if (fabs(m_translate.z - m_offset) < 0.001f) {
		m_transInterp = 0.0f;
	}
	float offset = (gforce > 14.0f ? -1.0f : (gforce < -14.0f ? 1.0f : 0.0f)) * COCKPIT_ACCEL_OFFSET;
	m_transInterp += timeStep * COCKPIT_ACCEL_INTERP_MULTIPLIER;
	if (m_transInterp > 1.0f) {
		m_transInterp = 1.0f;
		m_translate.z = offset;
	}
	m_translate.z = Easing::Quad::EaseIn(double(m_transInterp), m_translate.z, offset - m_translate.z, 1.0);
	m_gForce = gforce;
	m_offset = offset;
	m_shipVel = cur_vel;

	//------------------------------------------ Rotation
	// For yaw/pitch
	vector3d rot_axis = cur_dir.Cross(m_dir).Normalized();
	vector3d yaw_axis = player->GetOrient().VectorY().Normalized();
	vector3d pitch_axis = player->GetOrient().VectorX().Normalized();
	float dot = cur_dir.Dot(m_dir);
	float angle = acos(dot);
	// For roll
	if (yaw_axis.Dot(m_shipYaw) < 1.0f) {
		m_rotInterp = 0.0f;
		m_shipYaw = yaw_axis;
	}
	vector3d rot_yaw_axis = yaw_axis.Cross(m_yaw).Normalized();
	float dot_yaw = yaw_axis.Dot(m_yaw);
	float angle_yaw = acos(dot_yaw);

	if (dot < 1.0f || dot_yaw < 1.0f) {
		// Lag/Recovery interpolation
		m_rotInterp += timeStep * COCKPIT_ROTATION_INTERP_MULTIPLIER;
		if (m_rotInterp > 1.0f) {
			m_rotInterp = 1.0f;
		}

		// Yaw and Pitch
		if (dot < 1.0f) {
			if (angle > DEG2RAD(COCKPIT_LAG_MAX_ANGLE)) {
				angle = DEG2RAD(COCKPIT_LAG_MAX_ANGLE);
			}
			angle = Easing::Quad::EaseOut(m_rotInterp, angle, -angle, 1.0f);
			m_dir = cur_dir;
			if (angle >= 0.0f) {
				m_dir.ArbRotate(rot_axis, angle);
				// Apply pitch
				vector3d yz_proj = (m_dir - (m_dir.Dot(pitch_axis) * pitch_axis)).Normalized();
				float pitch_cos = yz_proj.Dot(cur_dir);
				float pitch_angle = 0.0f;
				if (pitch_cos < 1.0f) {
					pitch_angle = acos(pitch_cos);
					if (rot_axis.Dot(pitch_axis) < 0) {
						pitch_angle = -pitch_angle;
					}
					m_transform.RotateX(-pitch_angle);
				}
				// Apply yaw
				vector3d xz_proj = (m_dir - (m_dir.Dot(yaw_axis) * yaw_axis)).Normalized();
				float yaw_cos = xz_proj.Dot(cur_dir);
				float yaw_angle = 0.0f;
				if (yaw_cos < 1.0f) {
					yaw_angle = acos(yaw_cos);
					if (rot_axis.Dot(yaw_axis) < 0) {
						yaw_angle = -yaw_angle;
					}
					m_transform.RotateY(-yaw_angle);
				}
			} else {
				angle = 0.0f;
			}
		}

		// Roll
		if (dot_yaw < 1.0f) {
			if (angle_yaw > DEG2RAD(COCKPIT_LAG_MAX_ANGLE)) {
				angle_yaw = DEG2RAD(COCKPIT_LAG_MAX_ANGLE);
			}
			if (dot_yaw < 1.0f) {
				angle_yaw = Easing::Quad::EaseOut(m_rotInterp, angle_yaw, -angle_yaw, 1.0f);
			}
			m_yaw = yaw_axis;
			if (angle_yaw >= 0.0f) {
				m_yaw.ArbRotate(rot_yaw_axis, angle_yaw);
				// Apply roll
				vector3d xy_proj = (m_yaw - (m_yaw.Dot(cur_dir) * cur_dir)).Normalized();
				float roll_cos = xy_proj.Dot(yaw_axis);
				float roll_angle = 0.0f;
				if (roll_cos < 1.0f) {
					roll_angle = acos(roll_cos);
					if (rot_yaw_axis.Dot(cur_dir) < 0) {
						roll_angle = -roll_angle;
					}
					m_transform.RotateZ(-roll_angle);
				}
			} else {
				angle_yaw = 0.0f;
			}
		}
	} else {
		m_rotInterp = 0.0f;
	}
}

void ShipCockpit::RenderCockpit(const Camera *camera, FrameId frameId)
{
	PROFILE_SCOPED()
	RendererLocator::getRenderer()->ClearDepthBuffer();
	Body::SetFrame(frameId);
	Render(camera, m_translate, m_transform);
	Body::SetFrame(FrameId::Invalid);
}

void ShipCockpit::OnActivated(const Player *player)
{
	assert(player);
	m_dir = player->GetOrient().VectorZ().Normalized();
	m_yaw = player->GetOrient().VectorY().Normalized();
	m_shipDir = m_dir;
	m_shipYaw = m_yaw;
	m_shipVel = calculateSignedForwardVelocity(-m_shipDir, player->GetVelocity());
}
