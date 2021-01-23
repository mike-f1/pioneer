// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _OBJECTVIEWERVIEW_H
#define _OBJECTVIEWERVIEW_H

#include "UIView.h"
#include "libs/RefCounted.h"
#include "libs/matrix4x4.h"
#include "libs/vector2.h"
#include <memory>

#include "input/InputFwd.h"

class Body;
class Camera;
class CameraContext;
class Game;
class InputFrame;
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

	static void RegisterInputBindings();
	void AttachBindingCallback();
protected:
	virtual void OnSwitchTo() override;
	virtual void OnSwitchFrom() override;

private:

	void DrawAdditionalUIForSysBodies();
	void DrawAdditionalUIForBodies();

	bool m_showBBox = false;
	bool m_showCollMesh = false;
	bool m_showWireFrame = false;
	bool m_showTags = false;
	bool m_showDocking = false;
	bool m_showStat = false;

	bool m_showSingleBBox = false;
	bool m_showNearestBBox = false;
	bool m_showBoundSphere = false;

	SceneGraph::DebugFlags m_debugFlags;

	std::vector<std::string> m_stats;

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
	float m_sbMass = 0.;
	float m_sbRadius = 0.;
	uint32_t m_sbSeed = 0;
	float m_sbVolatileGas = 0.;
	float m_sbVolatileLiquid = 0.;
	float m_sbVolatileIces = 0.;
	float m_sbLife = 0.;
	float m_sbVolcanicity = 0.;
	float m_sbMetallicity = 0.;

	void OnChangeTerrain();
	void OnReloadSBData();
	void OnRandomSeed();
	void OnNextSeed();
	void OnPrevSeed();

	inline static struct ObjectViewerBinding {
	public:
		ActionId resetZoom;
		AxisId zoom;

		AxisId rotateLeftRight;
		AxisId rotateUpDown;

		ActionId rotateLightLeft;
		ActionId rotateLightRight;

	} m_objectViewerBindings;

	static std::unique_ptr<InputFrame> m_inputFrame;

	std::unique_ptr<InputFrameStatusTicket> m_bindingLock;
};

#endif /* _OBJECTVIEWERVIEW_H */
