// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _MISSILE_H
#define _MISSILE_H

#include "DynamicBody.h"
#include "ShipType.h"

class AICommand;

class Missile final: public DynamicBody {
public:
	OBJDEF(Missile, DynamicBody, MISSILE);
	Missile() = delete;
	Missile(const ShipType::Id &type, Body *owner, int power = -1);
	Missile(const Json &jsonObj, Space *space);
	virtual ~Missile();

	Json SaveToJson(Space *space) const override;

	void StaticUpdate(const float timeStep) override;
	void TimeStepUpdate(const float timeStep) override;
	bool OnCollision(Object *o, uint32_t flags, double relVel) override;
	bool OnDamage(Object *attacker, float kgDamage, const CollisionContact &contactData) override;
	void NotifyRemoved(const Body *const removedBody) override;
	void PostLoadFixup(Space *space) override;
	void Render(const Camera *camera, const vector3d &viewCoords, const matrix4x4d &viewTransform) override;

	void ECMAttack(int power_val);
	Body *GetOwner() const { return m_owner; }
	bool IsArmed() const { return m_armed; }
	void Arm();
	void Disarm();
	void AIKamikaze(Body *target);

protected:

private:
	void Explode();
	AICommand *m_curAICmd;
	int m_power;
	Body *m_owner;
	bool m_armed;
	const ShipType *m_type;

	int m_ownerIndex; // deserialisation
};

#endif /* _MISSILE_H */
