#ifndef LUAINPUTFRAME_H
#define LUAINPUTFRAME_H

#include "DeleteEmitter.h"

#include <list>
#include <string>

class InputFrame;

// The InputFrames in Lua are activated at game start and deactivated
// at game stop trough ctor and dtor, m_inputFrames are then destroyed
// when game exit (...when program shut down) avoiding leaks

// TODO: Better if they are passed to Game, not using ctor and dtor but
// that way we ends with another global which will/should be moved in InGameViews
// TODO2: remove explicit Reset _before_ Lua goes down or LuaRefs will cause crash
class LuaInputFrames: public DeleteEmitter {
public:
	LuaInputFrames();
	~LuaInputFrames();

	static void Reset();
	static InputFrame *AddOrUse(const std::string &name);

private:
	void SetEnableAll(bool enable);

	static std::list<InputFrame> m_inputFrames;
};

#endif // LUAINPUTFRAME_H
