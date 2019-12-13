#ifndef TRANSFERPLANNER_H
#define TRANSFERPLANNER_H

#include "vector3.h"
#include <string>

enum BurnDirection {
	PROGRADE,
	NORMAL,
	RADIAL,
};

class TransferPlanner {
public:
	TransferPlanner();
	vector3d GetVel() const;
	vector3d GetOffsetVel() const;
	vector3d GetPosition() const;
	double GetStartTime() const;
	void SetPosition(const vector3d &position);
	void IncreaseFactor(), ResetFactor(), DecreaseFactor();
	void AddStartTime(double timeStep);
	void ResetStartTime();
	void AddDv(BurnDirection d, double dv);
	void ResetDv(BurnDirection d);
	void ResetDv();
	std::string printDeltaTime();
	std::string printDv(BurnDirection d);
	std::string printFactor();

private:
	double m_dvPrograde;
	double m_dvNormal;
	double m_dvRadial;
	double m_factor; // dv multiplier
	const double m_factorFactor = 5.0; // m_factor multiplier
	vector3d m_position;
	vector3d m_velocity;
	double m_startTime;
};

#endif // TRANSFERPLANNER_H
