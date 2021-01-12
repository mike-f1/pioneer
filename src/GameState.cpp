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
#include "InGameViews.h"
#include "InGameViewsLocator.h"
#include "input/Input.h"
#include "input/InputLocator.h"
#include "Player.h"
#include "Space.h"
#include "galaxy/StarSystem.h"
#include "libs/utils.h"

static const int s_saveVersion = 90;

bool canLoadGame(const std::string &filename)
{
	auto file = FileSystem::userFiles.ReadFile(FileSystem::JoinPathBelow(GameConfSingleton::GetSaveDir(), filename));
	if (!file)
		return false;

	return true;
	// file data is freed here
}

void GameStateStatic::MakeNewGame(const SystemPath &path,
		const double startDateTime,
		const unsigned int sectorRadius_)
{
	Output("Starting new game at (%i;%i;%i;%i;%i)\n", path.sectorX, path.sectorY, path.sectorZ, path.systemIndex, path.bodyIndex);
	Game *game = new Game(path, startDateTime, sectorRadius_);

	// TODO: Set locator before InGameViews because it seems there some
	// calls to GameLocator during initialization... :P
	GameLocator::provideGame(game);

	InputLocator::getInput()->InitGame();

	// Sub optimal: need a better way to couple inGameViews to game
	InGameViewsLocator::NewInGameViews(new InGameViews(game, path, sectorRadius_));

	// Here because 'l_game_attr_player' would have
	// a player to be pushed on Lua VM through GameLocator,
	// but that is not yet set in a ctor
	game->EmitPauseState(game->IsPaused());
}

Json GameStateStatic::LoadGameToJson(const std::string &filename)
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

std::vector<FileSystem::FileInfo> updateFilesaveDir()
{
	std::vector<FileSystem::FileInfo> files;
	FileSystem::userFiles.ReadDirectory(GameConfSingleton::GetSaveDir(), files);
	return files;
}

std::optional<std::vector<FileSystem::FileInfo>> GameStateStatic::ReadFilesaveDir()
{
	// Ensure save dir exist
	if (!FileSystem::userFiles.MakeDirectory(GameConfSingleton::GetSaveDir())) {
		throw CouldNotOpenFileException();
	}

	FileSystem::FileInfo filesave_dir = FileSystem::userFiles.Lookup(GameConfSingleton::GetSaveDir());
	if (filesave_dir.GetModificationTime() != m_last_access_to_saves) {
		m_last_access_to_saves = filesave_dir.GetModificationTime();

		//TODO: Better algorithm here, there's need to check for differencse instead of discarding
		// the old list...
		m_savefiles = updateFilesaveDir();

		std::copy_if(begin(m_savefiles), end(m_savefiles), begin(m_savefiles), [](const FileSystem::FileInfo &fi) {
			if (!fi.IsFile()) return false;
			return canLoadGame(fi.GetName());
		});
	} else {
		//Output("No updates, keep the same files...\n");
	}

	if (m_savefiles.empty()) return {};
	return m_savefiles;
}

std::optional<std::string> GameStateStatic::FindMostRecentSaveGame()
{
	std::optional<std::vector<FileSystem::FileInfo>> files = ReadFilesaveDir();

	if (!files) return {};

	std::vector<FileSystem::FileInfo>::iterator min_el = std::min_element(
		begin((*files)), end((*files)), [](const FileSystem::FileInfo &first, const FileSystem::FileInfo &second) {
		return first.GetModificationTime() > second.GetModificationTime();
	});

	return (*min_el).GetName();
}

std::optional<std::vector<FileSystem::FileInfo>> GameStateStatic::CollectSaveGames()
{
	std::optional<std::vector<FileSystem::FileInfo>> files = ReadFilesaveDir();

	if (!files) return {};

	std::sort(begin((*files)), end((*files)), [] (const FileSystem::FileInfo &first, const FileSystem::FileInfo &second) {
		return first.GetModificationTime() > second.GetModificationTime();
	});
	return files;
}

void GameStateStatic::LoadGame(const std::string &filename)
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
		 game = new Game(rootNode, sectorRadius);

		InputLocator::getInput()->InitGame();

		// Sub optimal: need a better way to couple inGameViews to game
		const SystemPath &path = game->GetSpace()->GetStarSystem()->GetPath();
		InGameViewsLocator::NewInGameViews(new InGameViews(rootNode, game, path, sectorRadius + 2));
	} catch (Json::type_error) {
		throw SavedGameCorruptException();
	} catch (Json::out_of_range) {
		throw SavedGameCorruptException();
	}

	GameLocator::provideGame(game);
}

void GameStateStatic::SaveGame(const std::string &filename)
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
	InGameViewsLocator::SaveInGameViews(rootNode);

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
