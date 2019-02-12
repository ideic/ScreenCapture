#include "stdafx.h"
#include "DirectXCapture.h"
#include <Windows.h>
#include <iostream>
#include <fstream>
#include "TimeUtility.h"
#include <d3d9.h> // d11 ?
#include <thread>
#include <d3d9caps.h>
#include <d3d9helper.h>
#include <vector>

DirectXCapture::DirectXCapture(uint8_t framrate, std::string output) : _output(output), _frameRate(framrate), _terminate(false)
{
}


DirectXCapture::~DirectXCapture()
{
}
#define BITSPERPIXEL		32
void DirectXCapture::StartCapture()
{
	//int nScreenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	//int nScreenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);


	auto screenCaptureTime = (1000 / _frameRate);
	

	IDirect3D9*	pD3D = NULL;
	D3DDISPLAYMODE mode;
	D3DPRESENT_PARAMETERS parameters = { 0 };
	IDirect3DDevice9* pDevice = nullptr;

	if ((pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == NULL)
	{
		throw std::runtime_error("Unable to Create Direct3D ");
	}

	//for (int m = 0; m < pD3D-> GetAdapterModeCount(D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8); m++) {
	//	pD3D->EnumAdapterModes(D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8, m, &mode);
	//}



	// init D3D and get screen size
	auto res = pD3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &mode);
	if (res != D3D_OK) {
		throw std::runtime_error("GetAdapterDisplayMode:" + std::to_string(res));
	}

	//mode.Height = nScreenHeight;
	//mode.Width = nScreenWidth;

	parameters.Windowed = TRUE;
	parameters.BackBufferCount = 1;
	parameters.BackBufferHeight = mode.Height;
	parameters.BackBufferWidth = mode.Width;
	parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
	parameters.hDeviceWindow = NULL;


	res = pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, NULL, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &parameters, &pDevice);

	if (D3D_OK != res)
	{
		throw std::runtime_error("Unable to Create Device Code:" + std::to_string(res));
	}

	_encoder.Init(mode.Width, mode.Height, _frameRate, mode.Width * mode.Height * _frameRate / 60, _output);

	while (!_terminate)
	{
		TimeUtility time;
		auto  now = std::chrono::system_clock().now();
		std::cout << time.Now() << " Frame Start " << std::endl;

		IDirect3DSurface9* pSurface;

		res = pDevice->CreateOffscreenPlainSurface(mode.Width, mode.Height, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH,	&pSurface, NULL); //D3DFMT_A8R8G8B8
		if (res != D3D_OK) {
			throw std::runtime_error("CreateOffscreenPlainSurface:" + std::to_string(res));
		}

		res = pDevice->GetFrontBufferData(0, pSurface);
		if (res != D3D_OK) {
			throw std::runtime_error("GetFrontBufferData:" + std::to_string(res));
		}


		LPVOID	pBits = malloc(mode.Width * BITSPERPIXEL / 8 * mode.Height);

		std::cout << time.Since(now) << " Lock Start" << std::endl;

		D3DLOCKED_RECT lockedRect;
		pSurface->LockRect(&lockedRect, NULL,D3DLOCK_NO_DIRTY_UPDATE |D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY);

		std::vector<uint8_t> result;

		auto begin = reinterpret_cast<uint8_t*>(lockedRect.pBits);

		for (int y =0; y< mode.Height; y++)
		{
			for (auto x = 0; x < mode.Width; x ++) {
				auto b = begin++;
				auto g = begin++;     
				auto r = begin++;	
				auto reserved =begin++; 

				result.push_back(*r);
				result.push_back(*g);
				result.push_back(*b);

			}
		}


		pSurface->UnlockRect();
		pSurface->Release();


		std::cout << time.Since(now) << " Lock Finished. Encode start" << std::endl;

		//std::ofstream dump;
		//dump.open("d:\\Idei\\POC\\ScreenCapture\\output\\result_Directx.h264.dump", std::ofstream::binary);
		//if (dump.fail()) {
		//	std::cout << strerror(errno) << std::endl;
		//	return;
		//}

		//dump.write(reinterpret_cast<char*>(pBits), mode.Width * BITSPERPIXEL / 8 * mode.Height);
		//dump.close();


		_encoder.AddFrame(result.data());

		free(pBits);

		std::cout << time.Since(now) << " Encode Done " << std::endl;

		std::cout << time.Since(now) << " Frame Finished " << std::endl;

		auto now2 = std::chrono::system_clock().now();
		auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(now2 - now).count();
		if (elapsedTime < screenCaptureTime) {
			std::this_thread::sleep_for(std::chrono::milliseconds(screenCaptureTime - elapsedTime));
			std::cout << "Waited:" << std::to_string(screenCaptureTime - elapsedTime) << std::endl;
		}
	}

	_encoder.Finish();

}

void DirectXCapture::StopCapture()
{
	_terminate = true;
}
