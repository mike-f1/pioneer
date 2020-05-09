// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _WORLDVIEW_H
#define _WORLDVIEW_H

#include "UIView.h"
#include "gui/GuiWidget.h"
#include "ship/ShipViewController.h"
#include "graphics/Drawables.h"

class Body;
class Camera;
class SpeedLines;
class NavTunnelWidget;
class Game;

enum VelIconType {
	V_PROGRADE,
	V_RETROGRADE,
	V_BURN
};

enum PlaneType {
	NONE,
	ROTATIONAL,
	PARENT
};

namespace Gui {
	class TexturedQuad;
	class VBox;
}

namespace KeyBindings {
	struct ActionBinding;
	struct AxisBinding;
}

namespace UI {
	class Widget;
	class Single;
	class Label;
} // namespace UI

namespace Graphics {
	class Material;
	class RenderState;
	class VertexBuffer;
} // namespace Graphics

class WorldView : public UIView {
public:
	friend class NavTunnelWidget;
	WorldView(Game *game);
	WorldView(const Json &jsonObj, Game *game);
	virtual ~WorldView();
	virtual void ShowAll();
	virtual void Update(const float frameTime) override;
	virtual void Draw3D() override;
	virtual void Draw();
	virtual void DrawUI(const float frameTime) override;

	virtual void SaveToJson(Json &jsonObj);

	RefCountedPtr<CameraContext> GetCameraContext() const { return m_cameraContext; }

	ShipViewController shipView;

	int GetActiveWeapon() const;

	std::tuple<double, double, double> CalculateHeadingPitchRoll(enum PlaneType);

	vector3d WorldSpaceToScreenSpace(const Body *body) const;
	vector3d WorldSpaceToScreenSpace(const vector3d &position) const;
	vector3d ShipSpaceToScreenSpace(const vector3d &position) const;
	vector3d GetTargetIndicatorScreenPosition(const Body *body) const;
	vector3d GetMouseDirection() const;
	vector3d CameraSpaceToScreenSpace(const vector3d &pos) const;

	void BeginCameraFrame() { m_cameraContext->BeginFrame(); };
	void EndCameraFrame() { m_cameraContext->EndFrame(); };

	bool ShouldShowLabels() { return m_labelsOn; }
	bool DrawGui() { return m_guiOn; };

protected:
	virtual void BuildUI(UI::Single *container) override;
	virtual void OnSwitchTo() override;
	virtual void OnSwitchFrom() override;

private:
	void InitObject(Game *game);
	void RegisterInputBindings();

	enum IndicatorSide {
		INDICATOR_HIDDEN,
		INDICATOR_ONSCREEN,
		INDICATOR_LEFT,
		INDICATOR_RIGHT,
		INDICATOR_TOP,
		INDICATOR_BOTTOM
	};

	struct Indicator {
		vector2f pos;
		vector2f realpos;
		IndicatorSide side;
		Indicator() :
			pos(0.0f, 0.0f),
			realpos(0.0f, 0.0f),
			side(INDICATOR_HIDDEN) {}
	};

	void UpdateProjectedObjects();
	void UpdateIndicator(Indicator &indicator, const vector3d &direction);
	void HideIndicator(Indicator &indicator);

	void DrawCombatTargetIndicator(const Indicator &target, const Indicator &lead, const Color &c);
	void DrawEdgeMarker(const Indicator &marker, const Color &c);

	void OnToggleLabels(bool down);
	void OnRequestTimeAccelInc(bool down);
	void OnRequestTimeAccelDec(bool down);

	void OnPlayerChangeTarget();

	NavTunnelWidget *m_navTunnel;
	std::unique_ptr<SpeedLines> m_speedLines;

	bool m_labelsOn;
	bool m_guiOn;

	sigc::connection m_onPlayerChangeTargetCon;

	RefCountedPtr<CameraContext> m_cameraContext;
	std::unique_ptr<Camera> m_camera;

	Indicator m_combatTargetIndicator;
	Indicator m_targetLeadIndicator;

	Graphics::RenderState *m_blendState;

	Graphics::Drawables::Line3D m_edgeMarker;
	Graphics::Drawables::Lines m_indicator;

	struct BaseBinding {
	public:
		using Action = KeyBindings::ActionBinding;
		using Axis =  KeyBindings::AxisBinding;

		Action *toggleHudMode;
		Action *increaseTimeAcceleration;
		Action *decreaseTimeAcceleration;

	} m_wviewBindings;

	std::unique_ptr<InputFrame> m_inputFrame;
};

class NavTunnelWidget : public Gui::Widget {
public:
	NavTunnelWidget(WorldView *worldView, Graphics::RenderState *);
	virtual void Draw();
	virtual void GetSizeRequested(float size[2]);
	void DrawTargetGuideSquare(const vector2f &pos, const float size, const Color &c);

private:
	void CreateVertexBuffer(const uint32_t size);

	WorldView *m_worldView;
	Graphics::RenderState *m_renderState;
	RefCountedPtr<Graphics::Material> m_material;
	std::unique_ptr<Graphics::VertexBuffer> m_vbuffer;
};

#endif /* _WORLDVIEW_H */
