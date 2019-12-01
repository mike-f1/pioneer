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
#include <SDL_events.h>

class Galaxy;
class GameLog;
class HyperspaceCloud;
class Player;
class Space;
class View;

namespace Graphics {
	class Renderer;
}

struct CannotSaveCurrentGameState {};
struct CannotSaveInHyperspace : public CannotSaveCurrentGameState {};
struct CannotSaveDeadPlayer : public CannotSaveCurrentGameState {};
struct InvalidGameStartLocation {
	std::string error;
	InvalidGameStartLocation(const std::string &error_) :
		error(error_) {}
};

class SectorView;
class UIView;
class SystemInfoView;
class SystemView;
class WorldView;
class DeathView;
class ShipCpanel;
class ObjectViewerView;

class Game {
public:
	static Json LoadGameToJson(const std::string &filename);
	// LoadGame and SaveGame throw exceptions on failure
	static Game *LoadGame(const std::string &filename);
	static bool CanLoadGame(const std::string &filename);
	// XXX game arg should be const, and this should probably be a member function
	// (or LoadGame/SaveGame should be somewhere else entirely)
	static void SaveGame(const std::string &filename, Game *game);

	// start docked in station referenced by path or nearby to body if it is no station
	Game(const SystemPath &path, const double startDateTime = 0.0);

	// load game
	Game(const Json &jsonObj);

	~Game();

	// save game
	void ToJson(Json &jsonObj);

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

	enum class ViewType {
		NONE,
		SECTOR,
		GALACTIC,
		SYSTEMINFO,
		SYSTEM,
		WORLD,
		DEATH,
		SPACESTATION,
		INFO,
		OBJECT
	};

	void SetView(ViewType vt);
	View *GetView() { return m_currentView; } // <-- Only for a check on template name in Pi::

	bool IsEmptyView() const { return nullptr == m_currentView; }
	bool IsSectorView() const { return ViewType::SECTOR == m_currentViewType; }
	bool IsGalacticView() const { return ViewType::GALACTIC == m_currentViewType; }
	bool IsSystemInfoView() const { return ViewType::SYSTEMINFO == m_currentViewType; }
	bool IsSystemView() const { return ViewType::SYSTEM == m_currentViewType; }
	bool IsWorldView() const { return ViewType::WORLD == m_currentViewType; }
	bool IsDeathView() const { return ViewType::DEATH == m_currentViewType; }
	bool IsSpaceStationView() const { return ViewType::SPACESTATION == m_currentViewType; }
	bool IsInfoView() const { return ViewType::INFO == m_currentViewType; }
	bool IsObjectView() const { return ViewType::OBJECT == m_currentViewType; }

	SectorView *GetSectorView() const { return m_gameViews->m_sectorView; }
	UIView *GetGalacticView() const { return m_gameViews->m_galacticView; }
	SystemInfoView *GetSystemInfoView() const { return m_gameViews->m_systemInfoView; }
	SystemView *GetSystemView() const { return m_gameViews->m_systemView; }
	WorldView *GetWorldView() const { return m_gameViews->m_worldView; }
	DeathView *GetDeathView() const { return m_gameViews->m_deathView; }
	UIView *GetSpaceStationView() const { return m_gameViews->m_spaceStationView; }
	UIView *GetInfoView() const { return m_gameViews->m_infoView; }
	ShipCpanel *GetCpan() const { return m_gameViews->m_cpan; }

	/* Only use #if WITH_OBJECTVIEWER */
	ObjectViewerView *GetObjectViewerView() const;

	void HandleSDLEvent(SDL_Event event);
	void UpdateView();
	void Draw3DView();

	GameLog *log;

	static void EmitPauseState(bool paused);

private:
	void GenCaches(const SystemPath *here, int cacheRadius,
		StarSystemCache::CacheFilledCallback callback = StarSystemCache::CacheFilledCallback());
	void UpdateStarSystemCache(const SystemPath *here, int cacheRadius);

	RefCountedPtr<SectorCache::Slave> m_sectorCache;
	RefCountedPtr<StarSystemCache::Slave> m_starSystemCache;

	View *m_currentView;
	ViewType m_currentViewType;

	class Views {
	public:
		Views();
		void Init(Game *game, const SystemPath &path);
		void LoadFromJson(const Json &jsonObj, Game *game, const SystemPath &path);
		~Views();

		void SetRenderer(Graphics::Renderer *r);

		SectorView *m_sectorView;
		UIView *m_galacticView;
		SystemInfoView *m_systemInfoView;
		SystemView *m_systemView;
		WorldView *m_worldView;
		DeathView *m_deathView;
		UIView *m_spaceStationView;
		UIView *m_infoView;
		ShipCpanel *m_cpan;

		/* Only use #if WITH_OBJECTVIEWER */
		ObjectViewerView *m_objectViewerView;
	};

	void CreateViews(const SystemPath &path);
	void LoadViewsFromJson(const Json &jsonObj, const SystemPath &path);
	void DestroyViews();

	void SwitchToHyperspace();
	void SwitchToNormalSpace();

	RefCountedPtr<Galaxy> m_galaxy;
	std::unique_ptr<Views> m_gameViews;
	std::unique_ptr<Space> m_space;
	double m_time;

	std::unique_ptr<Player> m_player;

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
