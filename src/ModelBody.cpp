// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "ModelBody.h"

#include "Aabb.h"
#include "Camera.h"
#include "CollMesh.h"
#include "Frame.h"
#include "GameSaveError.h"
#include "Json.h"
#include "ModelCache.h"
#include "Planet.h"
#include "Shields.h"
#include "collider/CollisionSpace.h"
#include "collider/Geom.h"
#include "graphics/Renderer.h"
#include "graphics/RendererLocator.h"
#include "scenegraph/Animation.h"
#include "scenegraph/CollisionGeometry.h"
#include "scenegraph/DynCollisionVisitor.h"
#include "scenegraph/MatrixTransform.h"
#include "scenegraph/Model.h"
#include "libs/gameconsts.h"
#include "libs/utils.h"

class Space;

ModelBody::ModelBody() :
	m_isStatic(false),
	m_colliding(true),
	m_idleAnimation(nullptr)
{}

ModelBody::ModelBody(const Json &jsonObj, Space *space) :
	Body(jsonObj, space)
{
	Json modelBodyObj = jsonObj["model_body"];

	try {
		m_isStatic = modelBodyObj["is_static"];
		m_colliding = modelBodyObj["is_colliding"];
		SetModel(modelBodyObj["model_name"].get<std::string>().c_str());
	} catch (Json::type_error &) {
		Output("Loading error in '%s' in function '%s' \n", __FILE__, __func__);
		throw SavedGameCorruptException();
	}

	m_model->LoadFromJson(modelBodyObj);
	m_shields->LoadFromJson(modelBodyObj);
}

ModelBody::~ModelBody()
{
	SetFrame(FrameId::Invalid); // Will remove geom from frame if necessary.
}

Json ModelBody::SaveToJson(Space *space) const
{
	Json jsonObj = Body::SaveToJson(space);

	Json modelBodyObj = Json::object(); // Create JSON object to contain model body data.

	modelBodyObj["is_static"] = m_isStatic;
	modelBodyObj["is_colliding"] = m_colliding;
	//TODO: is this needed? Model is saved in below line...
	modelBodyObj["model_name"] = m_model->GetName();
	m_model->SaveToJson(modelBodyObj);
	m_shields->SaveToJson(modelBodyObj);

	jsonObj["model_body"] = modelBodyObj; // Add model body object to supplied object.
	return jsonObj;
}

void ModelBody::SetStatic(bool isStatic)
{
	if (isStatic == m_isStatic) return;
	m_isStatic = isStatic;
	if (!m_geom) return;

	Frame *f = Frame::GetFrame(GetFrame());
	if (m_isStatic) {
		f->RemoveGeom(m_geom.get());
		f->AddStaticGeom(m_geom.get());
	} else {
		f->RemoveStaticGeom(m_geom.get());
		f->AddGeom(m_geom.get());
	}
}

void ModelBody::SetColliding(bool colliding)
{
	m_colliding = colliding;
	if (colliding)
		m_geom->Enable();
	else
		m_geom->Disable();
}

const Aabb &ModelBody::GetAabb() const
{
	return m_model->GetCollisionMesh()->GetAabb();
}

float ModelBody::GetCollMeshRadius() const
{
	return m_model->GetCollisionMesh()->GetRadius();
}

void ModelBody::RebuildCollisionMesh()
{
	Frame *f = Frame::GetFrame(GetFrame());
	if (m_geom) {
		if (f) RemoveGeomsFromFrame(f);
		m_dynGeoms.clear();
	}

	RefCountedPtr<CollMesh> collMesh = m_model->GetCollisionMesh();
	double maxRadius = collMesh->GetAabb().GetRadius();

	//static geom
	m_geom = std::make_unique<Geom>(collMesh->GetGeomTree(), GetOrient(), GetPosition(), this);

	SetPhysRadius(maxRadius);

	//have to figure out which collision geometries are responsible for which geomtrees
	SceneGraph::DynGeomFinder dgf;
	m_model->GetRoot()->Accept(dgf);

	//dynamic geoms
	for (auto &dgt : collMesh->GetDynGeomTrees()) {
		m_dynGeoms.emplace_back(std::make_unique<Geom>(dgt.second, GetOrient(), GetPosition(), this));
//		auto &dynG = m_dynGeoms.back();
//		dynG->m_animTransform = matrix4x4d::Identity();
//		SceneGraph::CollisionGeometry *cg = dgf.FindCgForTree(dgt);
//		if (cg)
//			cg->SetGeom(dynG.get());
//		else throw std::runtime_error { "Collision geometry not found" };
	}

	if (f) AddGeomsToFrame(f);
}

void ModelBody::SetCentralCylinder(std::unique_ptr<CSG_CentralCylinder> centralcylinder)
{
	// Copy CSG_CentralCylinder: first one 'sink' in model for debugging
	// purposes, while second 'sink' in Geoms for actual checks
	std::unique_ptr<CSG_CentralCylinder> cc2 = std::make_unique<CSG_CentralCylinder>(*centralcylinder.get());
	m_model->SetCentralCylinder(std::move(cc2));
	m_geom->SetCentralCylinder(std::move(centralcylinder));
}

void ModelBody::AddBox(std::unique_ptr<CSG_Box> box)
{
	// Copy CSG_Box: first one 'sink' in model for debugging
	// purposes, while second 'sink' in Geoms for actual checks
	std::unique_ptr<CSG_Box> box2 = std::make_unique<CSG_Box>(*box.get());
	m_geom->AddBox(std::move(box2));
	m_model->AddBox(std::move(box));
}

void ModelBody::SetModel(const char *modelName)
{
	//remove old instance
	m_model.reset();

	//create model instance (some modelbodies, like missiles could avoid this)
	m_model = ModelCache::FindModel(modelName)->MakeInstance();
	if (!m_model) throw std::runtime_error { std::string("Cannot find model ") + modelName };
	m_idleAnimation = m_model->FindAnimation("idle");

	SetClipRadius(m_model->GetDrawClipRadius());

	m_shields = std::make_unique<Shields>(m_model.get());

	RebuildCollisionMesh();
}

void ModelBody::SetPosition(const vector3d &p)
{
	Body::SetPosition(p);
	MoveGeoms(GetOrient(), p);
}

void ModelBody::SetOrient(const matrix3x3d &m)
{
	Body::SetOrient(m);
	const matrix4x4d m2 = m;
	MoveGeoms(m2, GetPosition());
}

void ModelBody::SetFrame(FrameId fId)
{
	if (fId == GetFrame()) return;

	//remove collision geoms from old frame
	Frame *f = Frame::GetFrame(GetFrame());
	if (f) RemoveGeomsFromFrame(f);

	Body::SetFrame(fId);

	//add collision geoms to new frame
	f = Frame::GetFrame(GetFrame());
	if (f) AddGeomsToFrame(f);
}

void ModelBody::AddGeomsToFrame(Frame *f)
{
	const int group = f->GetCollisionSpace()->GetGroupHandle();

	m_geom->SetGroup(group);

	if (m_isStatic) {
		f->AddStaticGeom(m_geom.get());
	} else {
		f->AddGeom(m_geom.get());
	}

	for (auto it = m_dynGeoms.begin(); it != m_dynGeoms.end(); ++it) {
		(*it)->SetGroup(group);
		f->AddGeom((*it).get());
	}
}

void ModelBody::RemoveGeomsFromFrame(Frame *f)
{
	if (f == nullptr) return;

	if (m_isStatic) {
		f->RemoveStaticGeom(m_geom.get());
	} else {
		f->RemoveGeom(m_geom.get());
	}

	for (auto it = m_dynGeoms.begin(); it != m_dynGeoms.end(); ++it)
		f->RemoveGeom((*it).get());
}

void ModelBody::MoveGeoms(const matrix4x4d &m, const vector3d &p)
{
	PROFILE_SCOPED()

	m_geom->MoveTo(m, p);

	//accumulate transforms to animated positions
	if (!m_dynGeoms.empty()) {
		SceneGraph::DynCollUpdateVisitor dcv;
		m_model->GetRoot()->Accept(dcv);
	}

	for (auto &dg : m_dynGeoms) {
		//combine orient & pos
		static matrix4x4d s_tempMat;
		for (unsigned i = 0; i < 12; i++)
			s_tempMat[i] = m[i];
		s_tempMat[12] = p.x;
		s_tempMat[13] = p.y;
		s_tempMat[14] = p.z;
		s_tempMat[15] = m[15];

		dg->MoveTo(s_tempMat * dg->m_animTransform);
	}
}

// Calculates the ambiently and directly lit portions of the lighting model taking into account the atmosphere and sun positions at a given location
// 1. Calculates the amount of direct illumination available taking into account
//    * multiple suns
//    * sun positions relative to up direction i.e. light is dimmed as suns set
//    * Thickness of the atmosphere overhead i.e. as atmospheres get thicker light starts dimming earlier as sun sets, without atmosphere the light switches off at point of sunset
// 2. Calculates the split between ambient and directly lit portions taking into account
//    * Atmosphere density (optical thickness) of the sky dome overhead
//        as optical thickness increases the fraction of ambient light increases
//        this takes altitude into account automatically
//    * As suns set the split is biased towards ambient
void ModelBody::CalcLighting(double &ambient, double &direct, const Camera *camera)
{
	const double minAmbient = 0.05;
	ambient = minAmbient;
	direct = 1.0;
	Body *astro = Frame::GetFrame(GetFrame())->GetBody();
	if (!(astro && astro->IsType(Object::PLANET)))
		return;

	Planet *planet = static_cast<Planet *>(astro);

	// position relative to the rotating frame of the planet
	vector3d upDir = GetInterpPositionRelTo(planet->GetFrame());
	const double planetRadius = planet->GetSystemBodyRadius();
	const double dist = std::max(planetRadius, upDir.Length());
	upDir = upDir.Normalized();

	double pressure, density;
	planet->GetAtmosphericState(dist, &pressure, &density);
	double surfaceDensity;
	Color cl;
	planet->GetSystemBodyAtmosphereFlavor(cl, surfaceDensity);

	// approximate optical thickness fraction as fraction of density remaining relative to earths
	double opticalThicknessFraction = density / EARTH_ATMOSPHERE_SURFACE_DENSITY;

	// tweak optical thickness curve - lower exponent ==> higher altitude before ambient level drops
	// Commenting this out, since it leads to a sharp transition at
	// atmosphereRadius, where density is suddenly 0
	//opticalThicknessFraction = pow(std::max(0.00001,opticalThicknessFraction),0.15); //max needed to avoid 0^power

	if (opticalThicknessFraction < 0.0001)
		return;

	//step through all the lights and calculate contributions taking into account sun position
	double light = 0.0;
	double light_clamped = 0.0;

	const std::vector<Camera::LightSource> &lightSources = camera->GetLightSources();
	for (std::vector<Camera::LightSource>::const_iterator l = lightSources.begin();
		 l != lightSources.end(); ++l) {
		double sunAngle;
		// calculate the extent the sun is towards zenith
		if (l->GetBody()) {
			// relative to the rotating frame of the planet
			const vector3d lightDir = (l->GetBody()->GetInterpPositionRelTo(planet->GetFrame()).Normalized());
			sunAngle = lightDir.Dot(upDir);
		} else {
			// light is the default light for systems without lights
			sunAngle = 1.0;
		}

		const double critAngle = -sqrt(dist * dist - planetRadius * planetRadius) / dist;

		//0 to 1 as sunangle goes from critAngle to 1.0
		double sunAngle2 = (std::clamp(sunAngle, critAngle, 1.0) - critAngle) / (1.0 - critAngle);

		// angle at which light begins to fade on Earth
		const double surfaceStartAngle = 0.3;
		// angle at which sun set completes, which should be after sun has dipped below the horizon on Earth
		const double surfaceEndAngle = -0.18;

		const double start = std::min((surfaceStartAngle * opticalThicknessFraction), 1.0);
		const double end = std::max((surfaceEndAngle * opticalThicknessFraction), -0.2);

		sunAngle = (std::clamp(sunAngle - critAngle, end, start) - end) / (start - end);

		light += sunAngle;
		light_clamped += sunAngle2;
	}

	light_clamped /= lightSources.size();
	light /= lightSources.size();

	// brightness depends on optical depth and intensity of light from all the stars
	direct = 1.0 - std::clamp((1.0 - light), 0.0, 1.0) * std::clamp(opticalThicknessFraction, 0.0, 1.0);

	// ambient light fraction
	// alter ratio between directly and ambiently lit portions towards ambiently lit as sun sets
	const double fraction = (0.2 + 0.8 * (1.0 - light_clamped)) * std::clamp(opticalThicknessFraction, 0.0, 1.0);

	// fraction of light left over to be lit directly
	direct = (1.0 - fraction) * direct;

	// scale ambient by amount of light
	ambient = fraction * (std::clamp((light), 0.0, 1.0)) * 0.25;

	ambient = std::max(minAmbient, ambient);
}

// setLighting: set renderer lights according to current position and sun
// positions. Original lighting is passed back in oldLights, oldAmbient, and
// should be reset after rendering with ModelBody::ResetLighting.
void ModelBody::SetLighting(const Camera *camera, std::vector<Graphics::Light> &oldLights, Color &oldAmbient)
{
	std::vector<Graphics::Light> newLights;
	double ambient, direct;
	CalcLighting(ambient, direct, camera);
	const std::vector<Camera::LightSource> &lightSources = camera->GetLightSources();
	newLights.reserve(lightSources.size());
	oldLights.reserve(lightSources.size());
	for (size_t i = 0; i < lightSources.size(); i++) {
		Graphics::Light light(lightSources[i].GetLight());

		oldLights.push_back(light);

		const float intensity = direct * camera->ShadowedIntensity(i, this);

		Color c = light.GetDiffuse();
		Color cs = light.GetSpecular();
		c.r *= float(intensity);
		c.g *= float(intensity);
		c.b *= float(intensity);
		cs.r *= float(intensity);
		cs.g *= float(intensity);
		cs.b *= float(intensity);
		light.SetDiffuse(c);
		light.SetSpecular(cs);

		newLights.push_back(light);
	}

	if (newLights.empty()) {
		// no lights means we're somewhere weird (eg hyperspace, ObjectViewer). fake one
		newLights.push_back(Graphics::Light(Graphics::Light::LIGHT_DIRECTIONAL, vector3f(0.f), Color::WHITE, Color::WHITE));
	}

	oldAmbient = RendererLocator::getRenderer()->GetAmbientColor();
	RendererLocator::getRenderer()->SetAmbientColor(Color(ambient * 255, ambient * 255, ambient * 255));
	RendererLocator::getRenderer()->SetLights(newLights.size(), &newLights[0]);
}

void ModelBody::ResetLighting(const std::vector<Graphics::Light> &oldLights, const Color &oldAmbient)
{
	// restore old lights
	if (!oldLights.empty())
		RendererLocator::getRenderer()->SetLights(oldLights.size(), &oldLights[0]);
	RendererLocator::getRenderer()->SetAmbientColor(oldAmbient);
}

void ModelBody::RenderModel(const Camera *camera, const vector3d &viewCoords, const matrix4x4d &viewTransform, const bool setLighting)
{
	PROFILE_SCOPED()

	std::vector<Graphics::Light> oldLights;
	Color oldAmbient;
	if (setLighting)
		SetLighting(camera, oldLights, oldAmbient);

	matrix4x4d m2 = GetInterpOrient();
	m2.SetTranslate(GetInterpPosition());
	matrix4x4d t = viewTransform * m2;

	//double to float matrix
	matrix4x4f trans;
	for (int i = 0; i < 12; i++)
		trans[i] = float(t[i]);
	trans[12] = viewCoords.x;
	trans[13] = viewCoords.y;
	trans[14] = viewCoords.z;
	trans[15] = 1.0f;

	m_model->Render(trans);

	if (setLighting)
		ResetLighting(oldLights, oldAmbient);
}

void ModelBody::TimeStepUpdate(const float timestep)
{
	PROFILE_SCOPED()

	if (m_idleAnimation)
		// step animation by timestep/total length, loop to 0.0 if it goes >= 1.0
		m_idleAnimation->SetProgress(fmod(m_idleAnimation->GetProgress() + timestep / m_idleAnimation->GetDuration(), 1.0));

	m_model->UpdateAnimations();
}
