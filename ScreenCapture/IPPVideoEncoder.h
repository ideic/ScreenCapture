#pragma once
#include <string>
#include <fstream>

#include "mfxvideo.h"   /* The SDK include file */ 
#include "mfxvideo++.h" /* Optional for C++ development */ 
#include "mfxplugin.h" /* Plugin development */ 
class IPPVideoEncoder
{
private:
	mfxVideoParam _videoParam;
	mfxSession _session;
	mfxSyncPoint _syncpH264;
	mfxBitstream _stream;
	std::ofstream outputFile;
	mfxFrameAllocator _allocator;
	mfxFrameSurface1** _pmfxSurfaces;
	mfxU16 _numSurfaces;
	mfxU64 _bufferSize;

	void ResetStream(mfxU16 numberofSuggestedFrame, mfxU64 bufferSize);
	void ResetStream();

	void ConfigureH264Params(int width, int height, int fpsrate, int bitrate);

	void CreateSurfaces();
public:
	IPPVideoEncoder();
	~IPPVideoEncoder();

	void Init(int width, int height, int fpsrate, int bitrate, std::string outputFileName);

	void AddFrame(uint8_t *data);

	void Finish();
};

