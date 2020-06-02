// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _MUSIC_H
#define _MUSIC_H

#include "Sound.h"

#include <string>
#include <vector>

namespace Sound {
	class MusicEvent : public Event {
	public:
		MusicEvent();
		MusicEvent(uint32_t id);
		~MusicEvent();
		virtual void Play(const char *fx, const float volume_left, const float volume_right, Op op) override;
	};

	class MusicPlayer {
	public:
		MusicPlayer() = delete;
		~MusicPlayer() = delete;

		static void Init();
		static float GetVolume();
		static void SetVolume(const float);
		static void Play(const std::string &, const bool repeat = false, const float fadeDelta = 1.f);
		static void Stop();
		static void FadeOut(const float fadeDelta);
		static void Update();
		static const std::string GetCurrentSongName();
		static const std::vector<std::string> GetSongList();
		static bool IsPlaying();
		static void SetEnabled(bool);

	private:
		static float m_volume;
		//two streams for crossfade
		static MusicEvent m_eventOne;
		static MusicEvent m_eventTwo;
		static bool m_playing;
		static bool m_eventOnePlaying;
		static std::string m_currentSongName;
		static bool m_enabled;
	};
} // namespace Sound

#endif
