#pragma once
#include <string>

#include "mfxvideo.h"   /* The SDK include file */ 
#include "mfxvideo++.h" /* Optional for C++ development */ 
#include "mfxplugin.h" /* Plugin development */ 
class Screen2File
{
private:
	uint8_t _framrate;
	std::string _output;
	mfxSession _session;
	bool _terminate;

	void ConfigureScreenCaptureParams(mfxVideoParam & conf);
	void ConfigureH264Params(mfxVideoParam & conf);
	mfxFrameSurface1** CreateSurfaces(mfxU16 suggestedFrame, mfxU16 width, mfxU16 height, mfxFrameInfo  &frameInfo);
	void ResetStream(mfxBitstream &stream, mfxU16 numberofSuggestedFrame, mfxU64 bufferSize);

	void ProcessFrame(mfxFrameSurface1* frame);

public:
	Screen2File(uint8_t framrate, std::string output);
	~Screen2File();

	void StartCapture();
	void StopCapture();
};

