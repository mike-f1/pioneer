#ifndef PISTATE_H
#define PISTATE_H

#include "buildopts.h"

#include <memory>

#include "input/InputFwd.h"

class Cutscene;
class DebugInfo;
class InputFrame;

enum class MainState {
	MAIN_MENU,
	GAME_LOOP,
	TOMBSTONE,
	QUITTING,
};

namespace MainState_ {

	class PiState {
	public:
		PiState();
		virtual ~PiState();
		PiState(const PiState& rhs) = delete;
		PiState& operator=(const PiState& rhs) = delete;

		virtual PiState *Update() = 0;

		static float GetFrameTime() { return m_frameTime; }
		static float GetGameTickAlpha() { return m_gameTickAlpha; }

	protected:
		bool HandleEscKey();
		void HandleEvents();

		void CutSceneLoop(MainState &current, double step, Cutscene *m_cutscene);

		/** So, the game physics rate (50Hz) can run slower
		  * than the frame rate. m_gameTickAlpha is the interpolation
		  * factor between one physics tick and another [0.0-1.0]
		  */
		inline static float m_gameTickAlpha = 0.;
		inline static float m_frameTime = 0.;

		std::unique_ptr<DebugInfo> m_debugInfo;

	private:
		void RegisterInputBindings();
		// Key-press action feedback functions
		void QuickSave(bool down);
		void ScreenShot(bool down);
		void ToggleVideoRecording(bool down);

		struct PiBinding {
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

		std::unique_ptr<InputFrame> m_inputFrame;
	};

	class QuittingState: public PiState {
	public:
		QuittingState() {};
		~QuittingState() {};

		PiState *Update() override final { delete this; return nullptr; }

	private:
	};

} // namespace MainState
#endif // PISTATE_H
