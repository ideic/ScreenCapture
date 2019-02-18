// ScreenCapture.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <thread>
#include "Screen2File.h"
#include "GDICapture.h"
#include "DirectXCapture.h"
#include "DesktopDuplicationCapture.h"

void Intel(int frameRate)
{
	std::string output = "d:\\Idei\\POC\\ScreenCapture\\output\\result_Intel.h264";

	Screen2File capture(frameRate, output);

	auto t1 = std::thread([&capture]() {
		capture.StartCapture();
	});

	std::cin.ignore();
	capture.StopCapture();
	t1.join();
}

void GDI(int frameRate)
{
	std::string output = "result_GDI.h264";

	GDICapture capture(frameRate, output);

	auto t1 = std::thread([&capture]() {
		capture.StartCapture();
	});

	std::cout << "Press any key to finish" << std::endl;
	std::cin.ignore();
	capture.StopCapture();
	t1.join();
}

void DesktopDuplication(int frameRate) {
	std::string output = "d:\\Idei\\POC\\ScreenCapture\\output\\result_DD.h264";

	DesktopDuplicationCapture capture(frameRate, output);

	auto t1 = std::thread([&capture]() {
		capture.StartCapture();
	});

	std::cout << "Press any key to finish" << std::endl;
	std::cin.ignore();
	capture.StopCapture();
	t1.join();
}

void DirectX(int frameRate)
{
	std::string output = "d:\\Idei\\POC\\ScreenCapture\\output\\result_Directx.h264";

	DirectXCapture capture(frameRate, output);

	auto t1 = std::thread([&capture]() {
		capture.StartCapture();
	});

	std::cout << "Press any key to finish" << std::endl;
	std::cin.ignore();
	capture.StopCapture();
	t1.join();
}

int main()
{
	int framrate = 2;

	//Intel(framrate);
	GDI(framrate);

	//DirectX(framrate);
	//DesktopDuplication(framrate);

	std::cout << "Finished" << std::endl;
	std::cin.ignore();
	return 0;
}
