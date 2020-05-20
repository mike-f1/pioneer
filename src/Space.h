// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _SPACE_H
#define _SPACE_H

#include "FrameId.h"
#include "JsonFwd.h"
#include "Object.h"
#include "libs/IterationProxy.h"
#include "libs/RefCounted.h"
#include "libs/vector3.h"
#include <list>
#include <memory>

class Body;
class Frame;
class StarSystem;
class SystemBody;
class SystemPath;

namespace Background {
	class Container;
}

class Space {
public:
	// empty space (eg for hyperspace)
	Space();

	// initalise with system bodies
	Space(double total_time, float time_step, RefCountedPtr<StarSystem> starsystem, const SystemPath &path);

	// initialise from save file
	Space(RefCountedPtr<StarSystem> starsystem, const Json &jsonObj, double at_time);

	~Space();

	void ToJson(Json &jsonObj);

	// body/sbody indexing for save/load. valid after
	// construction/ToJson(), invalidated by TimeStep(). they will assert
	// if called while invalid
	Body *GetBodyByIndex(uint32_t idx) const;
	SystemBody *GetSystemBodyByIndex(uint32_t idx) const;
	uint32_t GetIndexForBody(const Body *body) const;
	uint32_t GetIndexForSystemBody(const SystemBody *sbody) const;

	RefCountedPtr<StarSystem> GetStarSystem() const;

	void AddBody(Body *);
	void RemoveBody(Body *);
	void KillBody(Body *);

	void TimeStep(float step, double total_time);

	void GetRandomOrbitFromDirection(const SystemPath &source, const SystemPath &dest,
		const vector3d &dir, vector3d &pos, vector3d &vel) const;

	Body *FindNearestTo(const Body *b, Object::Type t) const;
	Body *FindBodyForPath(const SystemPath *path) const;

	uint32_t GetNumBodies() const { return static_cast<uint32_t>(m_bodies.size()); }
	IterationProxy<std::list<Body *>> GetBodies() { return MakeIterationProxy(m_bodies); }
	const IterationProxy<const std::list<Body *>> GetBodies() const { return MakeIterationProxy(m_bodies); }

	Background::Container *GetBackground() { return m_background.get(); }
	void RefreshBackground();

	// body finder delegates
	typedef const std::vector<Body *> BodyNearList;
	BodyNearList GetBodiesMaybeNear(const Body *b, double dist)
	{
		return std::move(m_bodyNearFinder.GetBodiesMaybeNear(b, dist));
	}
	BodyNearList GetBodiesMaybeNear(const vector3d &pos, double dist)
	{
		return std::move(m_bodyNearFinder.GetBodiesMaybeNear(pos, dist));
	}

	void DebugDumpFrames(bool details);
private:
	void GenBody(const double at_time, SystemBody *b, FrameId fId, std::vector<vector3d> &posAccum);

	void UpdateBodies();

	FrameId m_rootFrameId;

	RefCountedPtr<StarSystem> m_starSystem;

	// all the bodies we know about
	std::list<Body *> m_bodies;

	// bodies that were removed/killed this timestep and need pruning at the end
	std::list<Body *> m_removeBodies;
	std::list<Body *> m_killBodies;

	void RebuildBodyIndex();
	void RebuildSystemBodyIndex();

	void AddSystemBodyToIndex(SystemBody *sbody);

	bool m_bodyIndexValid, m_sbodyIndexValid;
	std::vector<Body *> m_bodyIndex;
	std::vector<SystemBody *> m_sbodyIndex;

	//background (elements that are infinitely far away,
	//e.g. starfield and milky way)
	std::unique_ptr<Background::Container> m_background;

	class BodyNearFinder {
	public:
		BodyNearFinder(const Space *space) :
			m_space(space) {}
		void Prepare();

		BodyNearList GetBodiesMaybeNear(const Body *b, double dist);
		BodyNearList GetBodiesMaybeNear(const vector3d &pos, double dist);

	private:
		struct BodyDist {
			BodyDist(Body *_body, double _dist) :
				body(_body),
				dist(_dist) {}
			Body *body;
			double dist;

			bool operator<(const BodyDist &a) const { return dist < a.dist; }

			friend bool operator<(const BodyDist &a, double d) { return a.dist < d; }
			friend bool operator<(double d, const BodyDist &a) { return d < a.dist; }
		};

		const Space *m_space;
		std::vector<BodyDist> m_bodyDist;
		std::vector<Body *> m_nearBodies;
	};

	BodyNearFinder m_bodyNearFinder;

#ifndef NDEBUG
	//to check RemoveBody and KillBody are not called from within
	//the NotifyRemoved callback (#735)
	bool m_processingFinalizationQueue;
#endif

};

#endif /* _SPACE_H */
