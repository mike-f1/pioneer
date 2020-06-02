// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "SoundMusic.h"

#include "LuaEvent.h"
#include <map>

#include "profiler/Profiler.h"

float Sound::MusicPlayer::m_volume = 0.0;
Sound::MusicEvent Sound::MusicPlayer::m_eventOne;
Sound::MusicEvent Sound::MusicPlayer::m_eventTwo;
bool Sound::MusicPlayer::m_playing = false;
bool Sound::MusicPlayer::m_eventOnePlaying = false;
std::string Sound::MusicPlayer::m_currentSongName = "";
bool Sound::MusicPlayer::m_enabled = false;

namespace Sound {

	MusicEvent::MusicEvent() :
		Event() {}

	MusicEvent::MusicEvent(uint32_t id) :
		Event(id) {}

	MusicEvent::~MusicEvent() {}

	void MusicEvent::Play(const char *fx, const float volume_left, const float volume_right, Op op)
	{
		Stop();
		eid = PlayMusic(fx, volume_left, volume_right, op);
	}

	void MusicPlayer::Init()
	{
		m_volume = 0.8f;
		m_playing = false;
		m_eventOnePlaying = false;
		m_currentSongName = "";
		m_enabled = true;
	}

	float MusicPlayer::GetVolume()
	{
		return m_volume;
	}

	void MusicPlayer::SetVolume(const float vol)
	{
		m_volume = std::min(std::max(vol, 0.f), 1.f);
		//the other song might be fading out so don't set its volume
		if (m_eventOnePlaying && m_eventOne.IsPlaying())
			m_eventOne.SetVolume(m_volume);
		else if (m_eventTwo.IsPlaying())
			m_eventTwo.SetVolume(m_volume);
	}

	void MusicPlayer::Play(const std::string &name, const bool repeat /* = false */, const float fadeDelta /* = 1.f */)
	{
		if (!m_enabled) return;
		Sound::Op op = 0;
		if (repeat)
			op |= Sound::OP_REPEAT;
		if (m_eventOnePlaying) {
			m_eventOne.VolumeAnimate(0.f, 0.f, fadeDelta, fadeDelta);
			m_eventOne.SetOp(Sound::OP_STOP_AT_TARGET_VOLUME);
			m_eventTwo.Play(name.c_str(), 0.f, 0.f, op);
			m_eventTwo.VolumeAnimate(m_volume, m_volume, fadeDelta, fadeDelta);
			m_eventOnePlaying = false;
		} else {
			m_eventTwo.VolumeAnimate(0.f, 0.f, fadeDelta, fadeDelta);
			m_eventTwo.SetOp(Sound::OP_STOP_AT_TARGET_VOLUME);
			m_eventOne.Play(name.c_str(), 0.f, 0.f, op);
			m_eventOne.VolumeAnimate(m_volume, m_volume, fadeDelta, fadeDelta);
			m_eventOnePlaying = true;
		}
		m_playing = true;
		m_currentSongName = name;
	}

	void MusicPlayer::Stop()
	{
		m_eventOne.Stop();
		m_eventTwo.Stop();
		m_playing = false;
	}

	void MusicPlayer::FadeOut(const float fadeDelta)
	{
		if (m_eventOnePlaying) { //2 might be already fading out
			m_eventOne.SetOp(Sound::OP_STOP_AT_TARGET_VOLUME);
			m_eventOne.VolumeAnimate(0.f, 0.f, fadeDelta, fadeDelta);
		} else { // 1 might be already fading out
			m_eventTwo.SetOp(Sound::OP_STOP_AT_TARGET_VOLUME);
			m_eventTwo.VolumeAnimate(0.f, 0.f, fadeDelta, fadeDelta);
		}
	}

	void MusicPlayer::Update()
	{
		PROFILE_SCOPED()
		if (m_playing) { //expecting report
			if ((m_eventOnePlaying && !m_eventOne.IsPlaying()) || (!m_eventOnePlaying && !m_eventTwo.IsPlaying())) {
				m_playing = false;
				LuaEvent::Queue("onSongFinished");
			}
		}
	}

	const std::string MusicPlayer::GetCurrentSongName()
	{
		return m_currentSongName;
	}

	const std::vector<std::string> MusicPlayer::GetSongList()
	{
		using std::pair;
		using std::string;
		std::vector<string> songs;
		const std::map<string, Sample> &samples = Sound::GetSamples();
		for (std::map<string, Sample>::const_iterator it = samples.begin();
			 it != samples.end(); ++it) {
			if (it->second.isMusic)
				songs.push_back(it->first.c_str());
		}

		return songs;
	}

	bool MusicPlayer::IsPlaying()
	{
		return (m_eventOne.IsPlaying() || m_eventTwo.IsPlaying());
	}

	void MusicPlayer::SetEnabled(const bool en)
	{
		m_enabled = en;
		if (!en && IsPlaying()) Stop();
	}

} // namespace Sound
