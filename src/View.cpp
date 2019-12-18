// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "View.h"

#include "ShipCpanel.h"
#include "gui/GuiScreen.h"

ShipCpanel *View::s_cpan = nullptr;

View::View() :
	Gui::Fixed(float(Gui::Screen::GetWidth()), float(Gui::Screen::GetHeight() - 64))
{
}

View::~View()
{
	Gui::Screen::RemoveBaseWidget(s_cpan);
	Gui::Screen::RemoveBaseWidget(this);
}

void View::Attach()
{
	OnSwitchTo();

	const float w = float(Gui::Screen::GetWidth());
	const float h = float(Gui::Screen::GetHeight());

	Gui::Screen::AddBaseWidget(this, 0, 0);

	if (s_cpan) {
		Gui::Screen::AddBaseWidget(s_cpan, 0, h - 80);
	}

	ShowAll();
}

void View::Detach()
{
	Gui::Screen::RemoveBaseWidget(s_cpan);
	Gui::Screen::RemoveBaseWidget(this);
	OnSwitchFrom();
}
