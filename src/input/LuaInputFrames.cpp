#include "LuaInputFrames.h"

#include "BindingContainer.h"
#include "InputFrame.h"
#include "KeyBindings.h"
#include "LuaObject.h"
#include "LuaRef.h"

#include <string>

#include "libs/utils.h"

std::list<InputFrame> LuaInputFrames::m_inputFrames = {};

LuaInputFrames::LuaInputFrames()
{
	SetEnableAll(true);
}

LuaInputFrames::~LuaInputFrames()
{
	SetEnableAll(false);
}

//static
void LuaInputFrames::Reset()
{
	m_inputFrames.clear();
}

// static
InputFrame *LuaInputFrames::AddOrUse(const std::string &name)
{
	for (InputFrame &ifr: m_inputFrames) {
		if (ifr.GetName() == name) {
			return &ifr;
		}
	}
	m_inputFrames.emplace_back(name);
	return &m_inputFrames.back();
}

void LuaInputFrames::SetEnableAll(bool active)
{
	for (auto &ifr: m_inputFrames) {
			ifr.SetActive(active);
	}
}

static int l_inputFrame_create_or_use(lua_State *L)
{
	LUA_DEBUG_START(L);
	const char *name = luaL_checkstring(L, 1);
	InputFrame *iframe = LuaInputFrames::AddOrUse(name);
	LuaObject<InputFrame>::PushToLua(iframe);
	LUA_DEBUG_END(L, 1);
	return 1;
}

static int l_inputFrame_add_action(lua_State *L)
{
	LUA_DEBUG_START(L);
	using namespace KeyBindings;

	InputFrame *iframe = LuaObject<InputFrame>::CheckFromLua(1);
	const char *actionName = luaL_checkstring(L, 2);
	const char *actionPage = luaL_checkstring(L, 3);
	const char *actionGroup = luaL_checkstring(L, 4);
	const char *actionBindString = luaL_checkstring(L, 5);

	auto &page = InputFWD::GetBindingPage(actionPage);
	if (strlen(actionPage) == 0)
		page.shouldBeTranslated = false;

	auto &group = page.GetBindingGroup(actionGroup);
	ActionBinding action;
	action.SetFromString(std::string(actionBindString));

	iframe->AddActionBinding(actionName, group, action);
	LuaRef l_ref;
	if (lua_isfunction(L, 6)) {
		l_ref = LuaRef(L, 6);
		iframe->AddCallbackFunction(actionName, l_ref);
	}
	LUA_DEBUG_END(L, 1);
	return 0;
}

template <>
const char *LuaObject<InputFrame>::s_type = "InputFrames";

template <>
void LuaObject<InputFrame>::RegisterClass()
{
	static const luaL_Reg l_methods[] = {
		{ "CreateOrUse", l_inputFrame_create_or_use },
		{ "AddAction", l_inputFrame_add_action },
		{ 0, 0 }
	};

	// creates a class in the lua vm with the given name and attaches the
	// listed methods to it and the listed metamethods to its metaclass. if
	// attributes extra magic is added to the metaclass to make them work as
	// expected
	//static void CreateClass(const char *type, const char *parent, const luaL_Reg *methods, const luaL_Reg *attrs, const luaL_Reg *meta);
	LuaObjectBase::CreateClass(s_type, 0, l_methods, nullptr, nullptr);
}
