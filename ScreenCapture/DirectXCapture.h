#pragma once
#include "FFMpegVideoEncoder.h"
class DirectXCapture
{
private:
	std::string _output;
	uint8_t _frameRate;
	bool _terminate;
	FFMpegVideoEncoder _encoder;
public:
	DirectXCapture(uint8_t framrate, std::string output);
	~DirectXCapture();

	void StartCapture();
	void StopCapture();

};

