// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _SENSORS_H
#define _SENSORS_H
/*
 * Ship/station subsystem that holds a list of known contacts
 * and handles IFF
 * Some ideas:
 *  - targeting should be lost when going out of range
 *  - don't run radar sweep every frame (more of an optimization than simulation)
 *  - allow "pinned" radar contacts (visible at all ranges, for missions)
 */
#include "Body.h"
#include "Color.h"

#include <list>

class Body;
class HudTrail;
class Ship;

class Sensors {
public:
	enum class IFF {
		UNKNOWN, //also applies to inert objects
		NEUTRAL,
		ALLY,
		HOSTILE
	};

	enum class TargetingCriteria {
		TARGET_NEAREST_HOSTILE
	};

	struct RadarContact {
		RadarContact();
		RadarContact(Body *);
		~RadarContact();
		Body *body;
		HudTrail *trail;
		double distance;
		IFF iff;
		bool fresh;
	};

	typedef std::list<RadarContact> ContactList;

	static Color IFFColor(IFF);
	static bool ContactDistanceSort(const RadarContact &a, const RadarContact &b);

	Sensors(Ship *owner);
	bool ChooseTarget(TargetingCriteria);
	IFF CheckIFF(Body *other);
	const ContactList &GetContacts() { return m_radarContacts; }
	const ContactList &GetStaticContacts() { return m_staticContacts; }
	void Update(float time);
	void UpdateIFF(Body *);
	void ResetTrails();

private:
	Ship *m_owner;
	ContactList m_radarContacts;
	ContactList m_staticContacts; //things we know of regardless of range

	void PopulateStaticContacts();
};

#endif
