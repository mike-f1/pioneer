// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _OBJECTVIEWERVIEW_H
#define _OBJECTVIEWERVIEW_H

#include "InputFrame.h"
#include "RefCounted.h"
#include "UIView.h"
#include "matrix4x4.h"
#include "vector2.h"
#include <memory>

namespace KeyBindings {
	struct ActionBinding;
	struct AxisBinding;
	struct WheelBinding;
}

class Body;
class Camera;
class CameraContext;
class Game;

class ObjectViewerView : public UIView {
public:
	static void RegisterInputBindings();
	ObjectViewerView();
	virtual void Update(const float frameTime) override;
	virtual void Draw3D() override;
	virtual void DrawUI(const float frameTime) override;

	void SetObject(Body *b) { m_newTarget = b; }

protected:
	virtual void OnSwitchTo() override;
	virtual void OnSwitchFrom() override;

private:
	float m_viewingDist;
	Body *m_lastTarget, *m_newTarget;
	matrix4x4d m_camRot;
	matrix3x3d m_camTwist;

	float m_lightAngle;

	enum class Zooming {
		OUT,
		IN,
		NONE
	} m_zoomChange;

	void OnMouseWheel(bool up);

	enum class RotateLight {
		LEFT,
		RIGHT,
		NONE
	} m_lightRotate;

	void OnLightRotateLeft();
	void OnLightRotateRight();

	std::unique_ptr<Camera> m_camera;

	void OnResetViewParams();
	void OnResetTwistMatrix();

	// UI members
	vector2f m_screen;
	float m_sbMass;
	float m_sbRadius;
	Uint32 m_sbSeed;
	float m_sbVolatileGas;
	float m_sbVolatileLiquid;
	float m_sbVolatileIces;
	float m_sbLife;
	float m_sbVolcanicity;
	float m_sbMetallicity;

	void OnChangeTerrain();
	void OnReloadSBData();
	void OnRandomSeed();
	void OnNextSeed();
	void OnPrevSeed();

	sigc::connection m_onMouseWheelCon;

	static struct ObjectViewerBinding : public InputFrame {
	public:
		using Action = KeyBindings::ActionBinding;
		using Axis =  KeyBindings::AxisBinding;
		using Wheel = KeyBindings::WheelBinding;

		Action *resetZoom;
		Action *zoomIn;
		Action *zoomOut;

		Action *rotateLeft;
		Action *rotateRight;
		Action *rotateUp;
		Action *rotateDown;

		Action *rotateLightLeft;
		Action *rotateLightRight;

		Wheel *mouseWheel;

		virtual void RegisterBindings();
	} ObjectViewerBindings;
};

#endif /* _OBJECTVIEWERVIEW_H */
