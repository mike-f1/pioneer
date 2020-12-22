#ifndef INIT_STATE_H
#define INIT_STATE_H

#include "PiState.h"

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace MainState_ {

	class InitState: public PiState {
	public:
		InitState(const std::map<std::string, std::string> &options, bool no_gui = false);
		virtual ~InitState();

		PiState *Update() override final;

	private:
		const std::map<std::string, std::string> m_options;
		const bool m_no_gui;

		static void RegisterInputBindings();

		inline static struct PiBinding {
			ActionId quickSave;
			ActionId reqQuit;
			ActionId screenShot;
			ActionId toggleVideoRec;
			#ifdef WITH_DEVKEYS
			ActionId toggleDebugInfo;
			ActionId reloadShaders;
			#endif // WITH_DEVKEYS
			#ifdef PIONEER_PROFILER
			ActionId profilerBindSlow;
			ActionId profilerBindOne;
			#endif
			#ifdef WITH_OBJECTVIEWER
			ActionId objectViewer;
			#endif // WITH_OBJECTVIEWER
		} m_piBindings;

		// Key-press action feedback functions
		static void QuickSave(bool down);
		static void ScreenShot(bool down);
		static void ToggleVideoRecording(bool down);
	};

} // namespace MainState_
#endif // INIT_STATE_H
