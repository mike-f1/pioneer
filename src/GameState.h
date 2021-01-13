#ifndef GAMESTATE_H
#define GAMESTATE_H

#include "DateTime.h"
#include "FileSystem.h"
#include "Json.h"
#include <string>
#include <optional>
#include <thread>

class SystemPath;

static const unsigned int sectorRadius = 5;

using t_vec_fileinfo = std::vector<FileSystem::FileInfo>;

class GameStateStatic
{
public:
	GameStateStatic() = delete;

	static void MakeNewGame(const SystemPath &path,
			const double startDateTime = 0.0,
			const unsigned int cacheRadius = sectorRadius);

	static const Json PickJsonLoadGame(const std::string &filename);
	static std::optional<std::string> FindMostRecentSaveGame();
	static std::optional<std::vector<FileSystem::FileInfo>> CollectSaveGames();
	// LoadGame and SaveGame throw exceptions on failure
	static void LoadGame(const std::string &filename);
	static void SaveGame(const std::string &filename);

	struct jsonSave {
		Json value;
		bool valid;
	};
protected:

private:
	static t_vec_fileinfo ReadFilesaveDir();

	using t_gamesavePreloader = std::map<FileSystem::FileInfo, jsonSave>;

	inline static std::atomic<bool> m_preloaded_savefiles_lock = false;
	inline static t_gamesavePreloader m_preloaded_savefiles;

	class AutoThread {
	public:
		AutoThread();
		~AutoThread() { m_active = false; m_savefiles_watcher.join(); };
	private:
		std::atomic_bool m_active;
		std::thread m_savefiles_watcher;
	};
	static AutoThread savefiles_watcher;
};

#endif // GAMESTATE_H
