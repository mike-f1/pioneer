// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _GAME_H
#define _GAME_H

#include "JsonFwd.h"
#include "RefCounted.h"
#include "galaxy/GalaxyCache.h"
#include "galaxy/SystemPath.h"
#include "gameconsts.h"
#include "vector3.h"
#include <string>
#include <list>

class Galaxy;
class GameLog;
class HyperspaceCloud;
class LuaTimer;
class Player;
class Space;
class TransferPlanner;

namespace Graphics {
	class Renderer;
}

class Game {
	friend class GameState;
	// start docked in station referenced by path or nearby to body if it is no station
	Game(const SystemPath &path, const double startDateTime, unsigned int cacheRadius);

	// load game
	Game(const Json &jsonObj, unsigned int cacheRadius);

	~Game();

	// save game
	void ToJson(Json &jsonObj);

public:
	// various game states
	bool IsNormalSpace() const { return m_state == State::NORMAL; }
	bool IsHyperspace() const { return m_state == State::HYPERSPACE; }

	RefCountedPtr<Galaxy> GetGalaxy() const;
	Space *GetSpace() const { return m_space.get(); }
	double GetTime() const { return m_time; }
	Player *GetPlayer() const { return m_player.get(); }

	// physics step
	void TimeStep(float step);

	// update time acceleration once per render frame
	// returns true if timeaccel was changed
	bool UpdateTimeAccel();

	// request switch to hyperspace
	void WantHyperspace();

	// hyperspace parameters. only meaningful when IsHyperspace() is true
	float GetHyperspaceProgress() const { return m_hyperspaceProgress; }
	double GetHyperspaceDuration() const { return m_hyperspaceDuration; }
	double GetHyperspaceEndTime() const { return m_hyperspaceEndTime; }
	double GetHyperspaceArrivalProbability() const;
	const SystemPath &GetHyperspaceDest() const { return m_hyperspaceDest; }
	const SystemPath &GetHyperspaceSource() const { return m_hyperspaceSource; }
	void RemoveHyperspaceCloud(HyperspaceCloud *);

	void GetHyperspaceExitParams(const SystemPath &source, const SystemPath &dest, vector3d &pos, vector3d &vel);

	void GetHyperspaceExitParams(const SystemPath &source, vector3d &pos, vector3d &vel);

	vector3d GetHyperspaceExitPoint(const SystemPath &source, const SystemPath &dest)
	{
		vector3d pos, vel;
		GetHyperspaceExitParams(source, dest, pos, vel);
		return pos;
	}

	enum TimeAccel {
		TIMEACCEL_PAUSED,
		TIMEACCEL_1X,
		TIMEACCEL_10X,
		TIMEACCEL_100X,
		TIMEACCEL_1000X,
		TIMEACCEL_10000X,
		TIMEACCEL_HYPERSPACE
	};

	void SetTimeAccel(TimeAccel t);
	void RequestTimeAccel(TimeAccel t, bool force = false);

	/// Requests an increase in time acceleration
	/// @param force if set to false the system can reject the request under certain conditions
	void RequestTimeAccelInc(bool force = false);
	/// Requests a decrease in time acceleration
	/// @param force if set to false the system can reject the request under certain conditions
	void RequestTimeAccelDec(bool force = false);

	TimeAccel GetTimeAccel() const { return m_timeAccel; }
	TimeAccel GetRequestedTimeAccel() const { return m_requestedTimeAccel; }
	bool IsPaused() const { return m_timeAccel == TIMEACCEL_PAUSED; }

	float GetTimeAccelRate() const { return s_timeAccelRates[m_timeAccel]; }
	float GetInvTimeAccelRate() const { return s_timeInvAccelRates[m_timeAccel]; }

	float GetTimeStep() const { return s_timeAccelRates[m_timeAccel] * (1.0f / PHYSICS_HZ); }

	GameLog &GetGameLog() const { return *m_log.get(); };

	static void EmitPauseState(bool paused);

private:
	unsigned int m_cacheRadius;

	void GenCaches(const SystemPath *here, unsigned int cacheRadius,
		StarSystemCache::CacheFilledCallback callback = StarSystemCache::CacheFilledCallback());
	void UpdateStarSystemCache(const SystemPath *here, unsigned int cacheRadius);

	RefCountedPtr<SectorCache::Slave> m_sectorCache;
	RefCountedPtr<StarSystemCache::Slave> m_starSystemCache;

	void SwitchToHyperspace();
	void SwitchToNormalSpace();

	RefCountedPtr<Galaxy> m_galaxy;
	std::unique_ptr<Space> m_space;
	double m_time;

	std::unique_ptr<Player> m_player;
	std::unique_ptr<LuaTimer> m_luaTimer;

	std::unique_ptr<GameLog> m_log;

	enum class State {
		NORMAL,
		HYPERSPACE,
	};
	State m_state;

	bool m_wantHyperspace;

	std::list<HyperspaceCloud *> m_hyperspaceClouds;
	SystemPath m_hyperspaceSource;
	SystemPath m_hyperspaceDest;
	double m_hyperspaceProgress;
	double m_hyperspaceDuration;
	double m_hyperspaceEndTime;

	TimeAccel m_timeAccel;
	TimeAccel m_requestedTimeAccel;
	bool m_forceTimeAccel;
	static const float s_timeAccelRates[];
	static const float s_timeInvAccelRates[];
};

#endif
