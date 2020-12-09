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

local Iframes = import("InputFrames")

local ifr = Iframes.CreateOrUse("luaInputF")

timeSpeeds = {
	[0] = "paused",
	[1] = "1x",
	[10] = "10x",
	[100] = "100x",
	[1000] = "1000x",
	[10000] = "10000x",
}

local changeSpeed = false
local wantedSpeed = 0
ifr:AddAction("Speed1x", "General", "TimeControl", "Key1073741882Mod0", function(isUp)
		if isUp then wantedSpeed = 1 changeSpeed = true end
	end)
ifr:AddAction("Speed10x", "General", "TimeControl", "Key1073741882Mod0", function(isUp)
		if isUp then wantedSpeed = 10 changeSpeed = true end
	end)
ifr:AddAction("Speed100x", "General", "TimeControl", "Key1073741882Mod0", function(isUp)
		if isUp then wantedSpeed = 100 changeSpeed = true end
	end)
ifr:AddAction("Speed1000x", "General", "TimeControl", "Key1073741882Mod0", function(isUp)
		if isUp then wantedSpeed = 1000 changeSpeed = true end
	end)
ifr:AddAction("Speed10000x", "General", "TimeControl", "Key1073741882Mod0", function(isUp)
		if isUp then wantedSpeed = 10000 changeSpeed = true end
	end)

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
		if ui.coloredSelectedIconButton(icons['time_accel_' .. name], button_size, current == name, frame_padding, color, fg_color, tooltip)
		or changeSpeed then
			if changeSpeed then
				changeSpeed = false
				Game.SetTimeAcceleration(timeSpeeds[wantedSpeed], ui.ctrlHeld() or ui.isMouseDown(1))
			else
				Game.SetTimeAcceleration(name, ui.ctrlHeld() or ui.isMouseDown(1))
			end
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
