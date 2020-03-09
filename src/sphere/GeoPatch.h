// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _GEOPATCH_H
#define _GEOPATCH_H

#include "Color.h"
#include "GeoPatchID.h"
#include "JobQueue.h"
#include "libs/RefCounted.h"
#include "libs/matrix4x4.h"
#include "libs/vector3.h"
#include <array>
#include <deque>
#include <memory>

//#define DEBUG_BOUNDING_SPHERES

namespace Graphics {
	class RenderState;
	namespace Drawables {
		class Sphere3D;
	}
}

namespace Graphics {
	class Frustum;
	class VertexBuffer;
}

class GeoPatchContext;
class GeoSphere;
class BasePatchJob;
class SQuadSplitResult;
class SSingleSplitResult;

class GeoPatch {
public:
	GeoPatch(const RefCountedPtr<GeoPatchContext> &_ctx, GeoSphere *gs,
		const vector3d &v0_, const vector3d &v1_, const vector3d &v2_, const vector3d &v3_,
		const int depth, const GeoPatchID &ID_);

	~GeoPatch();

	inline void NeedToUpdateVBOs()
	{
		m_needUpdateVBOs = !m_heights.empty();
	}

	int GetChildIdx(const GeoPatch *child) const
	{
		for (int i = 0; i < NUM_KIDS; i++) {
			if (m_kids[i].get() == child) return i;
		}
		abort();
		return -1;
	}

	// in patch surface coords, [0,1]
	inline vector3d GetSpherePoint(const double x, const double y) const
	{
		return (m_v0 + x * (1.0 - y) * (m_v1 - m_v0) + x * y * (m_v2 - m_v0) + (1.0 - x) * y * (m_v3 - m_v0)).Normalized();
	}

	void Render(const vector3d &campos, const matrix4x4d &modelView, const Graphics::Frustum &frustum);

	inline bool canBeMerged() const
	{
		bool merge = true;
		if (m_kids[0]) {
			for (int i = 0; i < NUM_KIDS; i++) {
				merge &= m_kids[i]->canBeMerged();
			}
		}
		merge &= !(m_HasJobRequest);
		return merge;
	}

	void LODUpdate(const vector3d &campos, const Graphics::Frustum &frustum);

	void RequestSinglePatch();
	void ReceiveHeightmaps(SQuadSplitResult *psr);
	void ReceiveHeightmap(const SSingleSplitResult *psr);
	void ReceiveJobHandle(Job::Handle job);

	inline bool HasHeightData() const { return !m_heights.empty(); }
private:
	void UpdateVBOs();

	static const int NUM_KIDS = 4;

	RefCountedPtr<GeoPatchContext> m_ctx;
	const vector3d m_v0, m_v1, m_v2, m_v3;
	std::vector<double> m_heights;
	std::vector<vector3f> m_normals;
	std::vector<Color3ub> m_colors;
	std::unique_ptr<Graphics::VertexBuffer> m_vertexBuffer;
	std::unique_ptr<GeoPatch> m_kids[NUM_KIDS];
	GeoPatch *m_parent;
	GeoSphere *m_geosphere;
	double m_roughLength;
	vector3d m_clipCentroid, m_centroid;
	double m_clipRadius;
	int32_t m_depth;
	bool m_needUpdateVBOs;

	const GeoPatchID m_PatchID;
	Job::Handle m_job;
	bool m_HasJobRequest;

	std::unique_ptr<Graphics::Drawables::Sphere3D> m_boundsphere;
};

#endif /* _GEOPATCH_H */
