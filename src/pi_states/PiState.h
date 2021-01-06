#ifndef PISTATE_H
#define PISTATE_H

#include "buildopts.h"

#include <memory>

#include "input/InputFwd.h"

class Cutscene;
class DebugInfo;
class InputFrame;
class VideoRecorder;

enum class MainState {
	INIT_STATE,
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

		static float GetFrameTime() { return m_statelessVars.frameTime; }
		static float GetGameTickAlpha() { return m_statelessVars.gameTickAlpha; }

	protected:
		bool HandleEscKey();
		void HandleEvents();

		void CutSceneLoop(double step, Cutscene *m_cutscene);

		/* That struct represent shared variable  which survive state changes,
		 *  Thus they are defined as static and grouped here.
		 */
		static struct StateLessVar {
			/** So, the game physics rate (50Hz) can run slower
			  * than the frame rate. gameTickAlpha is the interpolation
			  * factor between one physics tick and another [0.0-1.0]
			  */
			float gameTickAlpha = 0.;
			float frameTime = 0.;
			std::unique_ptr<DebugInfo> debugInfo;
			std::unique_ptr<VideoRecorder> videoRecorder;
			std::unique_ptr<InputFrame> inputFrame;
#if PIONEER_PROFILER
			std::string profilerPath;
			bool doProfileSlow = false;
			bool doProfileOne = false;
#endif
		} m_statelessVars;

	private:
	};

} // namespace MainState_

#endif // PISTATE_H
