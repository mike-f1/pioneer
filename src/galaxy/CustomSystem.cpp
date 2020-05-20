// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "CustomSystem.h"

#include "GalaxyEnums.h"
#include "libs/utils.h"
#include "libs/gameconsts.h"

CustomSystem::CustomSystem() :
	sBody(nullptr),
	numStars(0),
	seed(0),
	want_rand_explored(true),
	faction(nullptr),
	govType(Polit::GOV_INVALID),
	want_rand_lawlessness(true)
{
	for (int i = 0; i < 4; ++i)
		primaryType[i] = GalaxyEnums::BodyType::TYPE_GRAVPOINT;
}

CustomSystem::~CustomSystem()
{
	delete sBody;
}

void CustomSystem::SanityChecks()
{
	if (IsRandom()) return;
	else sBody->SanityChecks();
}

CustomSystemBody::CustomSystemBody() :
	aspectRatio(fixed(1, 1)),
	averageTemp(1),
	want_rand_offset(true),
	latitude(0.0),
	longitude(0.0),
	volatileGas(0),
	ringStatus(WANT_RANDOM_RINGS),
	seed(0),
	want_rand_seed(true)
{
}

CustomSystemBody::~CustomSystemBody()
{
	for (std::vector<CustomSystemBody *>::iterator
			 it = children.begin();
		 it != children.end(); ++it) {
		delete (*it);
	}
}

static void checks(CustomSystemBody &csb)
{
	if (csb.name.empty()) {
		Error("custom system with name not set!\n");
		// throw an exception? Then it can be "catch" *per file*...
	}
	if (csb.radius <= 0 && csb.mass <= 0) {
		if (csb.type != GalaxyEnums::BodyType::TYPE_STARPORT_ORBITAL &&
			csb.type != GalaxyEnums::BodyType::TYPE_STARPORT_SURFACE &&
			csb.type != GalaxyEnums::BodyType::TYPE_GRAVPOINT
			) Error("custom system body '%s' with both radius ans mass left undefined!", csb.name.c_str());
	}
	if (csb.radius <= 0 && csb.type != GalaxyEnums::BodyType::TYPE_STARPORT_ORBITAL &&
			csb.type != GalaxyEnums::BodyType::TYPE_STARPORT_SURFACE &&
			csb.type != GalaxyEnums::BodyType::TYPE_GRAVPOINT
		) {
		Output("Warning: 'radius' is %f for body '%s'\n", csb.radius.ToFloat(), csb.name.c_str());
	}
	if (csb.mass <= 0 && csb.type != GalaxyEnums::BodyType::TYPE_STARPORT_ORBITAL &&
			csb.type != GalaxyEnums::BodyType::TYPE_STARPORT_SURFACE &&
			csb.type != GalaxyEnums::BodyType::TYPE_GRAVPOINT
		) {
		Output("Warning: 'mass' is %f for body '%s'\n", csb.mass.ToFloat(), csb.name.c_str());
	}
	if (csb.averageTemp <= 0 && csb.type != GalaxyEnums::BodyType::TYPE_STARPORT_ORBITAL &&
			csb.type != GalaxyEnums::BodyType::TYPE_STARPORT_SURFACE &&
			csb.type != GalaxyEnums::BodyType::TYPE_GRAVPOINT
		) {
		Output("Warning: 'averageTemp' is %i for body '%s'\n", csb.averageTemp, csb.name.c_str());
	}
	if (csb.type == GalaxyEnums::BodyType::TYPE_STAR_S_BH ||
		csb.type == GalaxyEnums::BodyType::TYPE_STAR_IM_BH ||
		csb.type == GalaxyEnums::BodyType::TYPE_STAR_SM_BH
	    ) {
		double schwarzschild = 2 * csb.mass.ToDouble() * ((G * SOL_MASS) / (LIGHT_SPEED * LIGHT_SPEED));
		schwarzschild /= SOL_RADIUS;
		if (csb.radius < schwarzschild) {
			Output("Warning: Blackhole radius defaulted to Schwarzschild radius (%f Sol radii)\n", schwarzschild);
			csb.radius = schwarzschild;
		}
	}
}

void CustomSystemBody::SanityChecks()
{
	checks(*this);
	for (CustomSystemBody *csb:children) csb->SanityChecks();
}
