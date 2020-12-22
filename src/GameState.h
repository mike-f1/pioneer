#ifndef GAMESTATE_H
#define GAMESTATE_H

#include "JsonFwd.h"
#include <string>

class SystemPath;

static const unsigned int sectorRadius = 5;

class GameStateStatic
{
public:
	GameStateStatic() = delete;

	static void MakeNewGame(const SystemPath &path,
			const double startDateTime = 0.0,
			const unsigned int cacheRadius = sectorRadius);

	static Json LoadGameToJson(const std::string &filename);
	// LoadGame and SaveGame throw exceptions on failure
	static void LoadGame(const std::string &filename);
	static bool CanLoadGame(const std::string &filename);
	static void SaveGame(const std::string &filename);

protected:

private:
};

#endif // GAMESTATE_H
