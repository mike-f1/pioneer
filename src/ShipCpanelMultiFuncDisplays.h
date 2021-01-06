// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _SHIPCPANELMULTIFUNCDISPLAYS_H
#define _SHIPCPANELMULTIFUNCDISPLAYS_H

#include "JsonFwd.h"
#include "Object.h"
#include "graphics/Drawables.h"
#include "gui/GuiWidget.h"
#include "input/InputFwd.h"

class InputFrame;

namespace Graphics {
	class RenderState;
}

enum multifuncfunc_t {
	MFUNC_RADAR,
	MFUNC_EQUIPMENT,
	MFUNC_MAX
};

class IMultiFunc {
public:
	sigc::signal<void> onGrabFocus;
	sigc::signal<void> onUngrabFocus;
	virtual void Update() = 0;
};

class RadarWidget : public IMultiFunc, public Gui::Widget {
public:
	RadarWidget();
	RadarWidget(const Json &jsonObj);
	virtual ~RadarWidget();
	void GetSizeRequested(float size[2]);
	void InitScaling(void);
	void Draw();
	virtual void Update();

	void TimeStepUpdate(float step);

	void SaveToJson(Json &jsonObj);

	static void RegisterInputBindings();
	void AttachBindingCallback();
private:
	void InitObject();

	void ToggleMode(bool down);

	void DrawBlobs(bool below);
	void GenerateBaseGeometry();
	void GenerateRingsAndSpokes();
	void DrawRingsAndSpokes(bool blend);

	struct Contact {
		Object::Type type;
		vector3d pos;
		bool isSpecial;
	};
	std::list<Contact> m_contacts;
	Graphics::Drawables::Lines m_contactLines;
	Graphics::Drawables::Points m_contactBlobs;

	enum class RadarMode {
		MODE_AUTO,
		MODE_MANUAL
	};
	RadarMode m_mode;

	float m_currentRange, m_manualRange, m_targetRange;
	float m_scale;

	float m_x;
	float m_y;

	float m_lastRange;
	float RADAR_XSHRINK;
	float RADAR_YSHRINK;

	std::vector<vector3f> m_circle;
	std::vector<vector3f> m_spokes;
	std::vector<vector3f> m_vts;
	std::vector<vector3f> m_edgeVts;
	std::vector<Color> m_edgeCols;

	Graphics::RenderState *m_renderState;

	Graphics::Drawables::Lines m_scanLines;
	Graphics::Drawables::Lines m_edgeLines;

	inline static struct RadarWidgetBinding {
		ActionId toggleScanMode;
		AxisId changeScanRange;

	} m_radarWidgetBindings;

	static std::unique_ptr<InputFrame> m_inputFrame;
};

#endif /* _SHIPCPANELMULTIFUNCDISPLAYS_H */
