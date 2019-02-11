#pragma once
#include <string>

extern "C" {
#define __STDC_CONSTANT_MACROS

//#include <libavutil/avassert.h>
//#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
//#include <libavutil/mathematics.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
//#include <libswresample/swresample.h>

}

class FFMpegVideoEncoder
{
private:
	int _fps;
	AVOutputFormat *_oformat;
	std::string _outputFile;
	AVFormatContext *_formatCtx;
	AVCodec *_codec;
	AVCodecContext *_codecCtx;
	AVStream *_videoStream;
	AVFrame *_videoFrame;

	SwsContext *_swsCtx;
	int _frameCounter;

	void Free();

public:
	FFMpegVideoEncoder();
	~FFMpegVideoEncoder();

	void Init(int width, int height, int fpsrate, int bitrate, std::string outputFileName);

	void AddFrame(uint8_t *data);

	void Finish();
};

