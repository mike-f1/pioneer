#ifndef VIDEORECORDER_H
#define VIDEORECORDER_H

#include <cstdint>
#include <cstdio>

class VideoRecorder
{
public:
	VideoRecorder();
	~VideoRecorder();
	VideoRecorder(const VideoRecorder& rhs) = delete;
	VideoRecorder& operator=(const VideoRecorder& rhs) = delete;

	void NewFrame(void *buffer, uint32_t size);
private:

	FILE *m_ffmpegFile;
};

#endif // VIDEORECORDER_H
