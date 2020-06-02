// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _CAMERA_H
#define _CAMERA_H

#include "Color.h"
#include "FrameId.h"
#include "graphics/Frustum.h"
#include "graphics/Light.h"
#include "libs/RefCounted.h"
#include "libs/matrix4x4.h"
#include "libs/vector3.h"

#include <memory>
#include <list>
#include <vector>

class Body;
class Frame;
class ShipCockpit;

namespace Graphics {
	class Material;
	class Renderer;
} // namespace Graphics

class CameraContext : public RefCounted {
public:
	// camera for rendering to width x height with view frustum properties
	CameraContext(float width, float height, float fovAng, float zNear, float zFar);
	~CameraContext();

	// frame to position the camera relative to
	void SetCameraFrame(FrameId frame) { m_frame = frame; }

	// camera position relative to the frame origin
	void SetCameraPosition(const vector3d &pos) { m_pos = pos; }

	// camera orientation relative to the frame origin
	void SetCameraOrient(const matrix3x3d &orient) { m_orient = orient; }

	// get the frustum. use for projection
	const Graphics::Frustum &GetFrustum() const { return m_frustum; }

	// generate and destroy the camera frame, used mostly to transform things to camera space
	void BeginFrame();
	void EndFrame();

	// only returns a valid frameID between BeginFrame and EndFrame
	FrameId GetCamFrame() const
	{
		return m_camFrame;
	}

	// apply projection and modelview transforms to the renderer
	void ApplyDrawTransforms();

private:
	float m_width;
	float m_height;
	float m_fovAng;
	float m_zNear;
	float m_zFar;

	Graphics::Frustum m_frustum;

	FrameId m_frame;
	vector3d m_pos;
	matrix3x3d m_orient;

	FrameId m_camFrame;
};

class Camera {
public:
	Camera(RefCountedPtr<CameraContext> context);
	~Camera();

	RefCountedPtr<CameraContext> GetContext() const { return m_context; }

	void Update();
	void Draw(const Body *excludeBody = nullptr, ShipCockpit *cockpit = nullptr);

	// camera-specific light with attached source body
	class LightSource {
	public:
		LightSource(const Body *b, const Graphics::Light &light) :
			m_body(b),
			m_light(light) {}

		const Body *GetBody() const { return m_body; }
		const Graphics::Light &GetLight() const { return m_light; }

	private:
		const Body *m_body;
		Graphics::Light m_light;
	};

	struct Shadow {
		Shadow(const vector3d &centre_, float srad_, float lrad_) :
			centre(centre_),
			srad(srad_),
			lrad(lrad_)
		{}
		vector3d centre;
		float srad;
		float lrad;

		bool operator<(const Shadow &other) const { return srad / lrad < other.srad / other.lrad; }
	};

	float ShadowedIntensity(const int lightNum, const Body *b) const;
	std::vector<Shadow> PrincipalShadows(const Body *b, const int n) const;

	// lights with properties in camera space
	const std::vector<LightSource> &GetLightSources() const { return m_lightSources; }
	int GetNumLightSources() const { return static_cast<uint32_t>(m_lightSources.size()); }

private:
	const std::vector<Shadow> CalcShadows(const int lightNum, const Body *b) const;

	RefCountedPtr<CameraContext> m_context;

	std::unique_ptr<Graphics::Material> m_billboardMaterial;

	// temp attrs for sorting and drawing
	struct BodyAttrs {
		Body *body;

		// camera position and orientation relative to the body
		vector3d viewCoords;
		matrix4x4d viewTransform;

		// body distance from camera
		double camDist;

		// body flags. DRAW_LAST is the interesting one
		uint32_t bodyFlags;

		// if true, draw object as billboard of billboardSize at billboardPos
		bool billboard;
		vector3f billboardPos;
		float billboardSize;
		Color billboardColor;

		// for sorting. "should a be drawn before b?"
		// NOTE: Add below function (thus an indirection) in order
		// to decouple Camera from Body.h
		static bool sort_BodyAttrs(const BodyAttrs &a, const BodyAttrs &b);
		friend bool operator<(const BodyAttrs &a, const BodyAttrs &b)
		{
			return sort_BodyAttrs(a, b);
		};
	};

	std::list<BodyAttrs> m_sortedBodies;
	std::vector<LightSource> m_lightSources;
};

#endif
