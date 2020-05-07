#ifndef _CSG_DEFINITIONS_H
#define _CSG_DEFINITIONS_H

#include "vector3.h"

/* Use CSG* to skip edge-tri test if a Geom inside
 * bbox of another is inside a defined volume placed
 * on the first geometry.
 * Eg.: Orbital SpaceStations have a cylindrical volume which
 * can be defined and used to replace an expensive collision
 * test when checked object is inside of that volume.
 *
 * When 'dock' is set, given object will trigger docking
 * sequence (see in Geom 'geomFlag = 0x10' in CheckInside*).
*/

enum class MainDirection { X, Y, Z }; // <- Only *Y* is implemented as all stations rotate along that axis

struct CSG_CentralCylinder {
	CSG_CentralCylinder() :
		m_diameter(-1.0)
	{}
	CSG_CentralCylinder(float diameter, float minH, float maxH, bool sTD, MainDirection dir = MainDirection::Y) :
		m_diameter(diameter),
		m_minH(minH),
		m_maxH(maxH),
		m_shouldTriggerDocking(sTD),
		m_mainDir(dir)
	{}

	float m_diameter;
	float m_minH, m_maxH;
	bool m_shouldTriggerDocking;
	MainDirection m_mainDir; // Should always be Y (which is the rotating axis of SpaceStations)
};

struct CSG_Box {
	CSG_Box(const vector3f &min_, const vector3f &max_, bool dock_) :
		m_min(min_),
		m_max(max_),
		m_shouldTriggerDocking(dock_)
	{}

	vector3f m_min, m_max;
	bool m_shouldTriggerDocking;
};

#endif // _CSG_DEFINITIONS_H
