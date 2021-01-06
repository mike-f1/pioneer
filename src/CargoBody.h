// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _CARGOBODY_H
#define _CARGOBODY_H

#include "DynamicBody.h"
#include "LuaRef.h"
#include <string>

class CargoBody final: public DynamicBody {
public:
	OBJDEF(CargoBody, DynamicBody, CARGOBODY);
	CargoBody() = delete;
	CargoBody(const LuaRef &cargo, float selfdestructTimer = 86400.0f); // default to 24 h lifetime
	CargoBody(const Json &jsonObj, Space *space);

	Json SaveToJson(Space *space) override;

	LuaRef GetCargoType() const { return m_cargo; }
	void SetLabel(const std::string &label) override;
	void Render(const Camera *camera, const vector3d &viewCoords, const matrix4x4d &viewTransform) override;
	void TimeStepUpdate(const float timeStep) override;
	bool OnCollision(Object *o, uint32_t flags, double relVel) override;
	bool OnDamage(Object *attacker, float kgDamage, const CollisionContact &contactData) override;

	~CargoBody(){};

protected:

private:
	void Init();
	LuaRef m_cargo;
	float m_hitpoints;
	float m_selfdestructTimer;
	bool m_hasSelfdestruct;
};

#endif /* _CARGOBODY_H */
