// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _VIEW_H
#define _VIEW_H

#include "JsonFwd.h"
#include "gui/GuiFixed.h"

class ShipCpanel;

/*
 * For whatever draws crap into the main area of the screen.
 * Eg:
 *  game 3d view
 *  system map
 *  sector map
 */
class View : public Gui::Fixed {
public:
	View();
	virtual ~View();
	// called before Gui::Draw will call widget ::Draw methods.
	virtual void Draw3D() = 0;
	// for checking key states, mouse crud
	virtual void Update(const float frameTime) = 0;
	virtual void SaveToJson(Json &jsonObj) {}
	virtual void LoadFromJson(const Json &jsonObj) {}

	void Attach();
	void Detach();

	static void SetCpanel(ShipCpanel *cpan) { s_cpan = cpan; }

protected:
	virtual void OnSwitchTo() = 0;
	virtual void OnSwitchFrom() {}

	static ShipCpanel *s_cpan;
};

#endif /* _VIEW_H */
