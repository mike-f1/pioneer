#include "VideoRecorder.h"

#include <string>
#include <time.h>
#include "libs/utils.h"

#include "FileSystem.h"
#include "GameConfig.h"
#include "GameConfSingleton.h"

#if !defined(_MSC_VER) && !defined(__MINGW32__)
#define _popen popen
#define _pclose pclose
#endif

VideoRecorder::VideoRecorder()
{
	char videoName[256];
	const time_t t = time(0);
	struct tm *_tm = localtime(&t);
	strftime(videoName, sizeof(videoName), "pioneer-%Y%m%d-%H%M%S", _tm);
	const std::string dir = "videos";
	FileSystem::userFiles.MakeDirectory(dir);
	const std::string fname = FileSystem::JoinPathBelow(FileSystem::userFiles.GetRoot() + "/" + dir, videoName);
	Output("Video Recording started to %s.\n", fname.c_str());
	// start ffmpeg telling it to expect raw rgba 720p-60hz frames
	// -i - tells it to read frames from stdin
	// if given no frame rate (-r 60), it will just use vfr
	char cmd[256] = { 0 };
	snprintf(cmd, sizeof(cmd), "ffmpeg -f rawvideo -pix_fmt rgba -s %dx%d -i - -threads 0 -preset fast -y -pix_fmt yuv420p -crf 21 -vf vflip %s.mp4",
		GameConfSingleton::getInstance().Int("ScrWidth"),
		GameConfSingleton::getInstance().Int("ScrHeight"),
		fname.c_str()
		);

	// open pipe to ffmpeg's stdin in binary write mode
#if defined(_MSC_VER) || defined(__MINGW32__)
	m_ffmpegFile = _popen(cmd, "wb");
#else
	m_ffmpegFile = _popen(cmd, "w");
#endif
}

VideoRecorder::~VideoRecorder()
{
	Output("Video Recording ended.\n");
	if (m_ffmpegFile != nullptr) {
		_pclose(m_ffmpegFile);
		m_ffmpegFile = nullptr;
	}
}

void VideoRecorder::NewFrame(void *buffer, uint32_t size)
{
	fwrite(buffer, size, 1, m_ffmpegFile);
}
