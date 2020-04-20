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
}

class Body;
class Camera;
class CameraContext;
class Game;

class ObjectViewerView : public UIView {
public:
	ObjectViewerView();
	~ObjectViewerView();
	virtual void Update(const float frameTime) override;
	virtual void Draw3D() override;
	virtual void DrawUI(const float frameTime) override;

	void SetObject(Body *b) { m_newTarget = b; }

protected:
	virtual void OnSwitchTo() override;
	virtual void OnSwitchFrom() override;

private:
	void RegisterInputBindings();

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

	struct ObjectViewerBinding {
	public:
		using Action = KeyBindings::ActionBinding;
		using Axis =  KeyBindings::AxisBinding;

		Action *resetZoom;
		Axis *zoom;

		Axis *rotateLeftRight;
		Axis *rotateUpDown;

		Action *rotateLightLeft;
		Action *rotateLightRight;

	} m_objectViewerBindings;

	std::unique_ptr<InputFrame> m_inputFrame;
};

#endif /* _OBJECTVIEWERVIEW_H */
