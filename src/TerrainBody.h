// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _TERRAINBODY_H
#define _TERRAINBODY_H

#include "Body.h"
#include "JsonFwd.h"
#include "libs/matrix4x4.h"
#include "galaxy/SystemBodyWrapper.h"

class BaseSphere;
class Camera;
class Frame;
class Space;
class SystemBody;

enum class GSDebugFlags;

namespace Graphics {
	class Renderer;
}

class TerrainBody : public Body, public SystemBodyWrapper {
public:
	OBJDEF(TerrainBody, Body, TERRAINBODY);

	virtual void Render(const Camera *camera, const vector3d &viewCoords, const matrix4x4d &viewTransform) override;
	virtual void SubRender(const matrix4x4d &, const vector3d &) {}
	virtual void SetFrame(FrameId fId) override;
	virtual bool OnCollision(Object *, uint32_t, double) override { return true; }
	virtual double GetMass() const override { return m_mass; }
	double GetTerrainHeight(const vector3d &pos) const;
	virtual const SystemBody *GetSystemBody() const override { return SystemBodyWrapper::GetSystemBody(); }

	// returns value in metres
	double GetMaxFeatureRadius() const { return m_maxFeatureHeight; }

	// implements calls to all relevant terrain management sub-systems
	static void OnChangeDetailLevel(int new_detail);

	void SetDebugFlags(GSDebugFlags flags);
	GSDebugFlags GetDebugFlags();

protected:
	TerrainBody() = delete;
	TerrainBody(SystemBody *);
	TerrainBody(const Json &jsonObj, Space *space);
	virtual ~TerrainBody();

	void InitTerrainBody();

	virtual void SaveToJson(Json &jsonObj, Space *space) override;

private:
	double m_mass;
	std::unique_ptr<BaseSphere> m_baseSphere;
	double m_maxFeatureHeight;
};

#endif
