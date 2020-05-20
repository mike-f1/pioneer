// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "ModelViewer.h"
#include "Pi.h"
#include "buildopts.h"
#include "galaxy/Galaxy.h"
#include "galaxy/GalaxyGenerator.h"
#include "libs/stringUtils.h"
#include "libs/utils.h"
#include "versioningInfo.h"
#include <cstdio>
#include <cstdlib>

enum class RunMode {
	GAME,
	MODELVIEWER,
	GALAXYDUMP,
	START_AT,
	VERSION,
	USAGE,
	USAGE_ERROR
};

extern "C" int main(int argc, char **argv)
{
#ifdef PIONEER_PROFILER
	Profiler::detect(argc, argv);
#endif

	RunMode mode = RunMode::GAME;
	std::string modeopt;

	if (argc > 1) {
		const char switchchar = argv[1][0];
		if (!(switchchar == '-' || switchchar == '/')) {
			mode = RunMode::USAGE_ERROR;
			goto start;
		}

		modeopt = std::string(argv[1]).substr(1);

		if (modeopt == "game" || modeopt == "g") {
			mode = RunMode::GAME;
			goto start;
		}

		if (modeopt == "modelviewer" || modeopt == "mv") {
			mode = RunMode::MODELVIEWER;
			goto start;
		}

		if (modeopt == "galaxydump" || modeopt == "gd") {
			mode = RunMode::GALAXYDUMP;
			goto start;
		}

		if (modeopt.find("startat", 0, 7) != std::string::npos ||
			modeopt.find("sa", 0, 2) != std::string::npos) {
			mode = RunMode::START_AT;
			goto start;
		}

		if (modeopt == "version" || modeopt == "v") {
			mode = RunMode::VERSION;
			goto start;
		}

		if (modeopt == "help" || modeopt == "h" || modeopt == "?") {
			mode = RunMode::USAGE;
			goto start;
		}

		mode = RunMode::USAGE_ERROR;
	}

start:

	int pos = 2;
	long int radius = 4;
	long int sx = 0, sy = 0, sz = 0;
	std::string filename;
	SystemPath startPath(0, 0, 0, 0, 0);

	switch (mode) {
	case RunMode::GALAXYDUMP: {
		if (argc < 3) {
			Output("pioneer: galaxy dump requires a filename\n");
			break;
		}
		filename = argv[pos];
		++pos;
		if (argc > pos) { // radius (optional)
			char *end = nullptr;
			radius = std::strtol(argv[pos], &end, 0);
			if (end == nullptr || *end != 0 || radius < 0 || radius > 10000) {
				Output("pioneer: invalid radius: %s\n", argv[pos]);
				break;
			}
			++pos;
		}
		if (argc > pos) { // center of dump (three comma separated coordinates, optional)
			char *end = nullptr;
			sx = std::strtol(argv[pos], &end, 0);
			if (end == nullptr || *end != ',' || sx < -10000 || sx > 10000) {
				Output("pioneer: invalid center: %s\n", argv[pos]);
				break;
			}
			sy = std::strtol(end + 1, &end, 0);
			if (end == nullptr || *end != ',' || sy < -10000 || sy > 10000) {
				Output("pioneer: invalid center: %s\n", argv[pos]);
				break;
			}
			sz = std::strtol(end + 1, &end, 0);
			if (end == nullptr || *end != 0 || sz < -10000 || sz > 10000) {
				Output("pioneer: invalid center: %s\n", argv[pos]);
				break;
			}
			++pos;
		}
		// fallthrough
	}
	case RunMode::START_AT: {
		// fallthrough protect
		if (mode == RunMode::START_AT) {
			// try to get start planet number
			std::vector<std::string> keyValue = stringUtils::SplitString(modeopt, "=");

			// if found value
			if (keyValue.size() == 2) {
				if (keyValue[1].empty()) {
					startPath = SystemPath(0, 0, 0, 0, 0);
					Error("Please provide an actual SystemPath, like 0,0,0,0,18\n");
					return -1;
				} else {
					try {
						startPath = SystemPath::Parse(keyValue[1].c_str());
					} catch (const SystemPath::ParseFailure &spf) {
						startPath = SystemPath(0, 0, 0, 0, 0);
						Error("Failed to parse system path %s\n", keyValue[1].c_str());
						return -1;
					}
				}
			}
			// if value not exists - start on Sol, Mars, Cydonia
			else
				startPath = SystemPath(0, 0, 0, 0, 18);
			// set usual mode
			mode = RunMode::GAME;
		}
		// fallthrough
	}
	case RunMode::GAME: {
		std::map<std::string, std::string> options;

		// if arguments more than parsed already
		if (argc > pos) {
			static const std::string delim("=");

			// for each argument
			for (; pos < argc; pos++) {
				const std::string arg(argv[pos]);
				std::vector<std::string> keyValue = stringUtils::SplitString(arg, "=");

				// if there no key and value || key is empty || value is empty
				if (keyValue.size() != 2 || keyValue[0].empty() || keyValue[1].empty()) {
					Output("malformed option: %s\n", arg.c_str());
					return 1;
				}

				// put key and value to config
				options[keyValue[0]] = keyValue[1];
			}
		}

		Pi::Init(options, mode == RunMode::GALAXYDUMP);

		if (mode == RunMode::GAME)
			for (;;) {
				Pi::Start(startPath);
				startPath = SystemPath(0, 0, 0, 0, 0); // Reset the start planet when coming back to the menu
			}
		else if (mode == RunMode::GALAXYDUMP) {
			FILE *file = filename == "-" ? stdout : fopen(filename.c_str(), "w");
			if (file == nullptr) {
				Output("pioneer: could not open \"%s\" for writing: %s\n", filename.c_str(), strerror(errno));
				break;
			}
			RefCountedPtr<Galaxy> galaxy = GalaxyGenerator::Create();
			galaxy->Dump(file, sx, sy, sz, radius);
			if (filename != "-" && fclose(file) != 0) {
				Output("pioneer: writing to \"%s\" failed: %s\n", filename.c_str(), strerror(errno));
			}
		}
		break;
	}

	case RunMode::MODELVIEWER: {
		std::string modelName;
		if (argc > 2)
			modelName = argv[2];
		ModelViewer::Run(modelName);
		break;
	}

	case RunMode::VERSION: {
		std::string version(PIONEER_VERSION);
		if (strlen(PIONEER_EXTRAVERSION)) version += " (" PIONEER_EXTRAVERSION ")";
		Output("pioneer %s\n", version.c_str());
		OutputVersioningInfo();
		break;
	}

	case RunMode::USAGE_ERROR:
		Output("pioneer: unknown mode %s\n", argv[1]);
		// fall through

	case RunMode::USAGE:
		Output(
			"usage: pioneer [mode] [options...]\n"
			"available modes:\n"
			"    -game        [-g]     game (default)\n"
			"    -modelviewer [-mv]    model viewer\n"
			"    -galaxydump  [-gd]    galaxy dumper\n"
			"    -startat     [-sa]    skip main menu and start at Mars\n"
			"    -startat=sp  [-sa=sp]  skip main menu and start at systempath x,y,z,si,bi\n"
			"    -version     [-v]     show version\n"
			"    -help        [-h,-?]  this help\n");
		break;
	}

	return 0;
}
