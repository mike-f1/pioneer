// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _MODELBODY_H
#define _MODELBODY_H

#include "Body.h"
#include "CollMesh.h"
#include "FrameId.h"

class Shields;
class Geom;
class Camera;

class CSG_CentralCylinder;
class CSG_Box;

namespace Graphics {
	class Light;
} // namespace Graphics

namespace SceneGraph {
	class Model;
	class Animation;
} // namespace SceneGraph

class ModelBody : public Body {
public:
	OBJDEF(ModelBody, Body, MODELBODY);
	ModelBody();
	ModelBody(const Json &jsonObj, Space *space);
	virtual ~ModelBody();
	void SetPosition(const vector3d &p) override;
	void SetOrient(const matrix3x3d &r) override;
	virtual void SetFrame(FrameId fId) override;
	// Colliding: geoms are checked against collision space
	void SetColliding(bool colliding);
	bool IsColliding() const { return m_colliding; }
	// Static: geoms are static relative to frame
	void SetStatic(bool isStatic);
	bool IsStatic() const { return m_isStatic; }
	const Aabb &GetAabb() const { return m_collMesh->GetAabb(); }
	SceneGraph::Model *GetModel() const { return m_model.get(); }
	CollMesh *GetCollMesh() { return m_collMesh.Get(); }
	Geom *GetGeom() const { return m_geom.get(); }

	void SetCentralCylinder(std::unique_ptr<CSG_CentralCylinder> centralcylinder);
	void AddBox(std::unique_ptr<CSG_Box> box);

	void SetModel(const char *modelName);

	void RenderModel(const Camera *camera, const vector3d &viewCoords, const matrix4x4d &viewTransform, const bool setLighting = true);

	virtual void TimeStepUpdate(const float timeStep) override;

protected:
	virtual void SaveToJson(Json &jsonObj, Space *space) override;

	void SetLighting(const Camera *camera, std::vector<Graphics::Light> &oldLights, Color &oldAmbient);
	void ResetLighting(const std::vector<Graphics::Light> &oldLights, const Color &oldAmbient);

	Shields *GetShields() const { return m_shields.get(); }

private:
	void RebuildCollisionMesh();
	void DeleteGeoms();
	void AddGeomsToFrame(Frame *);
	void RemoveGeomsFromFrame(Frame *);
	void MoveGeoms(const matrix4x4d &, const vector3d &);

	void CalcLighting(double &ambient, double &direct, const Camera *camera);

	bool m_isStatic;
	bool m_colliding;
	RefCountedPtr<CollMesh> m_collMesh;
	std::unique_ptr<Geom> m_geom; //static geom
	std::string m_modelName;
	std::unique_ptr<SceneGraph::Model> m_model;
	std::vector<std::unique_ptr<Geom>> m_dynGeoms;
	SceneGraph::Animation *m_idleAnimation;
	std::unique_ptr<Shields> m_shields;
};

#endif /* _MODELBODY_H */
