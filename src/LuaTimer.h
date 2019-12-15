// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _LUATIMER_H
#define _LUATIMER_H

/*
 * Class: Timer
 *
 * A class to invoke functions at specific times.
 *
 * The <Timer> class provides a facility whereby scripts can request that a
 * function be called at a given time, or regularly.
 *
 * Pioneer provides a single <Timer> object to the Lua environment. It resides
 * in the global namespace and is simply called Timer.
 *
 * The <Timer> is bound to the game clock, not the OS (real time) clock. The
 * game clock is subject to time acceleration. As such, timer triggers will
 * not necessarily occur at the exact time you request but can arrive seconds,
 * minutes or even hours after the requested time (game time).
 *
 * Because timer functions are called outside of the normal event model, it is
 * possible that game objects no longer exist. Consider this example:
 *
 * > local enemy = Space.SpawnShipNear("eagle_lrf", Game.player, 20, 20)
 * > Comms.ImportantMessage(enemy:GetLabel(), "You have 20 seconds to surrender or you will be destroyed.")
 * > Timer:CallAt(Game.time+20, function ()
 * >     Comms.ImportantMessage(enemy:GetLabel(), "You were warned. Prepare to die!")
 * >     enemy:Kill(Game.player)
 * > end)
 *
 * This works exactly as you'd expect: 20 seconds after the threat message is
 * sent, the enemy comes to life and attacks the player. If however the player
 * chooses to avoid the battle by hyperspacing away, the enemy ship is
 * destroyed by the game engine. In that case, the "enemy" object held by the
 * script is a shell, and any attempt to use it will be greeted by a Lua error.
 *
 * To protect against this, you should call <Object.exists> to confirm that the
 * underlying object exists before trying to use it.
 */

#include "DeleteEmitter.h"

class LuaTimer : public DeleteEmitter {
public:
	LuaTimer();
	~LuaTimer();
	void Tick(const double actualTime);
	static double GetTime() { return m_time; };
private:
	void RemoveAll();
	static double m_time;
};

#endif
