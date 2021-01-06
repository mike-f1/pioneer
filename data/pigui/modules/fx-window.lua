-- Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
-- Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

local Engine = import('Engine')
local Game = import('Game')
local ui = import('pigui/pigui.lua')
local Lang = import("Lang")
local lc = Lang.GetResource("core");
local lui = Lang.GetResource("ui-core");
local utils = import("utils")
local Event = import("Event")

local Iframes = import("InputFrames")

local ifr = Iframes.CreateOrUse("luaInputF")

local showInfo = false
ifr:AddAction("ShowInfo", "General", "ViewsControls", "Key1073741884Mod0", function(isUp)
		if isUp then showInfo = true end
	end)

local showSpaceStation = false
ifr:AddAction("ShowSpaceStation", "General", "ViewsControls", "Key1073741885Mod0", function(isUp)
		if isUp then showSpaceStation = true end
	end)

local showWorldView = false
ifr:AddAction("ShowWorldView", "General", "ViewsControls", "Key1073741882Mod0", function(isUp)
		if isUp then showWorldView = true end
	end)

local switchMapWorld = false
ifr:AddAction("SwitchMapWorldView", "General", "ViewsControls", "Key1073741883Mod0", function(isUp)
		if isUp then switchMapWorld = true end
	end)

local switchSectorView = false
ifr:AddAction("SwitchToSectorView", "General", "ViewsControls", "Key1073741886Mod0", function(isUp)
		if isUp then switchSectorView = true end
	end)

local switchSystemView = false
ifr:AddAction("SwitchToSystemView", "General", "ViewsControls", "Key1073741887Mod0", function(isUp)
		if isUp then switchSystemView = true end
	end)

local switchSystemInfoView = false
ifr:AddAction("SwitchToSystemInfoView", "General", "ViewsControls", "Key1073741888Mod0", function(isUp)
		if isUp then switchSystemInfoView = true end
	end)

local switchGalaxyView = false
ifr:AddAction("SwitchToGalaxyView", "General", "ViewsControls", "Key1073741889Mod0", function(isUp)
		if isUp then switchGalaxyView = true end
	end)

local player = nil
local colors = ui.theme.colors
local icons = ui.theme.icons

local mainButtonSize = Vector2(32,32) * (ui.screenHeight / 1200)
local mainButtonFramePadding = 3
local function mainMenuButton(icon, selected, tooltip, color)
	if color == nil then
		color = colors.white
	end
	return ui.coloredSelectedIconButton(icon, mainButtonSize, selected, mainButtonFramePadding, colors.buttonBlue, color, tooltip)
end

local currentView = "internal"

local next_cam_type = { ["internal"] = "external", ["external"] = "sidereal", ["sidereal"] = "internal", ["flyby"] = "internal" }
local cam_tooltip = { ["internal"] = lui.HUD_BUTTON_INTERNAL_VIEW, ["external"] = lui.HUD_BUTTON_EXTERNAL_VIEW, ["sidereal"] = lui.HUD_BUTTON_SIDEREAL_VIEW, ["flyby"] = lui.HUD_BUTTON_FLYBY_VIEW }
local function button_world(current_view)
	ui.sameLine()
	if current_view ~= "world" then
		if mainMenuButton(icons.view_internal, false, lui.HUD_BUTTON_SWITCH_TO_WORLD_VIEW) or showWorldView then
			Game.SetView("world")
			ui.playBoinkNoise()
		end
	else
		local camtype = Game.GetWorldCamType()
		if mainMenuButton(icons["view_" .. camtype], true, cam_tooltip[camtype]) or showWorldView then
			Game.SetWorldCamType(next_cam_type[camtype])
			ui.playBoinkNoise()
		end
		if (ui.altHeld() and showWorldView) then
			Game.SetWorldCamType("flyby")
			ui.playBoinkNoise()
		end
	end
	showWorldView = false
end

local current_map_view = "sector"
local function buttons_map(current_view)
	local onmap = current_view == "sector" or current_view == "system" or current_view == "system_info" or current_view == "galaxy"

	ui.sameLine()
	local active = current_view == "sector"
	if mainMenuButton(icons.sector_map, active, active and lui.HUD_BUTTON_SWITCH_TO_WORLD_VIEW or lui.HUD_BUTTON_SWITCH_TO_SECTOR_MAP) or switchSectorView then
		switchSectorView = false
		ui.playBoinkNoise()
		if active then
			Game.SetView("world")
		else
			Game.SetView("sector")
			current_map_view = "sector"
		end
	end

	ui.sameLine()
	active = current_view == "system"
	if mainMenuButton(icons.system_map, active, active and lui.HUD_BUTTON_SWITCH_TO_WORLD_VIEW or lui.HUD_BUTTON_SWITCH_TO_SYSTEM_MAP) or switchSystemView then
		switchSystemView = false
		ui.playBoinkNoise()
		if active then
			Game.SetView("world")
		else
			Game.SetView("system")
			current_map_view = "system"
		end
	end

	ui.sameLine()
	active = current_view == "system_info"
	if mainMenuButton(icons.system_overview, active, active and lui.HUD_BUTTON_SWITCH_TO_WORLD_VIEW or lui.HUD_BUTTON_SWITCH_TO_SYSTEM_OVERVIEW) or switchSystemInfoView then
		switchSystemInfoView = false
		ui.playBoinkNoise()
		if active then
			ui.systemInfoViewNextPage()
		else
			Game.SetView("system_info")
			current_map_view = "system_info"
		end
	end
	if onmap then
		ui.sameLine()
		active = current_view == "galaxy"
		if mainMenuButton(icons.galaxy_map, active, active and lui.HUD_BUTTON_SWITCH_TO_WORLD_VIEW or lui.HUD_BUTTON_SWITCH_TO_GALAXY_MAP) or switchGalaxyView then
			switchGalaxyView = false
			ui.playBoinkNoise()
			if active then
				Game.SetView("world")
			else
				Game.SetView("galaxy")
				current_map_view = "galaxy"
			end
		end
	end
	if switchMapWorld then
		switchMapWorld = false
		ui.playBoinkNoise()
		if onmap then
			Game.SetView("world")
		else
			Game.SetView(current_map_view)
		end
	end
end

local function button_info(current_view)
	ui.sameLine()
	if (mainMenuButton(icons.personal_info, current_view == "info", lui.HUD_BUTTON_SHOW_PERSONAL_INFO) or showInfo) then
		showInfo = false
		ui.playBoinkNoise()
		if current_view ~= "info" then
			Game.SetView("info")
		else
			Game.SetView("world")
		end
	end
end

local function button_comms(current_view)
	ui.sameLine()
	if mainMenuButton(icons.comms, current_view == "space_station", lui.HUD_BUTTON_SHOW_COMMS) or showSpaceStation then
		showSpaceStation = false
		if player:IsDocked() then
			ui.playBoinkNoise()
			if current_view == "space_station" then
				Game.SetView("world")
			else
				Game.SetView("space_station")
			end
		else
			if ui.toggleSystemTargets then
				ui.toggleSystemTargets()
			end
		end
	end
end

local function displayFxWindow()
	if ui.showOptionsWindow then return end
	player = Game.player
	local current_view = Game.CurrentView()
	local aux = Vector2((mainButtonSize.x + mainButtonFramePadding * 2) * 10, (mainButtonSize.y + mainButtonFramePadding * 2) * 1.5)
	ui.setNextWindowSize(aux, "Always")
	aux = Vector2(ui.screenWidth/2 - (mainButtonSize.x + 4 * mainButtonFramePadding) * 7.5/2, 0)
	ui.setNextWindowPos(aux , "Always")
	ui.window("Fx", {"NoTitleBar", "NoResize", "NoFocusOnAppearing", "NoBringToFrontOnFocus", "NoScrollbar"},
						function()
							button_world(current_view)

							button_info(current_view)

							button_comms(current_view)

							buttons_map(current_view)
	end)
end

ui.registerModule("game", displayFxWindow)

return {}
