// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Camera.h"

#include "Background.h"
#include "Body.h"
#include "Frame.h"
#include "Game.h"
#include "GameLocator.h"
#include "Planet.h"
#include "Player.h"
#include "Sfx.h"
#include "Space.h"
#include "ShipCockpit.h"
#include "galaxy/GalaxyEnums.h"
#include "galaxy/SystemBody.h"
#include "graphics/Material.h"
#include "graphics/Renderer.h"
#include "graphics/RendererLocator.h"
#include "graphics/Texture.h"
#include "graphics/TextureBuilder.h"

using namespace Graphics;

// size of reserved space for shadows vector
constexpr unsigned STD_SHADOWS_SIZE = 16;
// if a body would render smaller than this many pixels, just ignore it
static const float OBJECT_HIDDEN_PIXEL_THRESHOLD = 2.0f;

// if a terrain object would render smaller than this many pixels, draw a billboard instead
static const float BILLBOARD_PIXEL_THRESHOLD = 8.0f;

CameraContext::CameraContext(float width, float height, float fovAng, float zNear, float zFar) :
	m_width(width),
	m_height(height),
	m_fovAng(fovAng),
	m_zNear(zNear),
	m_zFar(zFar),
	m_frustum(m_width, m_height, m_fovAng, m_zNear, m_zFar),
	m_frame(FrameId::Invalid),
	m_pos(0.0),
	m_orient(matrix3x3d::Identity()),
	m_camFrame(FrameId::Invalid)
{
}

CameraContext::~CameraContext()
{
	if (m_camFrame)
		EndFrame();
}

void CameraContext::BeginFrame()
{
	assert(m_frame.valid());
	assert(!m_camFrame.valid());

	// make temporary camera frame
	m_camFrame = Frame::CreateCameraFrame(m_frame);

	Frame *camFrame = Frame::GetFrame(m_camFrame);
	// move and orient it to the camera position
	camFrame->SetOrient(m_orient, GameLocator::getGame() ? GameLocator::getGame()->GetTime() : 0.0);
	camFrame->SetPosition(m_pos);

	// make sure old orient and interpolated orient (rendering orient) are not rubbish
	camFrame->ClearMovement();
	camFrame->UpdateInterpTransform(1.0); // update root-relative pos/orient
}

void CameraContext::EndFrame()
{
	assert(m_frame.valid());
	assert(m_camFrame.valid());

	Frame::DeleteCameraFrame(m_camFrame);

	m_camFrame = FrameId::Invalid;
}

void CameraContext::ApplyDrawTransforms()
{
	RendererLocator::getRenderer()->SetPerspectiveProjection(m_fovAng, m_width / m_height, m_zNear, m_zFar);
	RendererLocator::getRenderer()->SetTransform(matrix4x4f::Identity());
}

bool Camera::BodyAttrs::sort_BodyAttrs(const BodyAttrs &a, const BodyAttrs &b)
{
	// both drawing last; distance order
	if (a.bodyFlags & Body::FLAG_DRAW_LAST && b.bodyFlags & Body::FLAG_DRAW_LAST)
		return a.camDist > b.camDist;

	// a drawing last; draw b first
	if (a.bodyFlags & Body::FLAG_DRAW_LAST)
		return false;

	// b drawing last; draw a first
	if (b.bodyFlags & Body::FLAG_DRAW_LAST)
		return true;

	// both in normal draw; distance order
	return a.camDist > b.camDist;
}

Camera::Camera(RefCountedPtr<CameraContext> context) :
	m_context(context)
{
	Graphics::MaterialDescriptor desc;
	desc.effect = Graphics::EffectType::BILLBOARD;
	desc.textures = 1;

	m_billboardMaterial.reset(RendererLocator::getRenderer()->CreateMaterial(desc));
	m_billboardMaterial->texture0 = Graphics::TextureBuilder::Billboard("textures/planet_billboard.dds").GetOrCreateTexture(RendererLocator::getRenderer(), "billboard");
}

Camera::~Camera()
{}

static void position_system_lights(Frame *camFrame, Frame *frame, std::vector<Camera::LightSource> &lights)
{
	PROFILE_SCOPED()
	if (lights.size() > 3) return;

	SystemBody *body = frame->GetSystemBody();
	// IsRotFrame check prevents double counting
	if (body && !frame->IsRotFrame() && (body->GetSuperType() == GalaxyEnums::BodySuperType::SUPERTYPE_STAR)) {
		vector3d lpos = frame->GetPositionRelTo(camFrame->GetId());
		const double dist = lpos.Length() / AU;
		lpos *= 1.0 / dist; // normalize

		const Color &col = GalaxyEnums::starRealColors[body->GetType()];

		const Color lightCol(col[0], col[1], col[2], 0);
		vector3f lightpos(lpos.x, lpos.y, lpos.z);
		Graphics::Light light(Graphics::Light::LIGHT_DIRECTIONAL, lightpos, lightCol, lightCol);
		lights.push_back(Camera::LightSource(frame->GetBody(), light));
	}

	for (FrameId kid : frame->GetChildren()) {
		Frame *kid_f = Frame::GetFrame(kid);
		position_system_lights(camFrame, kid_f, lights);
	}
}

void Camera::Update()
{
	FrameId camFrame = m_context->GetCamFrame();

	// evaluate each body and determine if/where/how to draw it
	m_sortedBodies.clear();
	for (Body *body : GameLocator::getGame()->GetSpace()->GetBodies()) {
		BodyAttrs attrs;
		attrs.body = body;
		attrs.billboard = false; // false by default

		// determine position and transform for draw
		//		Frame::GetFrameTransform(b->GetFrame(), camFrame, attrs.viewTransform);		// doesn't use interp coords, so breaks in some cases
		Frame *f = Frame::GetFrame(body->GetFrame());
		attrs.viewTransform = f->GetInterpOrientRelTo(camFrame);
		attrs.viewTransform.SetTranslate(f->GetInterpPositionRelTo(camFrame));
		attrs.viewCoords = attrs.viewTransform * body->GetInterpPosition();

		// cull off-screen objects
		double rad = body->GetClipRadius();
		if (!m_context->GetFrustum().TestPointInfinite(attrs.viewCoords, rad))
			continue;

		attrs.camDist = attrs.viewCoords.Length();
		attrs.bodyFlags = body->GetFlags();

		// approximate pixel width (disc diameter) of body on screen
		const float pixSize = Graphics::GetScreenHeight() * 2.0 * rad / (attrs.camDist * Graphics::GetFovFactor());

		// terrain objects are visible from distance but might not have any discernable features
		if (body->IsType(Object::TERRAINBODY)) {
			if (pixSize < BILLBOARD_PIXEL_THRESHOLD) {
				attrs.billboard = true;

				// project the position
				vector3d pos = m_context->GetFrustum().TranslatePoint(attrs.viewCoords);
				attrs.billboardPos = vector3f(pos);

				// limit the minimum billboard size for planets so they're always a little visible
				attrs.billboardSize = std::max(1.0f, pixSize);
				if (body->IsType(Object::STAR)) {
					attrs.billboardColor = GalaxyEnums::starRealColors[body->GetSystemBody()->GetType()];
				} else if (body->IsType(Object::PLANET)) {
					// XXX this should incorporate some lighting effect
					// (ie, colour of the illuminating star(s))
					attrs.billboardColor = body->GetSystemBody()->GetAlbedo();
				} else {
					attrs.billboardColor = Color::WHITE;
				}

				// this should always be the main star in the system - except for the star itself!
				if (!m_lightSources.empty() && !body->IsType(Object::STAR)) {
					const Graphics::Light &light = m_lightSources[0].GetLight();
					attrs.billboardColor *= light.GetDiffuse(); // colour the billboard a little with the Starlight
				}

				attrs.billboardColor.a = 255; // no alpha, these things are hard enough to see as it is
			}
		} else if (pixSize < OBJECT_HIDDEN_PIXEL_THRESHOLD) {
			continue;
		}

		m_sortedBodies.push_back(attrs);
	}

	// depth sort
	m_sortedBodies.sort();
}

void Camera::Draw(const Body *excludeBody, ShipCockpit *cockpit)
{
	PROFILE_SCOPED()

	FrameId camFrameId = m_context->GetCamFrame();
	FrameId rootFrameId = Frame::GetRootFrameId();

	Frame *camFrame = Frame::GetFrame(camFrameId);
	Frame *rootFrame = Frame::GetRootFrame();

	RendererLocator::getRenderer()->ClearScreen();

	matrix4x4d trans2bg = Frame::GetFrameTransform(rootFrameId, camFrameId);
	trans2bg.ClearToRotOnly();

	// Pick up to four suitable system light sources (stars)
	m_lightSources.clear();
	m_lightSources.reserve(4);
	position_system_lights(camFrame, rootFrame, m_lightSources);

	if (m_lightSources.empty()) {
		// no lights means we're somewhere weird (eg hyperspace). fake one
		Graphics::Light light(Graphics::Light::LIGHT_DIRECTIONAL, vector3f(0.f), Color::WHITE, Color::WHITE);
		m_lightSources.push_back(LightSource(0, light));
	}

	//fade space background based on atmosphere thickness and light angle
	float bgIntensity = 1.f;
	Frame *camParent = Frame::GetFrame(camFrame->GetParent());
	if (camParent && camParent->IsRotFrame()) {
		//check if camera is near a planet
		Body *camParentBody = camParent->GetBody();
		if (camParentBody && camParentBody->IsType(Object::PLANET)) {
			Planet *planet = static_cast<Planet *>(camParentBody);
			const vector3f relpos(planet->GetInterpPositionRelTo(camFrameId));
			double altitude(relpos.Length());
			double pressure, density;
			planet->GetAtmosphericState(altitude, &pressure, &density);
			if (pressure >= 0.001) {
				//go through all lights to calculate something resembling light intensity
				float intensity = 0.f;
				const Body *pBody = GameLocator::getGame()->GetPlayer();
				for (unsigned i = 0; i < m_lightSources.size(); i++) {
					// Set up data for eclipses. All bodies are assumed to be spheres.
					const LightSource &it = m_lightSources[i];
					const vector3f lightDir(it.GetLight().GetPosition().Normalized());
					intensity += ShadowedIntensity(i, pBody) * std::max(0.f, lightDir.Dot(-relpos.Normalized())) * (it.GetLight().GetDiffuse().GetLuminance() / 255.0f);
				}
				intensity = Clamp(intensity, 0.0f, 1.0f);

				//calculate background intensity with some hand-tweaked fuzz applied
				bgIntensity = Clamp(1.f - std::min(1.f, powf(density, 0.25f)) * (0.3f + powf(intensity, 0.25f)), 0.f, 1.f);
			}
		}
	}

	GameLocator::getGame()->GetSpace()->GetBackground()->SetIntensity(bgIntensity);
	GameLocator::getGame()->GetSpace()->GetBackground()->Draw(trans2bg);

	{
		std::vector<Graphics::Light> rendererLights;
		rendererLights.reserve(m_lightSources.size());
		for (size_t i = 0; i < m_lightSources.size(); i++)
			rendererLights.push_back(m_lightSources[i].GetLight());
		RendererLocator::getRenderer()->SetLights(rendererLights.size(), &rendererLights[0]);
	}

	for (std::list<BodyAttrs>::iterator i = m_sortedBodies.begin(); i != m_sortedBodies.end(); ++i) {
		BodyAttrs *attrs = &(*i);

		// explicitly exclude a single body if specified (eg player)
		if (attrs->body == excludeBody)
			continue;

		// draw something!
		if (attrs->billboard) {
			Graphics::Renderer::MatrixTicket mt(RendererLocator::getRenderer(), Graphics::MatrixMode::MODELVIEW);
			RendererLocator::getRenderer()->SetTransform(matrix4x4d::Identity());
			m_billboardMaterial->diffuse = attrs->billboardColor;
			RendererLocator::getRenderer()->DrawPointSprites(1, &attrs->billboardPos, SfxManager::additiveAlphaState, m_billboardMaterial.get(), attrs->billboardSize);
		} else
			attrs->body->Render(this, attrs->viewCoords, attrs->viewTransform);
	}

	SfxManager::RenderAll(rootFrameId, camFrameId);

	// NB: Do any screen space rendering after here:
	// Things like the cockpit and AR features like hudtrails, space dust etc.

	// Render cockpit
	// XXX only here because it needs a frame for lighting calc
	// should really be in WorldView, immediately after camera draw
	if (cockpit)
		cockpit->RenderCockpit(this, camFrameId);
}

 const std::vector<Camera::Shadow> Camera::CalcShadows(const int lightNum, const Body *b) const
{
	 std::vector<Shadow> shadowsOut;
	 shadowsOut.reserve(STD_SHADOWS_SIZE);
	// Set up data for eclipses. All bodies are assumed to be spheres.
	const Body *lightBody = m_lightSources[lightNum].GetBody();
	if (!lightBody)
		return {};

	const double lightRadius = lightBody->GetPhysRadius();
	const vector3d bLightPos = lightBody->GetPositionRelTo(b);
	const vector3d lightDir = bLightPos.Normalized();

	double bRadius;
	if (b->IsType(Object::TERRAINBODY))
		bRadius = b->GetSystemBody()->GetRadius();
	else
		bRadius = b->GetPhysRadius();

	// Look for eclipsing third bodies:
	for (const Body *b2 : GameLocator::getGame()->GetSpace()->GetBodies()) {
		if (b2 == b || b2 == lightBody || !(b2->IsType(Object::PLANET) || b2->IsType(Object::STAR)))
			continue;

		double b2Radius = b2->GetSystemBody()->GetRadius();
		vector3d b2pos = b2->GetPositionRelTo(b);
		const double perpDist = lightDir.Dot(b2pos);

		if (perpDist <= 0 || perpDist > bLightPos.Length())
			// b2 isn't between b and lightBody; no eclipse
			continue;

		// Project to the plane perpendicular to lightDir, taking the line between the shadowed sphere
		// (b) and the light source as zero. Our calculations assume that the light source is at
		// infinity. All lengths are normalised such that b has radius 1. srad is then the radius of the
		// occulting sphere (b2), and lrad is the apparent radius of the light disc when considered to
		// be at the distance of b2, and projectedCentre is the normalised projected position of the
		// centre of b2 relative to the centre of b. The upshot is that from a point on b, with
		// normalised projected position p, the picture is of a disc of radius lrad being occulted by a
		// disc of radius srad centred at projectedCentre-p. To determine the light intensity at p, we
		// then just need to estimate the proportion of the light disc being occulted.
		const double srad = b2Radius / bRadius;
		const double lrad = (lightRadius / bLightPos.Length()) * perpDist / bRadius;
		if (srad / lrad < 0.01) {
			// any eclipse would have negligible effect - ignore
			continue;
		}
		const vector3d projectedCentre = (b2pos - perpDist * lightDir) / bRadius;
		if (projectedCentre.Length() < 1 + srad + lrad) {
			// some part of b is (partially) eclipsed
			shadowsOut.emplace_back(projectedCentre, static_cast<float>(srad), static_cast<float>(lrad));
		}
	}
	return shadowsOut;
}

float discCovered(const float dist, const float rad)
{
	// proportion of unit disc covered by a second disc of radius rad placed
	// dist from centre of first disc.
	//
	// WLOG, the second disc is displaced horizontally to the right.
	// xl = rightwards distance to intersection of the two circles.
	// xs = normalised leftwards distance from centre of second disc to intersection.
	// d = vertical distance to an intersection point
	// The clampings handle the cases where one disc contains the other.
	const float radsq = rad * rad;
	const float xl = Clamp((dist * dist + 1.f - radsq) / (2.f * std::max(0.001f, dist)), -1.f, 1.f);
	const float xs = Clamp((dist - xl) / std::max(0.001f, rad), -1.f, 1.f);
	const float d = sqrt(std::max(0.f, 1.f - xl * xl));

	const float th = Clamp(acosf(xl), 0.f, float(M_PI));
	const float th2 = Clamp(acosf(xs), 0.f, float(M_PI));

	assert(!is_nan(d) && !is_nan(th) && !is_nan(th2));

	// covered area can be calculated as the sum of segments from the two
	// discs plus/minus some triangles, and it works out as follows:
	return Clamp((th + radsq * th2 - dist * d) / float(M_PI), 0.f, 1.f);
}

static std::vector<Camera::Shadow> shadows;

float Camera::ShadowedIntensity(const int lightNum, const Body *b) const
{
	shadows.clear();
	shadows.reserve(STD_SHADOWS_SIZE);
	shadows = CalcShadows(lightNum, b);
	float product = 1.0;
	for (std::vector<Camera::Shadow>::const_iterator it = shadows.begin(), itEnd = shadows.end(); it != itEnd; ++it)
		product *= 1.0 - discCovered(it->centre.Length() / it->lrad, it->srad / it->lrad);
	return product;
}

// PrincipalShadows(b,n): returns the n biggest shadows on b in order of size
std::vector<Camera::Shadow> Camera::PrincipalShadows(const Body *b, const int n) const
{
	std::vector<Shadow> shadowsOut;
	shadows.clear();
	shadows.reserve(STD_SHADOWS_SIZE);
	for (size_t i = 0; i < 4 && i < m_lightSources.size(); i++) {
		shadows = CalcShadows(i, b);
	}
	shadowsOut.reserve(shadows.size());
	std::sort(shadows.begin(), shadows.end());

	std::reverse_copy(begin(shadows), end(shadows), begin(shadowsOut));
	return shadowsOut;
}
