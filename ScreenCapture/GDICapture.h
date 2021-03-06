#pragma once
#include <string>
#include <Windows.h>
#include <vector>
//#include "FFMpegVideoEncoder.h"
#include "IPPVideoEncoder.h"
class GDICapture
{
private:
	std::string _output;
	uint8_t _frameRate;
	bool _terminate;

	void SaveBitmap(HBITMAP hBitmap);
	//FFMpegVideoEncoder _encoder;
	IPPVideoEncoder _encoder;
	uint8_t* screenShotData;
	void SetScreenShotData(HBITMAP hBitmap);
public:
	GDICapture(uint8_t framrate, std::string output);
	~GDICapture();
	void StartCapture();
	void StopCapture();


};

