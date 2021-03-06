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
local Format = import("Format")

local player = nil
local pionillium = ui.fonts.pionillium
local colors = ui.theme.colors
local icons = ui.theme.icons

local button_size = Vector2(32,32) * (ui.screenHeight / 1200)
local frame_padding = 3
local bg_color = colors.buttonBlue
local fg_color = colors.white

local function displayTimeWindow()
	player = Game.player
	local date = Format.Date(Game.time)

	local current = Game.GetTimeAcceleration()
	local requested = Game.GetRequestedTimeAcceleration()
	function accelButton(name)
		local color = bg_color
		if requested == name and current ~= name then
			color = colors.white
		end
		local time = name
		-- translate only paused, the rest can stay
		if time == "paused" then
			time = lc.PAUSED
		end
		tooltip = string.interp(lui.HUD_REQUEST_TIME_ACCEL, { time = time })
		if ui.coloredSelectedIconButton(icons['time_accel_' .. name], button_size, current == name, frame_padding, color, fg_color, tooltip) then
				Game.SetTimeAcceleration(name, ui.ctrlHeld() or ui.isMouseDown(1))
		end
		ui.sameLine()
	end
	ui.withFont(pionillium.large.name, pionillium.large.size, function()
								local text_size = ui.calcTextSize(date)
								local window_size = Vector2(math.max(text_size.x, (button_size.x + frame_padding * 2 + 7) * 6) + 15, text_size.y + button_size.y + frame_padding * 2 + 20)
								ui.timeWindowSize = window_size
								ui.setNextWindowSize(window_size, "Always")
								ui.setNextWindowPos(Vector2(0, ui.screenHeight - window_size.y), "Always")
								ui.window("Time", {"NoTitleBar", "NoResize", "NoSavedSettings", "NoFocusOnAppearing", "NoBringToFrontOnFocus"}, function()
														ui.text(date)
														accelButton("paused")
														accelButton("1x")
														accelButton("10x")
														accelButton("100x")
														accelButton("1000x")
														accelButton("10000x")
								end)
	end)
end

ui.registerModule("game", displayTimeWindow)

return {}
