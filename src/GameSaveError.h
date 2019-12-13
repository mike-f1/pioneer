// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef GAME_SAVE_ERROR_H
#define GAME_SAVE_ERROR_H

#include <string>

struct CannotSaveCurrentGameState {};
struct CannotSaveInHyperspace : public CannotSaveCurrentGameState {};
struct CannotSaveDeadPlayer : public CannotSaveCurrentGameState {};
struct InvalidGameStartLocation {
	std::string error;
	InvalidGameStartLocation(const std::string &error_) :
		error(error_) {}
};

struct SavedGameCorruptException {};
struct SavedGameWrongVersionException {};
struct CouldNotOpenFileException {};
struct CouldNotWriteToFileException {};

#endif
