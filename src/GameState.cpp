#include "buildopts.h"

#include "GameState.h"

#include "Json.h"
#include "JsonUtils.h"
#include "FileSystem.h"
#include "Game.h"
#include "GameConfSingleton.h"
#include "GameLocator.h"
#include "GameSaveError.h"
#include "GZipFormat.h"
#include "Player.h"
#include "utils.h"

static const int s_saveVersion = 87;

void GameState::MakeNewGame(const SystemPath &path, const double startDateTime)
{
	Output("Starting new game at (%i;%i;%i;%i;%i)\n", path.sectorX, path.sectorY, path.sectorZ, path.systemIndex, path.bodyIndex);
	Game *game = new Game(path, startDateTime);
	GameLocator::provideGame(game);
	// Here because 'l_game_attr_player' would have
	// a player to be pushed on Lua VM through GameLocator,
	// but that is not yet set in a ctor
	game->EmitPauseState(game->IsPaused());
}

Json GameState::LoadGameToJson(const std::string &filename)
{
	Json rootNode = JsonUtils::LoadJsonSaveFile(FileSystem::JoinPathBelow(GameConfSingleton::GetSaveDir(), filename), FileSystem::userFiles);
	if (!rootNode.is_object()) {
		Output("Loading saved game '%s' failed.\n", filename.c_str());
		throw SavedGameCorruptException();
	}
	if (!rootNode["version"].is_number_integer() || rootNode["version"].get<int>() != s_saveVersion) {
		Output("Loading saved game '%s' failed: wrong save file version.\n", filename.c_str());
		throw SavedGameCorruptException();
	}
	return rootNode;
}

void GameState::LoadGame(const std::string &filename)
{
	Output("Game::LoadGame('%s')\n", filename.c_str());

	Json rootNode = LoadGameToJson(filename);

	try {
		int version = rootNode["version"];
		Output("savefile version: %d\n", version);
		if (version != s_saveVersion) {
			Output("can't load savefile, expected version: %d\n", s_saveVersion);
			throw SavedGameWrongVersionException();
		}
	} catch (Json::type_error &) {
		throw SavedGameCorruptException();
	}

	Game *game = nullptr;

	try {
		 game = new Game(rootNode);
	} catch (Json::type_error) {
		throw SavedGameCorruptException();
	} catch (Json::out_of_range) {
		throw SavedGameCorruptException();
	}

	GameLocator::provideGame(game);
}

bool GameState::CanLoadGame(const std::string &filename)
{
	auto file = FileSystem::userFiles.ReadFile(FileSystem::JoinPathBelow(GameConfSingleton::GetSaveDir(), filename));
	if (!file)
		return false;

	return true;
	// file data is freed here
}

void GameState::SaveGame(const std::string &filename)
{
	PROFILE_SCOPED()

	Game *game = GameLocator::getGame();
	assert(game);

	if (game->IsHyperspace())
		throw CannotSaveInHyperspace();

	if (game->GetPlayer()->IsDead())
		throw CannotSaveDeadPlayer();

	if (!FileSystem::userFiles.MakeDirectory(GameConfSingleton::GetSaveDir())) {
		throw CouldNotOpenFileException();
	}

#ifdef PIONEER_PROFILER
	std::string profilerPath;
	FileSystem::userFiles.MakeDirectory("profiler");
	FileSystem::userFiles.MakeDirectory("profiler/saving");
	profilerPath = FileSystem::JoinPathBelow(FileSystem::userFiles.GetRoot(), "profiler/saving");
	Profiler::reset();
#endif

	Json rootNode; // Create the root JSON value for receiving the game data.

	// version
	rootNode["version"] = s_saveVersion;

	game->ToJson(rootNode); // Encode the game data as JSON and give to the root value.
	std::vector<uint8_t> jsonData;
	{
		PROFILE_SCOPED_DESC("json.to_cbor");
		jsonData = Json::to_cbor(rootNode); // Convert the JSON data to CBOR.
	}

	FILE *f = FileSystem::userFiles.OpenWriteStream(FileSystem::JoinPathBelow(GameConfSingleton::GetSaveDir(), filename));
	if (!f) throw CouldNotOpenFileException();

	try {
		// Compress the CBOR data.
		const std::string comressed_data = gzip::CompressGZip(
			std::string(reinterpret_cast<const char *>(jsonData.data()), jsonData.size()),
			filename + ".json");
		size_t nwritten = fwrite(comressed_data.data(), comressed_data.size(), 1, f);
		fclose(f);
		if (nwritten != 1) throw CouldNotWriteToFileException();
	} catch (gzip::CompressionFailedException) {
		fclose(f);
		throw CouldNotWriteToFileException();
	}

#ifdef PIONEER_PROFILER
	Profiler::dumphtml(profilerPath.c_str());
#endif
}

void GameState::DestroyGame()
{
	if (GameLocator::getGame() == nullptr) {
		Output("Attempt to destroy a not existing Game!\n");
		return;
	}
	delete GameLocator::getGame();
	GameLocator::provideGame(nullptr);
}
