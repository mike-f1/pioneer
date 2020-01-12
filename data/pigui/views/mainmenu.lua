-- Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
-- Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

local Engine = import('Engine')
local Game = import('Game')
local Ship = import("Ship")
local ShipDef = import("ShipDef")
local SystemPath = import("SystemPath")
local Equipment = import("Equipment")
local Format = import("Format")
local ui = import('pigui/pigui.lua')
local Event = import('Event')
local Lang = import("Lang")
local ModelSpinner = import("PiGui.Modules.ModelSpinner")

local lc = Lang.GetResource("core")
local lui = Lang.GetResource("ui-core")
local qlc = Lang.GetResource("quitconfirmation-core")
local elc = Lang.GetResource("equipment-core")
local clc = Lang.GetResource("commodity")

local modelSpinner = ModelSpinner()
local cachedShip = nil
local cachedTime = nil

local cargo = Equipment.cargo
local misc = Equipment.misc
local laser = Equipment.laser
local hyperspace = Equipment.hyperspace

local player = nil
local colors = ui.theme.colors
local pionillium = ui.fonts.pionillium
local orbiteer = ui.fonts.orbiteer
local icons = ui.theme.icons

local mainButtonSize = Vector2(400,46) * (ui.screenHeight / 1200)
local dialogButtonSize = Vector2(150,46) * (ui.screenHeight / 1200)
local mainButtonFontSize = 24 * (ui.screenHeight / 1200)

local showQuitConfirm = false
local quitConfirmMsg
local max_flavours = 22

local startLocations = {
	{['name']=lui.START_AT_MARS,
	 ['desc']=lui.START_AT_MARS_DESC,
	 ['location']=SystemPath.New(0,0,0,0,18),
	 ['shipType']='sinonatrix',['money']=100,['hyperdrive']=true,
	 ['equipment']={
		{laser.pulsecannon_1mw,1},
		{misc.atmospheric_shielding,1},
		{misc.autopilot,1},
		{misc.radar,1},
		{cargo.hydrogen,2}}},
	{['name']=lui.START_AT_NEW_HOPE,
	 ['desc']=lui.START_AT_NEW_HOPE_DESC,
	 ['location']=SystemPath.New(1,-1,-1,0,4),
	 ['shipType']='pumpkinseed',['money']=100,['hyperdrive']=true,
	 ['equipment']={
		{laser.pulsecannon_1mw,1},
		{misc.atmospheric_shielding,1},
		{misc.autopilot,1},
		{misc.radar,1},
		{cargo.hydrogen,2}}},
	{['name']=lui.START_AT_BARNARDS_STAR,
	 ['desc']=lui.START_AT_BARNARDS_STAR_DESC,
	 ['location']=SystemPath.New(-1,0,0,0,16),
	 ['shipType']='xylophis',['money']=100,['hyperdrive']=false,
	 ['equipment']={
		{misc.atmospheric_shielding,1},
		{misc.autopilot,1},
		{misc.radar,1},
		{cargo.hydrogen,2}}}
}

local cycleTime = 9000
local zoomTime = 2500
local minSize = 50

local function shipSpinner()
	local spinnerWidth = ui.getColumnWidth() or ui.getContentRegion().x

	if cachedShip == nil then
		cachedShip = modelSpinner:setRandomModel()
	end

	if cachedTime == nil then
		cachedTime = Engine.ticks
	end

	-- TODO: Need a (better) way to handle time during intro, asking milliseconds passed
	-- in Engine is not a good thing
	local time = Engine.ticks - cachedTime
	local zooming = false

	if time >= cycleTime then
		cachedTime = Engine.ticks
		cachedShip = modelSpinner:setRandomModel()
	end

	if time <= zoomTime then
		-- Zoom in:
		modelSpinner:setSize(Vector2(spinnerWidth, spinnerWidth / 1.5) * (time + minSize) / (zoomTime + minSize))
		zooming = true
	end

	if time >= cycleTime - zoomTime then
		-- Zoom out:
		modelSpinner:setSize(Vector2(spinnerWidth, spinnerWidth / 1.5) * (cycleTime - (time - minSize)) / (zoomTime + minSize))
		zooming = true
	end

	ui.child("modelSpinnerWindow", Vector2(-1,-1), {"NoTitleBar", "AlwaysAutoResize", "NoMove", "NoInputs", "AlwaysUseWindowPadding"}, function()
		ui.group(function ()
			modelSpinner:draw()
			if not zooming then
				local font = ui.fonts.orbiteer.large
				ui.withFont(font.name, font.size, function()
					ui.text(cachedShip)
					ui.sameLine()
				end)
			end
		end)
	end)
end

local function dialogTextButton(label, enabled, callback)
	local bgcolor = enabled and colors.buttonBlue or colors.grey

	local button
	ui.withFont(pionillium.large.name, mainButtonFontSize, function()
		button = ui.coloredSelectedButton(label, dialogButtonSize, false, bgcolor, nil, enabled)
	end)
	if button then
		callback()
	end
	return button
end

local function mainTextButton(label, tooltip, enabled, callback)
	local bgcolor = enabled and colors.buttonBlue or colors.grey

	local button
	ui.withFont(pionillium.large.name, mainButtonFontSize, function()
		button = ui.coloredSelectedButton(label, mainButtonSize, false, bgcolor, tooltip, enabled)
	end)
	if button and enabled then
		callback()
	end
	return button
end --mainTextButton

local function confirmQuit()
	ui.setNextWindowPosCenter('Always')

	ui.withStyleColors({["WindowBg"] = colors.commsWindowBackground}, function()
		-- TODO: this window should be ShowBorders
		ui.window("MainMenuQuitConfirm", {"NoTitleBar", "NoResize", "AlwaysAutoResize"}, function()
			ui.withFont(pionillium.large.name, mainButtonFontSize, function()
				ui.text(qlc.QUIT)
			end)
			ui.withFont(pionillium.large.name, pionillium.large.size, function()
				ui.pushTextWrapPos(450)
				ui.textWrapped(quitConfirmMsg)
				ui.popTextWrapPos()
			end)
			ui.dummy(Vector2(5,15)) -- small vertical spacing
			ui.dummy(Vector2(30, 5))
			ui.sameLine()
			dialogTextButton(qlc.YES, true, Engine.Quit)
			ui.sameLine()
			ui.dummy(Vector2(30, 5))
			ui.sameLine()
			dialogTextButton(qlc.NO, true, function() showQuitConfirm = false end)
		end)
	end)
end

local function showOptions()
	ui.showOptionsWindow = true
end

local function quitGame()
	if Engine.GetConfirmQuit() then
		quitConfirmMsg = string.interp(qlc["MSG_" .. Engine.rand:Integer(1, max_flavours)],{yes = qlc.YES, no = qlc.NO})
		showQuitConfirm = true
	else
		Engine.Quit()
	end
end

local function continueGame()
	Game.LoadGame("_exit")
end

local function startAtLocation(location)
	Game.StartGame(location.location)
	Game.player:SetShipType(location.shipType)
	Game.player:SetLabel(Ship.MakeRandomLabel())
	if location.hyperdrive then
		Game.player:AddEquip(hyperspace['hyperdrive_'..ShipDef[Game.player.shipId].hyperdriveClass])
	end
	Game.player:SetMoney(location.money)
	for _,equip in pairs(location.equipment) do
		Game.player:AddEquip(equip[1],equip[2])
	end
end

local function mainMenu()
	local canContinue = Game.CanLoadGame('_exit')
	local buttons = 4

	ui.withStyleColors({["WindowBg"] = colors.lightBlackBackground}, function()
		ui.child("MainMenuButtons", Vector2(-1,-1), {"NoTitleBar", "NoMove", "NoResize", "NoFocusOnAppearing", "NoBringToFrontOnFocus"}, function()
			mainTextButton(lui.CONTINUE_GAME, nil, canContinue, continueGame)

			for _,loc in pairs(startLocations) do
				local desc = loc.desc .. "\n"
				local name = loc.shipType
				local sd = ShipDef[loc.shipType]
				if sd then
					name = sd.name
				end
				desc = desc .. lc.SHIP .. ": " .. name .. "\n"
				desc = desc .. lui.CREDITS .. ": "  .. Format.Money(loc.money) .. "\n"
				desc = desc .. lui.HYPERDRIVE .. ": " .. (loc.hyperdrive and lui.YES or lui.NO) .. "\n"
				desc = desc .. lui.EQUIPMENT .. ":\n"
				for _,eq in pairs(loc.equipment) do
				local equipname
					if pcall(function() local t = elc[eq[1].l10n_key] end) then
						equipname = elc[eq[1].l10n_key]
					elseif pcall(function() local t= clc[eq[1].l10n_key] end) then
						equipname = clc[eq[1].l10n_key]
					end
					desc = desc .. "  - " .. equipname
					if eq[2] > 1 then
						desc = desc .. " x" .. eq[2] .. "\n"
					else desc = desc .. "\n"
					end
				end
				mainTextButton(loc.name, desc, true, function() startAtLocation(loc) end)
			end

			mainTextButton(lui.LOAD_GAME, nil, true, function() ui.showSavedGameWindow = "LOAD" end)
			mainTextButton(lui.OPTIONS, nil, true, showOptions)
			mainTextButton(lui.QUIT, nil, true, quitGame)

			if showQuitConfirm then confirmQuit() end
		end)
	end)
end

local function callModules(mode)
	for k,v in pairs(ui.getModules(mode)) do
		v.fun()
	end
end

local function showMainMenu()
	ui.setNextWindowPos(Vector2(110,65),'Always')
	ui.withStyleColors({["WindowBg"]=colors.transparent}, function()
		ui.window("headingWindow", {"NoTitleBar","NoResize","NoFocusOnAppearing","NoBringToFrontOnFocus","AlwaysAutoResize"}, function()
			ui.withFont("orbiteer", 36 * (ui.screenHeight / 1200),function() ui.text("Pioneer") end)
		end)
	end)

	ui.setNextWindowPos(Vector2(10,100),'Always')
	ui.setNextWindowSize(Vector2(ui.screenWidth - 20, ui.screenHeight - 10),'Always')
	ui.withStyleVars({ WindowPadding = Vector2(12, 12) }, function()
		ui.withStyleColors({["WindowBg"]=colors.transparent}, function()
			ui.window("", {"NoTitleBar", "AlwaysAutoResize", "NoInputs", "AlwaysUseWindowPadding"}, function()
				ui.columns(2, "menuCol")
				ui.setColumnWidth(0, ui.screenWidth * 3 / 5)
				shipSpinner()
				ui.nextColumn()
				mainMenu()
			end)
		end)
	end)

	local build_text = Engine.version
	ui.withFont("orbiteer", 16 * (ui.screenHeight/1200),
		function()
			ui.setNextWindowPos(Vector2(ui.screenWidth - ui.calcTextSize(build_text).x * 1.2,ui.screenHeight - 50), 'Always')
			ui.withStyleColors({["WindowBg"] = colors.transparent}, function()
				ui.window("buildLabel", {"NoTitleBar", "NoResize", "NoFocusOnAppearing", "NoBringToFrontOnFocus", "AlwaysAutoResize"},
					function()
						ui.text(build_text)
					end)
			end)
	end)

	callModules('mainMenu')
end -- showMainMenu

ui.registerHandler('mainMenu',function(delta)
	showMainMenu()
end)
