// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _GAMECONSTS_H
#define _GAMECONSTS_H

#include <cstdint>

constexpr double PHYSICS_HZ = 60.0;

constexpr double MAX_LANDING_SPEED = 30.0; // m/sec
constexpr double LIGHT_SPEED = 3e8; // m/sec

constexpr uint32_t UNIVERSE_SEED = 0xabcd1234;

constexpr double EARTH_RADIUS = 6378135.0; // m
constexpr double EARTH_MASS = 5.9742e24; // Kg
constexpr double SOL_RADIUS = 6.955e8; // m
constexpr double SOL_MASS = 1.98892e30; // Kg

constexpr double AU = 149598000000.0; // m
constexpr double G = 6.67428e-11;

constexpr double EARTH_ATMOSPHERE_SURFACE_DENSITY = 1.225;
constexpr double GAS_CONSTANT_R = 8.3144621;

#endif /* _GAMECONSTS_H */
