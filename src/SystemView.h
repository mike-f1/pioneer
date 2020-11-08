// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _SYSTEMVIEW_H
#define _SYSTEMVIEW_H

#include "Color.h"
#include "UIView.h"
#include "graphics/Drawables.h"
#include "libs/matrix4x4.h"
#include "libs/vector3.h"
#include <memory>

class Game;
class InputFrame;
class Orbit;
class Ship;
class StarSystem;
class SystemBody;
class TransferPlanner;

namespace KeyBindings {
	struct ActionBinding;
	struct AxisBinding;
	struct WheelBinding;
}

namespace Gui {
	class ImageButton;
	class Label;
	class LabelSet;
	class MultiStateImageButton;
	class TexturedQuad;
}

enum ShipDrawing {
	BOXES,
	ORBITS,
	OFF
};

enum class GridDrawing {
	GRID,
	GRID_AND_LEGS,
	OFF
};

enum ShowLagrange {
	LAG_ICON,
	LAG_ICONTEXT,
	LAG_OFF
};

class SystemView : public UIView {
public:
	SystemView();
	virtual ~SystemView();
	virtual void Update(const float frameTime) override;
	virtual void Draw3D() override;

	const TransferPlanner *GetPlanner() const { return m_planner.get(); };
	void ResetPlanner();

private:
	virtual void OnSwitchTo() override;
	virtual void OnSwitchFrom() override;
	void RegisterInputBindings();

	static const double PICK_OBJECT_RECT_SIZE;
	static const uint16_t N_VERTICES_MAX;
	void PutOrbit(const Orbit *orb, const vector3d &offset, const Color &color, const double planetRadius = 0.0, const bool showLagrange = false);
	void PutBody(const SystemBody *b, const vector3d &offset, const matrix4x4f &trans);
	void PutLabel(const SystemBody *b, const vector3d &offset);
	void PutSelectionBox(const SystemBody *b, const vector3d &rootPos, const Color &col);
	void PutSelectionBox(const vector3d &worldPos, const Color &col);
	void GetTransformTo(const SystemBody *b, vector3d &pos);
	void OnClickObject(const SystemBody *b);
	void OnClickLagrange();
	void OnClickAccel(float step);
	void OnClickRealt();
	void OnIncreaseFactorButtonClick(void), OnResetFactorButtonClick(void), OnDecreaseFactorButtonClick(void);
	void OnIncreaseStartTimeButtonClick(void), OnResetStartTimeButtonClick(void), OnDecreaseStartTimeButtonClick(void);
	void OnToggleShipsButtonClick(void);
	void OnToggleGridButtonClick(void);
	void OnToggleL4L5ButtonClick(Gui::MultiStateImageButton *);
	void ResetViewpoint();
	void RefreshShips(void);
	void DrawShips(const double t, const vector3d &offset);
	void PrepareGrid();
	void DrawGrid();
	void LabelShip(Ship *s, const vector3d &offset);
	void OnClickShip(Ship *s);

	RefCountedPtr<StarSystem> m_system;
	const SystemBody *m_selectedObject;
	std::vector<SystemBody *> m_displayed_sbody;
	bool m_unexplored;
	ShowLagrange m_showL4L5;
	std::unique_ptr<TransferPlanner> m_planner;
	std::list<std::pair<Ship *, Orbit>> m_contacts;
	Gui::LabelSet *m_shipLabels;
	ShipDrawing m_shipDrawing;
	GridDrawing m_gridDrawing;
	int m_grid_lines;
	float m_rot_x, m_rot_y;
	float m_rot_x_to, m_rot_y_to;
	float m_zoom, m_zoomTo;
	double m_time;
	bool m_realtime;
	double m_timeStep;
	Gui::ImageButton *m_zoomInButton;
	Gui::ImageButton *m_zoomOutButton;
	Gui::ImageButton *m_toggleShipsButton;
	Gui::ImageButton *m_toggleGridButton;
	Gui::ImageButton *m_ResetOrientButton;
	Gui::MultiStateImageButton *m_toggleL4L5Button;
	Gui::ImageButton *m_plannerIncreaseStartTimeButton, *m_plannerResetStartTimeButton, *m_plannerDecreaseStartTimeButton;
	Gui::ImageButton *m_plannerIncreaseFactorButton, *m_plannerResetFactorButton, *m_plannerDecreaseFactorButton;
	Gui::ImageButton *m_plannerAddProgradeVelButton;
	Gui::ImageButton *m_plannerAddRetrogradeVelButton;
	Gui::ImageButton *m_plannerAddNormalVelButton;
	Gui::ImageButton *m_plannerAddAntiNormalVelButton;
	Gui::ImageButton *m_plannerAddRadiallyInVelButton;
	Gui::ImageButton *m_plannerAddRadiallyOutVelButton;
	Gui::ImageButton *m_plannerZeroProgradeVelButton, *m_plannerZeroNormalVelButton, *m_plannerZeroRadialVelButton;
	Gui::Label *m_timePoint;
	Gui::Label *m_infoLabel;
	Gui::Label *m_infoText;
	Gui::Label *m_plannerFactorText, *m_plannerStartTimeText, *m_plannerProgradeDvText, *m_plannerNormalDvText, *m_plannerRadialDvText;
	Gui::LabelSet *m_objectLabels;

	std::unique_ptr<Graphics::Drawables::Disk> m_bodyIcon;
	std::unique_ptr<Gui::TexturedQuad> m_l4Icon;
	std::unique_ptr<Gui::TexturedQuad> m_l5Icon;
	std::unique_ptr<Gui::TexturedQuad> m_periapsisIcon;
	std::unique_ptr<Gui::TexturedQuad> m_apoapsisIcon;
	Graphics::RenderState *m_lineState;
	Graphics::Drawables::Lines m_orbits;
	Graphics::Drawables::Lines m_selectBox;

	std::unique_ptr<vector3f[]> m_orbitVts;
	std::unique_ptr<Color[]> m_orbitColors;

	std::unique_ptr<Graphics::VertexArray> m_lineVerts;
	Graphics::Drawables::Lines m_lines;

	struct SystemViewBinding {
		using Action = KeyBindings::ActionBinding;
		using Axis =  KeyBindings::AxisBinding;

		Axis *zoomView;

	} m_systemViewBindings;

	std::unique_ptr<InputFrame> m_inputFrame;
};

#endif /* _SYSTEMVIEW_H */
