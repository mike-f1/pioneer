// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _SYSTEMINFOVIEW_H
#define _SYSTEMINFOVIEW_H

#include "Color.h"
#include "galaxy/StarSystem.h"
#include "galaxy/SystemPath.h"
#include "graphics/Drawables.h"

#include "UIView.h"
#include "gui/GuiImageRadioButton.h"
#include <vector>

class Game;
class StarSystem;
class SystemBody;

namespace Graphics {
	class RenderState;
}

namespace Gui {
	class MultiStateImageButton;
	class Tabbed;
	class TexturedQuad;
	class VBox;
}

class SystemInfoView : public UIView {
public:
	SystemInfoView(Game *game);
	virtual void Update(const float frameTime) override;
	virtual void Draw3D() override;
	void NextPage();

protected:
	virtual void OnSwitchTo() override;

private:
	class BodyIcon : public Gui::ImageRadioButton {
	public:
		BodyIcon(const char *img);
		virtual void Draw() override;
		virtual void OnActivate() override;
		bool HasStarport() const { return m_hasStarport; }
		void SetHasStarport() { m_hasStarport = true; }
		void SetSelectColor(const Color &color) { m_selectColor = color; }

	private:
		Graphics::RenderState *m_renderState;
		Graphics::Drawables::Lines m_selectBox;
		std::unique_ptr<Graphics::Drawables::Circle> m_circle;
		bool m_hasStarport;
		Color m_selectColor;
	};

	enum RefreshType {
		REFRESH_NONE,
		REFRESH_SELECTED_BODY,
		REFRESH_ALL
	};

	RefreshType NeedsRefresh() const;
	void SystemChanged(const SystemPath &path);
	void UpdateEconomyTab();
	void OnBodyViewed(SystemBody *b);
	void OnBodySelected(SystemBody *b);
	void OnClickBackground(Gui::MouseButtonEvent *e);
	void PutBodies(SystemBody *body, Gui::Fixed *container, int dir, float pos[2], int &majorBodies, int &starports, int &onSurface, float &prevSize);
	void UpdateIconSelections();

	Gui::VBox *m_infoBox;
	Gui::Fixed *m_econInfo;
	Gui::Fixed *m_econMajImport, *m_econMinImport;
	Gui::Fixed *m_econMajExport, *m_econMinExport;
	Gui::Fixed *m_econIllegal;
	Gui::Fixed *m_sbodyInfoTab, *m_econInfoTab;

	Gui::Label *m_commodityTradeLabel;
	Gui::Tabbed *m_tabs;
	RefCountedPtr<StarSystem> m_system;
	SystemPath m_selectedBodyPath;
	RefreshType m_refresh;
	//map is not enough to associate icons as each tab has their own. First element is the body index of SystemPath (names are not unique)
	std::vector<std::pair<uint32_t, BodyIcon *>> m_bodyIcons;
	bool m_unexplored;
	bool m_hasTradeComputer;
};

#endif /* _SYSTEMINFOVIEW_H */
