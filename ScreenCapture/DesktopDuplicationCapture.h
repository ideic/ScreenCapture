#pragma once
#include <string>
#include "FFMpegVideoEncoder.h"
#include <vector>
class DesktopDuplicationCapture
{
private:
	std::string _output;
	uint8_t _frameRate;
	bool _terminate;
	FFMpegVideoEncoder _encoder;
	std::vector<uint8_t> GetData(uint8_t* data, uint16_t height, uint16_t width);

public:
	DesktopDuplicationCapture(uint8_t framrate, std::string output);
	~DesktopDuplicationCapture();

	void StartCapture();
	void StopCapture();

};

