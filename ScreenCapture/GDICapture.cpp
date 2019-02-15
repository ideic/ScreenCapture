#include "stdafx.h"
#include "GDICapture.h"
#include <fstream>
#include <iostream>
#include <thread>
#include "TimeUtility.h"
#include <ippcc.h>
#include <ippcore.h>
#include <ippvm.h>
#include <ipps.h>
#include <ippi.h>

#include <array>

GDICapture::GDICapture(uint8_t framrate, std::string output) : _output(output), _frameRate(framrate), _terminate(false)
{
}

GDICapture::~GDICapture()
{
}

void GDICapture::StartCapture()
{
	int nScreenWidth = GetSystemMetrics(SM_CXSCREEN); //SM_CXVIRTUALSCREEN
	int nScreenHeight = GetSystemMetrics(SM_CYSCREEN); // SM_CYVIRTUALSCREEN

	_encoder.Init(nScreenWidth, nScreenHeight, _frameRate, nScreenWidth * nScreenHeight * _frameRate / 60, _output);

	auto screenCaptureTime = (1000 / _frameRate);
	while (!_terminate)
	{
		TimeUtility time;
		auto  now = std::chrono::system_clock().now();
		std::cout << time.Now() << " Frame Start " << std::endl;

		HWND hDesktopWnd = GetDesktopWindow();
		HDC hDesktopDC = GetDC(hDesktopWnd);
		HDC hCaptureDC = CreateCompatibleDC(hDesktopDC);
		HBITMAP hCaptureBitmap = CreateCompatibleBitmap(hDesktopDC, nScreenWidth, nScreenHeight);
		SelectObject(hCaptureDC, hCaptureBitmap);
		BitBlt(hCaptureDC, 0, 0, nScreenWidth, nScreenHeight, hDesktopDC, 0, 0, SRCCOPY | CAPTUREBLT);

		std::cout << time.Since(now) << " Screen shot done. Start Encode " << std::endl;
		//SaveBitmap(hCaptureBitmap);

		CURSORINFO cursor = { sizeof(cursor) };
		::GetCursorInfo(&cursor);
		if (cursor.flags == CURSOR_SHOWING) {
			RECT rcWnd;
			::GetWindowRect(hDesktopWnd, &rcWnd);
			ICONINFOEXW info = { sizeof(info) };
			::GetIconInfoExW(cursor.hCursor, &info);
			const int x = cursor.ptScreenPos.x - rcWnd.left  - info.xHotspot;
			const int y = cursor.ptScreenPos.y - rcWnd.top - info.yHotspot;
			BITMAP bmpCursor = { 0 };
			::GetObject(info.hbmColor, sizeof(bmpCursor), &bmpCursor);
			::DrawIconEx(hCaptureDC, x, y, cursor.hCursor, bmpCursor.bmWidth, bmpCursor.bmHeight, 0, NULL, DI_NORMAL);
		}
		//
		SetScreenShotData(hCaptureBitmap);
		std::cout << time.Since(now) << "Screen shot transcoded" << std::endl;

		_encoder.AddFrame(screenShotData.data());

		std::cout << time.Since(now) << " Encode Done " << std::endl;

		ReleaseDC(hDesktopWnd, hDesktopDC);
		DeleteDC(hCaptureDC);
		DeleteObject(hCaptureBitmap);

		std::cout << time.Since(now) << " Frame Finished " << std::endl;

		auto now2 = std::chrono::system_clock().now();
		auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(now2 - now).count();
		if (elapsedTime < screenCaptureTime) {
			std::this_thread::sleep_for(std::chrono::milliseconds(screenCaptureTime - elapsedTime));
			std::cout << "Waited:" << std::to_string(screenCaptureTime - elapsedTime)<<std::endl;
		}
	}

	_encoder.Finish();

}

void GDICapture::StopCapture()
{
	_terminate = true;
}


void GDICapture::SaveBitmap(HBITMAP hBitmap)
{
	HDC					hdc = NULL;
	LPVOID				pBuf = NULL;
	BITMAPINFO			bmpInfo;
	BITMAPFILEHEADER	bmpFileHeader;

	do {

		hdc = GetDC(NULL);
		ZeroMemory(&bmpInfo, sizeof(BITMAPINFO));
		bmpInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		GetDIBits(hdc, hBitmap, 0, 0, NULL, &bmpInfo, DIB_RGB_COLORS); //DIB_RGB_COLORS

		if (bmpInfo.bmiHeader.biSizeImage <= 0)
			bmpInfo.bmiHeader.biSizeImage = bmpInfo.bmiHeader.biWidth*abs(bmpInfo.bmiHeader.biHeight)*(bmpInfo.bmiHeader.biBitCount + 7) / 8;

		if ((pBuf = malloc(bmpInfo.bmiHeader.biSizeImage)) == NULL)
		{
			MessageBox(NULL, _T("Unable to Allocate Bitmap Memory"), _T("Error"), MB_OK | MB_ICONERROR);
			break;
		}

		bmpInfo.bmiHeader.biCompression = BI_RGB;
		GetDIBits(hdc, hBitmap, 0, bmpInfo.bmiHeader.biHeight, pBuf, &bmpInfo, DIB_RGB_COLORS); // DIB_RGB_COLORS

		std::ofstream outputFile;
		outputFile.open(_output + ".bmp", std::ofstream::binary);
		if (outputFile.fail()) {
			std::cout << strerror(errno) << std::endl;
			return;
		}

		bmpFileHeader.bfReserved1 = 0;
		bmpFileHeader.bfReserved2 = 0;
		bmpFileHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + bmpInfo.bmiHeader.biSizeImage;
		bmpFileHeader.bfType = 'MB';
		bmpFileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

		outputFile.write(reinterpret_cast<char*>(&bmpFileHeader), sizeof(BITMAPFILEHEADER));
		outputFile.write(reinterpret_cast<char*>(&bmpInfo.bmiHeader), sizeof(BITMAPINFOHEADER));
		outputFile.write(reinterpret_cast<char*>(pBuf), bmpInfo.bmiHeader.biSizeImage);


		outputFile.close();

	} while (false);

	if (hdc)
		ReleaseDC(NULL, hdc);

	if (pBuf)
		free(pBuf);

}

void GDICapture::SetScreenShotData(HBITMAP hBitmap)
{
	HDC					hdc = NULL;
	LPVOID				pBuf = NULL;
	BITMAPINFO			bmpInfo;

	screenShotData.clear();

	hdc = GetDC(NULL);
	ZeroMemory(&bmpInfo, sizeof(BITMAPINFO));
	bmpInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	GetDIBits(hdc, hBitmap, 0, 0, NULL, &bmpInfo, DIB_RGB_COLORS);

	if (bmpInfo.bmiHeader.biSizeImage <= 0)
		bmpInfo.bmiHeader.biSizeImage = bmpInfo.bmiHeader.biWidth*abs(bmpInfo.bmiHeader.biHeight)*(bmpInfo.bmiHeader.biBitCount + 7) / 8;

	if ((pBuf = malloc(bmpInfo.bmiHeader.biSizeImage)) == NULL)
	{
		throw std::runtime_error("Unable to Allocate Bitmap Memory");
	}

	bmpInfo.bmiHeader.biCompression = BI_RGB;
	GetDIBits(hdc, hBitmap, 0, bmpInfo.bmiHeader.biHeight, pBuf, &bmpInfo, DIB_RGB_COLORS);

	Ipp8u* dstOrig = new Ipp8u[bmpInfo.bmiHeader.biWidth * bmpInfo.bmiHeader.biHeight * 4];

	//This works great
	auto status = ippiMirror_8u_AC4R(reinterpret_cast<Ipp8u*>(pBuf), bmpInfo.bmiHeader.biWidth * 4, dstOrig, bmpInfo.bmiHeader.biWidth * 4, { bmpInfo.bmiHeader.biWidth, bmpInfo.bmiHeader.biHeight }, ippAxsHorizontal);
	
	//uint8_t channels = 3;
	//uint32_t pixelSize = bmpInfo.bmiHeader.biWidth * bmpInfo.bmiHeader.biHeight;

	//Ipp8u**  dst = new Ipp8u*[channels];
	//dst[0] = new Ipp8u[pixelSize  + pixelSize ];
	//dst[1] = dst[0] + pixelSize;
	//dst[2] = dst[0] + pixelSize / 2;
	////for (int i = 1; i < channels; i++) {
	////	dst[i] = dst[i - 1] + pixelSize;
	////}


	auto begin = reinterpret_cast<uint8_t*>(dstOrig);
	auto end = reinterpret_cast<uint8_t*>(dstOrig + bmpInfo.bmiHeader.biWidth * bmpInfo.bmiHeader.biHeight * 4);
	screenShotData.insert(screenShotData.begin(), begin, end);


	//int dstStep[3]{ bmpInfo.bmiHeader.biWidth,bmpInfo.bmiHeader.biWidth,bmpInfo.bmiHeader.biWidth };
	//int srcStep = bmpInfo.bmiHeader.biWidth * 4;
	//IppiSize roiSize = { bmpInfo.bmiHeader.biWidth, bmpInfo.bmiHeader.biHeight };

	//status = ippiBGRToYCbCr420_8u_AC4P3R(dstOrig, srcStep, dst, dstStep, roiSize);


	//auto begin = reinterpret_cast<uint8_t*>(dst[0]);
	//auto end = reinterpret_cast<uint8_t*>(dst[0] + pixelSize * 2);
	//screenShotData.insert(screenShotData.begin(), begin, end);


	//auto begin = reinterpret_cast<uint8_t*>(dstOrig);

	//for (int y = 0; y< bmpInfo.bmiHeader.biHeight; y++)
	//{
	//	for (auto x = 0; x <bmpInfo.bmiHeader.biWidth; x++) {
	//		auto b = begin++;
	//		auto g = begin++;
	//		auto r = begin++;
	//		auto reserved = begin++;

	//		screenShotData.push_back(*r);
	//		screenShotData.push_back(*g);
	//		screenShotData.push_back(*b);

	//	}
	//}



	delete[] dstOrig;
	//delete[] dst[0];
	//delete[] dst;


	//std::ofstream dump;
	//dump.open("d:\\Idei\\POC\\ScreenCapture\\output\\result_GDI.h264.dump", std::ofstream::binary);
	//if (dump.fail()) {
	//	std::cout << strerror(errno) << std::endl;
	//	return;
	//}

	//dump.write(reinterpret_cast<char*>(screenShotData.data()), screenShotData.size());
	//dump.close();

	if (hdc)
		ReleaseDC(NULL, hdc);

	if (pBuf)
		free(pBuf);
}