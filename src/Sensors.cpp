// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Sensors.h"

#include "Body.h"
#include "Game.h"
#include "GameLocator.h"
#include "HudTrail.h"
#include "Player.h"
#include "Ship.h"
#include "Space.h"

Sensors::RadarContact::RadarContact() :
	body(nullptr),
	trail(nullptr),
	distance(0.0),
	iff(IFF::UNKNOWN),
	fresh(true)
{
}

Sensors::RadarContact::RadarContact(Body *b) :
	body(b),
	trail(nullptr),
	distance(0.0),
	iff(IFF::UNKNOWN),
	fresh(true)
{
}

Sensors::RadarContact::~RadarContact()
{
	body = nullptr;
	delete trail;
}

Color Sensors::IFFColor(IFF iff)
{
	switch (iff) {
	case IFF::NEUTRAL: return Color::BLUE;
	case IFF::ALLY: return Color::GREEN;
	case IFF::HOSTILE: return Color::RED;
	case IFF::UNKNOWN:
	default:
		return Color::GRAY;
	}
}

bool Sensors::ContactDistanceSort(const RadarContact &a, const RadarContact &b)
{
	return a.distance < b.distance;
}

Sensors::Sensors(Ship *owner)
{
	m_owner = owner;
}

bool Sensors::ChooseTarget(TargetingCriteria crit)
{
	PROFILE_SCOPED();
	bool found = false;

	m_radarContacts.sort(ContactDistanceSort);

	for (auto it = m_radarContacts.begin(); it != m_radarContacts.end(); ++it) {
		//match object type
		//match iff
		if (it->body->IsType(Object::SHIP)) {
			//if (it->iff != IFF_HOSTILE) continue;
			//should move the target to ship after all (from PlayerShipController)
			//targeting inputs stay in PSC
			static_cast<Player *>(m_owner)->SetCombatTarget(it->body);
			found = true;
			break;
		}
	}

	return found;
}

Sensors::IFF Sensors::CheckIFF(Body *other)
{
	PROFILE_SCOPED();
	//complicated relationship check goes here
	if (other->IsType(Object::SHIP)) {
		uint8_t rel = m_owner->GetRelations(other);
		if (rel == 0)
			return IFF::HOSTILE;
		else if (rel == 100)
			return IFF::ALLY;
		return IFF::NEUTRAL;
	} else {
		return IFF::UNKNOWN;
	}
}

void Sensors::Update(float time)
{
	PROFILE_SCOPED();
	if (m_owner != GameLocator::getGame()->GetPlayer()) return;

	PopulateStaticContacts(); //no need to do all the time

	//Find nearby contacts, same range as radar scanner. It should use these
	//contacts, worldview labels too.
	Space::BodyNearList nearby = GameLocator::getGame()->GetSpace()->GetBodiesMaybeNear(m_owner, 100000.0f);
	for (Body *body : nearby) {
		if (body == m_owner || !body->IsType(Object::SHIP)) continue;
		if (body->IsDead()) continue;

		auto cit = m_radarContacts.begin();
		while (cit != m_radarContacts.end()) {
			if (cit->body == body) break;
			++cit;
		}

		//create new contact or refresh old
		if (cit == m_radarContacts.end()) {
			m_radarContacts.push_back(RadarContact());
			RadarContact &rc = m_radarContacts.back();
			rc.body = body;
			rc.iff = CheckIFF(rc.body);
			rc.trail = new HudTrail(rc.body, IFFColor(rc.iff));
		} else {
			cit->fresh = true;
		}
	}

	//update contacts and delete stale ones
	auto it = m_radarContacts.begin();
	while (it != m_radarContacts.end()) {
		if (!it->fresh) {
			m_radarContacts.erase(it++);
		} else {
			const Ship *ship = dynamic_cast<Ship *>(it->body);
			if (ship && Ship::FLYING == ship->GetFlightState()) {
				it->distance = m_owner->GetPositionRelTo(it->body).Length();
				it->trail->Update(time);
			} else {
				it->trail->Reset(FrameId::Invalid);
			}
			it->fresh = false;
			++it;
		}
	}
}

void Sensors::UpdateIFF(Body *b)
{
	PROFILE_SCOPED();
	for (auto it = m_radarContacts.begin(); it != m_radarContacts.end(); ++it) {
		if (it->body == b) {
			it->iff = CheckIFF(b);
			it->trail->SetColor(IFFColor(it->iff));
		}
	}
}

void Sensors::ResetTrails()
{
	PROFILE_SCOPED();
	for (auto it = m_radarContacts.begin(); it != m_radarContacts.end(); ++it)
		it->trail->Reset(GameLocator::getGame()->GetPlayer()->GetFrame());
}

void Sensors::PopulateStaticContacts()
{
	PROFILE_SCOPED();
	m_staticContacts.clear();

	for (Body *b : GameLocator::getGame()->GetSpace()->GetBodies()) {
		switch (b->GetType()) {
		case Object::STAR:
		case Object::PLANET:
		case Object::CITYONPLANET:
		case Object::SPACESTATION:
			break;
		default:
			continue;
		}
		m_staticContacts.push_back(RadarContact(b));
		RadarContact &rc = m_staticContacts.back();
		rc.fresh = true;
	}
}
