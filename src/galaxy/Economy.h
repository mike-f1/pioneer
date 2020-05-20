// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef GALAXY_ECONOMY_H
#define GALAXY_ECONOMY_H

#include "libs/bitmask_op.h"

namespace GalacticEconomy {
	enum class EconType;
}

template<>
struct enable_bitmask_operators<GalacticEconomy::EconType> {
	static constexpr bool enable = true;
};

namespace GalacticEconomy {
	enum class EconType { // <enum scope=GalacticEconomy name=EconType prefix=ECON_ public>
		NONE = 0,
		MINING = 1 << 0,
		AGRICULTURE = 1 << 1,
		INDUSTRY = 1 << 2
	};

	enum class Commodity { // <enum scope='GalacticEconomy::Commodity' name=CommodityType public>
		NONE, // <enum skip>

		HYDROGEN,
		LIQUID_OXYGEN,
		METAL_ORE,
		CARBON_ORE,
		METAL_ALLOYS,
		PLASTICS,
		FRUIT_AND_VEG,
		ANIMAL_MEAT,
		LIVE_ANIMALS,
		LIQUOR,
		GRAIN,
		TEXTILES,
		FERTILIZER,
		WATER,
		MEDICINES,
		CONSUMER_GOODS,
		COMPUTERS,
		ROBOTS,
		PRECIOUS_METALS,
		INDUSTRIAL_MACHINERY,
		FARM_MACHINERY,
		MINING_MACHINERY,
		AIR_PROCESSORS,
		SLAVES,
		HAND_WEAPONS,
		BATTLE_WEAPONS,
		NERVE_GAS,
		NARCOTICS,
		MILITARY_FUEL,
		RUBBISH,
		RADIOACTIVES,

		COMMODITY_COUNT // <enum skip>
	};

	static const int COMMODITY_COUNT = int(Commodity::COMMODITY_COUNT);

	struct CommodityInfo {
		static const int MAX_ECON_INPUTS = 2;

		const char *name;
		// production requirement. eg metal alloys input would be metal ore
		// (used in trade balance calculations)
		Commodity inputs[MAX_ECON_INPUTS];
		EconType econType;
	};

	extern const CommodityInfo COMMODITY_DATA[];
} // namespace GalacticEconomy

#endif
