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
#include "Json.h"
#include "input/Input.h"
#include "input/InputLocator.h"
#include "Player.h"
#include "Space.h"
#include "galaxy/StarSystem.h"
#include "libs/utils.h"

#include <chrono>

static const int s_saveVersion = 90;

struct {
	bool operator()(const FileSystem::FileInfo& a, const FileSystem::FileInfo& b) const {
		return a.GetModificationTime() > b.GetModificationTime();
	}
} saves_modtime_comparator;

GameStateStatic::jsonSave canLoadGame(const FileSystem::FileInfo &fi)
{
	Json rootNode = JsonUtils::LoadJsonSaveFile(fi.Read());

	if (!rootNode.is_object()) return { {}, false };
	if (!rootNode["version"].is_number_integer() || rootNode["version"].get<int>() != s_saveVersion) return { {}, false };

	return { rootNode, true };
}

GameStateStatic::AutoThread::AutoThread() : m_active(true),
	m_savefiles_watcher([&]() {
		while (m_active) {
			std::this_thread::sleep_for(std::chrono::seconds(1));

			if (!FileSystem::userFiles.MakeDirectory(GameConfSingleton::GetSaveDir())) {
				throw CouldNotOpenFileException();
			}

			t_vec_fileinfo files;
			FileSystem::userFiles.ReadDirectory(GameConfSingleton::GetSaveDir(), files);

			m_preloaded_savefiles_lock.store(true);
			// Remove erased files:
			for (auto it = begin(m_preloaded_savefiles); it != end(m_preloaded_savefiles);/*nothing*/) {
				const auto &found = find_if(begin(files), end(files), [&it](const auto &new_fi) {
					return (it->first.GetName() == new_fi.GetName());
				});
				if (found == files.end()) {
					it = m_preloaded_savefiles.erase(it);
				} else {
					it++;
				}
			}
			// Look for added or changed files:
			std::for_each(begin(files), end(files), [&](const auto &new_fi) {
				const auto found = find_if(begin(m_preloaded_savefiles), end(m_preloaded_savefiles), [&new_fi](const auto &pair) {
					return (pair.first.GetName() == new_fi.GetName());
				});
				if (found == m_preloaded_savefiles.end()) {
					// No same name found, try loading and store
					m_preloaded_savefiles[new_fi] = canLoadGame(new_fi);
				} else {
					// Name is equal,it's an overwrite?
					if ((*found).first.GetModificationTime() != new_fi.GetModificationTime()) {
						m_preloaded_savefiles.erase(found);
						m_preloaded_savefiles[new_fi] = canLoadGame(new_fi);
					}
				}
			});
			m_preloaded_savefiles_lock.store(false);
		};
		Output("Exiting savefiles monitor...\n");
	})
{
	Output("Savefiles monitor up and running...\n");
}

GameStateStatic::AutoThread GameStateStatic::savefiles_watcher;

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

	// TODO: Sub optimal: need a better way to couple inGameViews to game
	InGameViewsLocator::NewInGameViews(new InGameViews(game, path, sectorRadius_));

	// Here because 'l_game_attr_player' would have
	// a player to be pushed on Lua VM through GameLocator,
	// but that is not yet set in a ctor
	game->EmitPauseState(game->IsPaused());
}

const Json GameStateStatic::PickJsonLoadGame(const std::string &filename)
{
	const auto found = std::find_if(begin(m_preloaded_savefiles), end(m_preloaded_savefiles), [&filename](const auto &pair) {
		return (pair.first.GetName() == filename);
	});

	if (found != m_preloaded_savefiles.end()) return (*found).second.value;
	else return {};
}

t_vec_fileinfo GameStateStatic::ReadFilesaveDir()
{
	while (m_preloaded_savefiles_lock.load()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	t_vec_fileinfo savefiles;
	savefiles.reserve(m_preloaded_savefiles.size());

	std::for_each(begin(m_preloaded_savefiles), end(m_preloaded_savefiles), [&savefiles](const auto &pair) {
		if (pair.second.valid) savefiles.push_back(pair.first);
	});

	return savefiles;
}

std::optional<std::string> GameStateStatic::FindMostRecentSaveGame()
{
	t_vec_fileinfo savefiles = ReadFilesaveDir();

	if (savefiles.empty()) return {};

	const auto most_recent = std::min_element(begin(savefiles), end(savefiles), saves_modtime_comparator);

	return (*most_recent).GetName();
}

std::optional<t_vec_fileinfo> GameStateStatic::CollectSaveGames()
{
	t_vec_fileinfo savefiles = ReadFilesaveDir();

	if (savefiles.empty()) return {};

	std::sort(begin(savefiles), end(savefiles), saves_modtime_comparator);

	return savefiles;
}

void GameStateStatic::LoadGame(const std::string &filename)
{
	Output("Game::LoadGame('%s')\n", filename.c_str());

	Json rootNode = PickJsonLoadGame(filename);

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

	auto save = [&filename, &jsonData]() {
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
	};
	save();
#ifdef PIONEER_PROFILER
	Profiler::dumphtml(profilerPath.c_str());
#endif
}
