#include "stdafx.h"
#include "GDICapture.h"
#include <fstream>
#include <iostream>
#include <thread>
#include "TimeUtility.h"

GDICapture::GDICapture(uint8_t framrate, std::string output) : _output(output), _frameRate(framrate), _terminate(false)
{
}

GDICapture::~GDICapture()
{
}

void GDICapture::StartCapture()
{
	int nScreenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	int nScreenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

	_encoder.Init(nScreenWidth, nScreenHeight, _frameRate, nScreenWidth * nScreenHeight * _frameRate / 60, _output);

	while (!_terminate)
	{
		TimeUtility time;
		std::cout << "Frame Start: " << time.Now() << std::endl;

		HWND hDesktopWnd = GetDesktopWindow();
		HDC hDesktopDC = GetDC(hDesktopWnd);
		HDC hCaptureDC = CreateCompatibleDC(hDesktopDC);
		HBITMAP hCaptureBitmap = CreateCompatibleBitmap(hDesktopDC, nScreenWidth, nScreenHeight);
		SelectObject(hCaptureDC, hCaptureBitmap);
		BitBlt(hCaptureDC, 0, 0, nScreenWidth, nScreenHeight, hDesktopDC, 0, 0, SRCCOPY | CAPTUREBLT);

		//SaveBitmap(hCaptureBitmap);

		_encoder.AddFrame(GetBitMap(hCaptureBitmap).data());

		ReleaseDC(hDesktopWnd, hDesktopDC);
		DeleteDC(hCaptureDC);
		DeleteObject(hCaptureBitmap);

		std::cout << "Frame Finished: " << time.Now() << std::endl;

		std::this_thread::sleep_for(std::chrono::milliseconds(1000 / _frameRate));
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

std::vector<uint8_t> GDICapture::GetBitMap(HBITMAP hBitmap)
{
	HDC					hdc = NULL;
	LPVOID				pBuf = NULL;
	BITMAPINFO			bmpInfo;
	BITMAPFILEHEADER	bmpFileHeader;

	std::vector<uint8_t> result;

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

	bmpFileHeader.bfReserved1 = 0;
	bmpFileHeader.bfReserved2 = 0;
	bmpFileHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + bmpInfo.bmiHeader.biSizeImage;
	bmpFileHeader.bfType = 'MB';
	bmpFileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

	for (int y = bmpInfo.bmiHeader.biHeight; y > 0;  y--)
	{
		auto begin = reinterpret_cast<uint8_t*>(pBuf) + bmpInfo.bmiHeader.biWidth * 4 * (y - 1) ;

		for (auto x = 0; x < bmpInfo.bmiHeader.biWidth; x++) {
			auto b = *(begin + 4 * x ); // g         -- reserved
			auto g = *(begin + (4 * x) + 1); // r      -- b 
			auto r = *(begin + (4 * x) + 2);//		 -- g
			auto reserved = *(begin + (4 * x )+ 3); //b --
			
			result.push_back(r); 
			result.push_back(g); 
			result.push_back(b); 

		}
	}

	//std::ofstream dump;
	//dump.open("d:\\Idei\\POC\\ScreenCapture\\output\\result_GDI.h264.dump", std::ofstream::binary);
	//if (dump.fail()) {
	//	std::cout << strerror(errno) << std::endl;
	//	return result;
	//}

	//dump.write(reinterpret_cast<char*>(result.data()), result.size());
	//dump.close();

	if (hdc)
		ReleaseDC(NULL, hdc);

	if (pBuf)
		free(pBuf);

	return result;
}