#include "TransferPlanner.h"

#include "Frame.h"
#include "Game.h"
#include "GameLocator.h"
#include "GameLog.h"
#include "Lang.h"
#include "Orbit.h"
#include "Player.h"
#include "galaxy/SystemBody.h"

#include <iomanip>
#include <sstream>

TransferPlanner::TransferPlanner() :
	m_position(0., 0., 0.),
	m_velocity(0., 0., 0.)
{
	m_dvPrograde = 0.0;
	m_dvNormal = 0.0;
	m_dvRadial = 0.0;
	m_startTime = 0.0;
	m_factor = 1;
}

vector3d TransferPlanner::GetVel() const { return m_velocity + GetOffsetVel(); }

vector3d TransferPlanner::GetOffsetVel() const
{
	if (m_position.ExactlyEqual(vector3d(0., 0., 0.)))
		return vector3d(0., 0., 0.);

	const vector3d pNormal = m_position.Cross(m_velocity);

	return m_dvPrograde * m_velocity.Normalized() +
		m_dvNormal * pNormal.Normalized() +
		m_dvRadial * m_position.Normalized();
}

void TransferPlanner::AddStartTime(double timeStep)
{
	if (std::fabs(m_startTime) < 1.)
		m_startTime = GameLocator::getGame()->GetTime();

	m_startTime += m_factor * timeStep;
	double deltaT = m_startTime - GameLocator::getGame()->GetTime();
	if (deltaT > 0.) {
		FrameId frameId = Frame::GetFrame(GameLocator::getGame()->GetPlayer()->GetFrame())->GetNonRotFrame();
		Frame *frame = Frame::GetFrame(frameId);
		Orbit playerOrbit = Orbit::FromBodyState(GameLocator::getGame()->GetPlayer()->GetPositionRelTo(frameId),
			GameLocator::getGame()->GetPlayer()->GetVelocityRelTo(frameId),
			frame->GetSystemBody()->GetMass()
			);

		m_position = playerOrbit.OrbitalPosAtTime(deltaT);
		m_velocity = playerOrbit.OrbitalVelocityAtTime(frame->GetSystemBody()->GetMass(), deltaT);
	} else
		ResetStartTime();
}

void TransferPlanner::ResetStartTime()
{
	m_startTime = 0;
	Frame *frame = Frame::GetFrame(GameLocator::getGame()->GetPlayer()->GetFrame());
	if (!frame || GetOffsetVel().ExactlyEqual(vector3d(0., 0., 0.))) {
		m_position = vector3d(0., 0., 0.);
		m_velocity = vector3d(0., 0., 0.);
	} else {
		frame = Frame::GetFrame(frame->GetNonRotFrame());
		m_position = GameLocator::getGame()->GetPlayer()->GetPositionRelTo(frame->GetId());
		m_velocity = GameLocator::getGame()->GetPlayer()->GetVelocityRelTo(frame->GetId());
	}
}

double TransferPlanner::GetStartTime() const
{
	return m_startTime;
}

static std::string formatTime(double t)
{
	std::stringstream formattedTime;
	formattedTime << std::setprecision(1) << std::fixed;
	double absT = fabs(t);
	if (absT < 60.)
		formattedTime << t << "s";
	else if (absT < 3600)
		formattedTime << t / 60. << "m";
	else if (absT < 86400)
		formattedTime << t / 3600. << "h";
	else if (absT < 31536000)
		formattedTime << t / 86400. << "d";
	else
		formattedTime << t / 31536000. << "y";
	return formattedTime.str();
}

std::string TransferPlanner::printDeltaTime()
{
	std::stringstream out;
	out << std::setw(9);
	double deltaT = m_startTime - GameLocator::getGame()->GetTime();
	if (std::fabs(m_startTime) < 1.)
		out << Lang::NOW;
	else
		out << formatTime(deltaT);

	return out.str();
}

void TransferPlanner::AddDv(BurnDirection d, double dv)
{
	if (m_position.ExactlyEqual(vector3d(0., 0., 0.))) {
		FrameId frame = Frame::GetFrame(GameLocator::getGame()->GetPlayer()->GetFrame())->GetNonRotFrame();
		m_position = GameLocator::getGame()->GetPlayer()->GetPositionRelTo(frame);
		m_velocity = GameLocator::getGame()->GetPlayer()->GetVelocityRelTo(frame);
		m_startTime = GameLocator::getGame()->GetTime();
	}

	switch (d) {
	case PROGRADE: m_dvPrograde += m_factor * dv; break;
	case NORMAL: m_dvNormal += m_factor * dv; break;
	case RADIAL: m_dvRadial += m_factor * dv; break;
	}
}

void TransferPlanner::ResetDv(BurnDirection d)
{
	switch (d) {
	case PROGRADE: m_dvPrograde = 0; break;
	case NORMAL: m_dvNormal = 0; break;
	case RADIAL: m_dvRadial = 0; break;
	}

	if (std::fabs(m_startTime) < 1. &&
		GetOffsetVel().ExactlyEqual(vector3d(0., 0., 0.))) {
		m_position = vector3d(0., 0., 0.);
		m_velocity = vector3d(0., 0., 0.);
		m_startTime = 0.;
	}
}

void TransferPlanner::ResetDv()
{
	m_dvPrograde = 0;
	m_dvNormal = 0;
	m_dvRadial = 0;

	if (std::fabs(m_startTime) < 1.) {
		m_position = vector3d(0., 0., 0.);
		m_velocity = vector3d(0., 0., 0.);
		m_startTime = 0.;
	}
}

std::string TransferPlanner::printDv(BurnDirection d)
{
	double dv = 0;
	char buf[10];

	switch (d) {
	case PROGRADE: dv = m_dvPrograde; break;
	case NORMAL: dv = m_dvNormal; break;
	case RADIAL: dv = m_dvRadial; break;
	}

	snprintf(buf, sizeof(buf), "%6.0fm/s", dv);
	return std::string(buf);
}

void TransferPlanner::IncreaseFactor(void)
{
	if (m_factor > 1000) return;
	m_factor *= m_factorFactor;
}
void TransferPlanner::ResetFactor(void) { m_factor = 1; }

void TransferPlanner::DecreaseFactor(void)
{
	if (m_factor < 0.0002) return;
	m_factor /= m_factorFactor;
}

std::string TransferPlanner::printFactor(void)
{
	char buf[16];
	snprintf(buf, sizeof(buf), "%8gx", 10 * m_factor);
	return std::string(buf);
}

vector3d TransferPlanner::GetPosition() const { return m_position; }

void TransferPlanner::SetPosition(const vector3d &position) { m_position = position; }
