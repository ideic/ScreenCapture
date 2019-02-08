// ScreenCapture.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <thread>
#include "Screen2File.h"

int main()
{
	int framrate = 6;
	std::string output = "d:\\Idei\\POC\\ScreenCapture\\output\\result.h264";
	
	Screen2File capture(framrate, output);

	auto t1 = std::thread([&capture]() {
		capture.StartCapture(); 
	});

	std::cin.ignore();
	capture.StopCapture();
	t1.join();

	return 0;
}
