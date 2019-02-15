#include "stdafx.h"
#include "FFMpegVideoEncoder.h"
#include <stdexcept>

FFMpegVideoEncoder::FFMpegVideoEncoder()
{
}


FFMpegVideoEncoder::~FFMpegVideoEncoder()
{
}

void FFMpegVideoEncoder::Init(int width, int height, int fpsrate, int bitrate, std::string outputFileName)
{
	_fps = fpsrate;
	_outputFile = outputFileName;

	int err;

	if (!(_oformat = av_guess_format(NULL, _outputFile.c_str(), NULL))) {
		throw std::runtime_error("FFMPEG Failed to define output format");
	}

	if ((err = avformat_alloc_output_context2(&_formatCtx, _oformat, NULL, _outputFile.c_str()) < 0)) {
		avformat_free_context(_formatCtx);
		throw std::runtime_error("Failed to allocate output context" + std::to_string(err));
	}

	if (!(_codec = avcodec_find_encoder(_oformat->video_codec))) {
		avformat_free_context(_formatCtx);
		throw std::runtime_error("Failed to find encoder");
	}

	if (!(_videoStream = avformat_new_stream(_formatCtx, _codec))) {
		avformat_free_context(_formatCtx);
		throw std::runtime_error("Failed to create new stream");
	}

	if (!(_codecCtx = avcodec_alloc_context3(_codec))) {
		// free video stream ?
		// free codec ?
		avformat_free_context(_formatCtx);
		throw std::runtime_error("Failed to allocate codec context");
	}

	_videoStream->codecpar->codec_id = _oformat->video_codec;
	_videoStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
	_videoStream->codecpar->width = width;
	_videoStream->codecpar->height = height;
	_videoStream->codecpar->format = AV_PIX_FMT_YUV420P;
	_videoStream->codecpar->bit_rate = bitrate * 1000;
	
	AVRational timeBase;
	timeBase.den = _fps;
	timeBase.num = 1;
	_videoStream->time_base = timeBase;

	avcodec_parameters_to_context(_codecCtx, _videoStream->codecpar);
	_codecCtx->time_base = timeBase;
	_codecCtx->max_b_frames = 2;
	_codecCtx->gop_size = 12;
	if (_videoStream->codecpar->codec_id == AV_CODEC_ID_H264) {
		av_opt_set(_codecCtx, "preset", "ultrafast", 0);
	}
	if (_formatCtx->oformat->flags & AVFMT_GLOBALHEADER) {
		_codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}
	avcodec_parameters_from_context(_videoStream->codecpar, _codecCtx);

	if ((err = avcodec_open2(_codecCtx, _codec, NULL)) < 0) {
		//Free
		throw std::runtime_error("Failed to open codec" + std::to_string(err));
	}

	if (!(_oformat->flags & AVFMT_NOFILE)) {
		if ((err = avio_open(&_formatCtx->pb, _outputFile.c_str(), AVIO_FLAG_WRITE)) < 0) {
			//Free
			throw std::runtime_error("Failed to open file" + std::to_string(err));
		}
	}

	if ((err = avformat_write_header(_formatCtx, NULL)) < 0) {
		// Free
		throw std::runtime_error("Failed to write header"+ std::to_string(err));
	}

	av_dump_format(_formatCtx, 0, _outputFile.c_str(), 1);
}

void FFMpegVideoEncoder::AddFrame(uint8_t * data)
{
	int err;
	if (!_videoFrame) {

		_videoFrame = av_frame_alloc();
		_videoFrame->format = AV_PIX_FMT_YUV420P;
		_videoFrame->width = _codecCtx->width;
		_videoFrame->height = _codecCtx->height;

		if ((err = av_frame_get_buffer(_videoFrame, 32)) < 0) {
			throw std::runtime_error("Failed to allocate picture" + std::to_string(err));
		}
	}

	if (!_swsCtx) {
		_swsCtx = sws_getContext(_codecCtx->width, _codecCtx->height, AV_PIX_FMT_BGRA, _codecCtx->width, _codecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, 0, 0, 0);  //AV_PIX_FMT_RGB24
	}

	int inLinesize[1] = { 4 * _codecCtx->width };

	// From RGB to YUV
	sws_scale(_swsCtx, (const uint8_t * const *)&data, inLinesize, 0, _codecCtx->height, _videoFrame->data, _videoFrame->linesize);

	_videoFrame->pts = _frameCounter++;

	if ((err = avcodec_send_frame(_codecCtx, _videoFrame)) < 0) {
		throw std::runtime_error("Failed to send frame" + std::to_string(err));
	}

	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;

	if (avcodec_receive_packet(_codecCtx, &pkt) == 0) {
		pkt.flags |= AV_PKT_FLAG_KEY;
		av_interleaved_write_frame(_formatCtx, &pkt);
		av_packet_unref(&pkt);
	}

}

void FFMpegVideoEncoder::Finish()
{
	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;

	for (;;) {
		avcodec_send_frame(_codecCtx, NULL);
		if (avcodec_receive_packet(_codecCtx, &pkt) == 0) {
			av_interleaved_write_frame(_formatCtx, &pkt);
			av_packet_unref(&pkt);
		}
		else {
			break;
		}
	}

	av_write_trailer(_formatCtx);
	if (!(_oformat->flags & AVFMT_NOFILE)) {
		int err = avio_close(_formatCtx->pb);
		if (err < 0) {
			throw std::runtime_error("Failed to close file: " + std::to_string(err));
		}
	}

	Free();

	//Remux();
}

void FFMpegVideoEncoder::Free() {
	if (_videoFrame) {
		av_frame_free(&_videoFrame);
	}
	if (_codecCtx) {
		avcodec_free_context(&_codecCtx);
	}
	if (_formatCtx) {
		avformat_free_context(_formatCtx);
	}
	if (_swsCtx) {
		sws_freeContext(_swsCtx);
	}
}