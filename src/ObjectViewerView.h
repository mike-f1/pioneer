// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _OBJECTVIEWERVIEW_H
#define _OBJECTVIEWERVIEW_H

#include "InputFrame.h"
#include "UIView.h"
#include "libs/RefCounted.h"
#include "libs/matrix4x4.h"
#include "libs/vector2.h"
#include <memory>

namespace KeyBindings {
	struct ActionBinding;
	struct AxisBinding;
}

class Body;
class Camera;
class CameraContext;
class Game;
class InputFrameStatusTicket;

enum class GSDebugFlags;
namespace SceneGraph {
	enum class DebugFlags;
}

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

	void DrawAdditionalUIForSysBodies();
	void DrawAdditionalUIForBodies();

	bool m_showBBox = false;
	bool m_showCollMesh = false;
	bool m_showWireFrame = false;
	bool m_showTags = false;
	bool m_showDocking = false;

	bool m_showSingleBBox = false;
	bool m_showNearestBBox = false;
	bool m_showBoundSphere = false;

	SceneGraph::DebugFlags m_debugFlags;

	GSDebugFlags m_planetDebugFlags;

	float m_viewingDist;
	Body *m_lastTarget, *m_newTarget;
	matrix4x4d m_camRot;
	matrix3x3d m_camTwist;

	float m_lightAngle;

	enum class RotateLight {
		LEFT,
		RIGHT,
		NONE
	} m_lightRotate;

	enum class Zooming {
		OUT,
		IN,
		NONE
	} m_zoomChange;

	void OnLightRotateLeft();
	void OnLightRotateRight();

	std::unique_ptr<Camera> m_camera;

	void OnResetViewParams();
	void OnResetTwistMatrix();

	// UI members
	vector2f m_screen;
	float m_sbMass;
	float m_sbRadius;
	uint32_t m_sbSeed;
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

	std::unique_ptr<InputFrameStatusTicket> m_bindingLock;

};

#endif /* _OBJECTVIEWERVIEW_H */
