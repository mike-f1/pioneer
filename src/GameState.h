#ifndef GAMESTATE_H
#define GAMESTATE_H

#include "JsonFwd.h"
#include <string>

class SystemPath;

class GameState
{
	public:
		GameState() = delete;

	static void MakeNewGame(const SystemPath &path, const double startDateTime = 0.0);
	static Json LoadGameToJson(const std::string &filename);
	// LoadGame and SaveGame throw exceptions on failure
	static void LoadGame(const std::string &filename);
	static bool CanLoadGame(const std::string &filename);
	// XXX game arg should be const, and this should probably be a member function
	// (or LoadGame/SaveGame should be somewhere else entirely)
	static void SaveGame(const std::string &filename);

	static void DestroyGame();

	protected:

	private:
};

#endif // GAMESTATE_H
