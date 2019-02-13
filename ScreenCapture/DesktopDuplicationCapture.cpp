#include "stdafx.h"
#include "DesktopDuplicationCapture.h"
#include <iostream>
#include <fstream>
#include "TimeUtility.h"
#include <thread>
#include <vector>

#include <d3d11.h>
#include <dxgi1_2.h>
#include <d2d1.h>
#include <d2d1helper.h>

#include <wrl/client.h>

DesktopDuplicationCapture::DesktopDuplicationCapture(uint8_t framrate, std::string output) : _output(output), _frameRate(framrate), _terminate(false)
{
}


DesktopDuplicationCapture::~DesktopDuplicationCapture()
{
}

void DesktopDuplicationCapture::StopCapture()
{
	_terminate = true;
}


void DesktopDuplicationCapture::StartCapture()
{
	//int nScreenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	//int nScreenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);


	auto screenCaptureTime = (1000 / _frameRate);


	IDXGIOutputDuplication* m_DeskDupl;
	ID3D11Device* device = nullptr;
	ID3D11DeviceContext* deviceContext = nullptr;

	D3D_FEATURE_LEVEL FeatureLevel;
	// Driver types supported
	D3D_DRIVER_TYPE DriverTypes[] =
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE,
	};
	// Feature levels supported
	D3D_FEATURE_LEVEL FeatureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_1
	};

	HRESULT hr = S_OK;
	for (UINT DriverTypeIndex = 0; DriverTypeIndex < ARRAYSIZE(DriverTypes); ++DriverTypeIndex)
	{

		hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, FeatureLevels, ARRAYSIZE(FeatureLevels), D3D11_SDK_VERSION, &device, &FeatureLevel, &deviceContext);
		if (SUCCEEDED(hr))
		{
			// Device creation success, no need to loop anymore
			break;
		}
	}

	if (FAILED(hr))
	{
		throw std::runtime_error("Failed to create device in InitializeDx" + std::to_string(hr));
	}

	// Get DXGI device
	IDXGIDevice* DxgiDevice = nullptr;
	hr = device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&DxgiDevice));
	if (FAILED(hr))
	{
		throw std::runtime_error("Failed to QI for DXGI Device" + std::to_string(hr));
	}

	// Get DXGI adapter
	IDXGIAdapter* DxgiAdapter = nullptr;
	hr = DxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&DxgiAdapter));
	DxgiDevice->Release();
	DxgiDevice = nullptr;
	if (FAILED(hr))
	{
		throw std::runtime_error("Failed to get parent DXGI Adapter" + std::to_string(hr));
	}

	// Get output
	IDXGIOutput* DxgiOutput = nullptr;
	hr = DxgiAdapter->EnumOutputs(0, &DxgiOutput);
	DxgiAdapter->Release();
	DxgiAdapter = nullptr;
	if (FAILED(hr))
	{
		throw std::runtime_error("Failed to get specified output in DUPLICATIONMANAGER" + std::to_string(hr));
	}

	DXGI_OUTPUT_DESC outputDesc;

	DxgiOutput->GetDesc(&outputDesc);

	// QI for Output 1
	IDXGIOutput1* DxgiOutput1 = nullptr;
	hr = DxgiOutput->QueryInterface(__uuidof(DxgiOutput1), reinterpret_cast<void**>(&DxgiOutput1));
	DxgiOutput->Release();
	DxgiOutput = nullptr;
	if (FAILED(hr))
	{
		throw std::runtime_error("Failed to QI for DxgiOutput1 in DUPLICATIONMANAGER" + std::to_string(hr));
	}

	// Create desktop duplication
	hr = DxgiOutput1->DuplicateOutput(device, &m_DeskDupl);
	DxgiOutput1->Release();
	DxgiOutput1 = nullptr;
	if (FAILED(hr))
	{
		if (hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE)
		{
			throw std::runtime_error("There is already the maximum number of applications using the Desktop Duplication API running, please close one of those applications and then try again.");
		}
		throw std::runtime_error("Failed to get duplicate output in DUPLICATIONMANAGER" + std::to_string(hr));
	}

	UINT m_MetaDataSize = 0;
	_Field_size_bytes_(m_MetaDataSize) BYTE* m_MetaDataBuffer = nullptr;
	UINT DirtyCount;
	UINT MoveCount;

	_encoder.Init(outputDesc.DesktopCoordinates.right, outputDesc.DesktopCoordinates.bottom, _frameRate, outputDesc.DesktopCoordinates.right * outputDesc.DesktopCoordinates.bottom * _frameRate / 60, _output);

	while (!_terminate)
	{
		TimeUtility time;
		auto  now = std::chrono::system_clock().now();
		std::cout << time.Now() << " Frame Start " << std::endl;


		IDXGIResource* DesktopResource = nullptr;
		DXGI_OUTDUPL_FRAME_INFO FrameInfo;

		//Get Frame
		HRESULT hr = m_DeskDupl->AcquireNextFrame(500, &FrameInfo, &DesktopResource);
		if (hr == DXGI_ERROR_WAIT_TIMEOUT)
		{
			std::cout << "Timeout at AcquireNextFrame" << std::endl;
			continue;
		}
		else if (FAILED(hr)) {
			std::cout << "AcquireNextFrame error " << std::to_string(hr) << std::endl;
			continue;
		}

		ID3D11Texture2D* m_AcquiredDesktopImage;
		// QI for IDXGIResource
		hr = DesktopResource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&m_AcquiredDesktopImage));
		if (FAILED(hr))
		{
			std::cout << "Failed to QI for ID3D11Texture2D from acquired IDXGIResource " << std::to_string( hr) << std::endl;
			continue;
		}

		DesktopResource->Release();

		// Get metadata
		//if (FrameInfo.TotalMetadataBufferSize)
		//{
		//	// Old buffer too small
		//	if (FrameInfo.TotalMetadataBufferSize > m_MetaDataSize)
		//	{
		//		if (m_MetaDataBuffer)
		//		{
		//			delete[] m_MetaDataBuffer;
		//			m_MetaDataBuffer = nullptr;
		//		}
		//		m_MetaDataBuffer = new (std::nothrow) BYTE[FrameInfo.TotalMetadataBufferSize];
		//		if (!m_MetaDataBuffer)
		//		{
		//			m_MetaDataSize = 0;
		//			MoveCount = 0;
		//			DirtyCount = 0;
		//			std::cout << "Failed to allocate memory for metadata in DUPLICATIONMANAGER E_OUTOFMEMORY" << std::endl;
		//			continue;
		//		}
		//		m_MetaDataSize = FrameInfo.TotalMetadataBufferSize;
		//	}

		//	UINT BufSize = FrameInfo.TotalMetadataBufferSize;

		//	// Get move rectangles
		//	hr = m_DeskDupl->GetFrameMoveRects(BufSize, reinterpret_cast<DXGI_OUTDUPL_MOVE_RECT*>(m_MetaDataBuffer), &BufSize);
		//	if (FAILED(hr))
		//	{
		//		MoveCount = 0;
		//		DirtyCount = 0;
		//		std::cout << "Failed to get frame move rects in DUPLICATIONMANAGER" + std::to_string(hr) << std::endl;
		//		continue;
		//	}
		//	MoveCount = BufSize / sizeof(DXGI_OUTDUPL_MOVE_RECT);

		//	BYTE* DirtyRects = m_MetaDataBuffer + BufSize;
		//	BufSize = FrameInfo.TotalMetadataBufferSize - BufSize;

		//	// Get dirty rectangles
		//	hr = m_DeskDupl->GetFrameDirtyRects(BufSize, reinterpret_cast<RECT*>(DirtyRects), &BufSize);
		//	if (FAILED(hr))
		//	{
		//		MoveCount = 0;
		//		DirtyCount = 0;
		//		std::cout << "Failed to get frame dirty rects in DUPLICATIONMANAGER" + std::to_string(hr);
		//		continue;
		//	}
		//	DirtyCount = BufSize / sizeof(RECT);

		//}


		// Get mouse info
		//Ret = DuplMgr.GetMouse(TData->PtrInfo, &(CurrentData.FrameInfo), TData->OffsetX, TData->OffsetY);


		//ProcessFrame

		// Process dirties and moves
		//if (FrameInfo.TotalMetadataBufferSize)
		//{
		//	D3D11_TEXTURE2D_DESC Desc;
		//	m_AcquiredDesktopImage->GetDesc(&Desc);

		//	if (MoveCount)
		//	{
		//		auto Ret = CopyMove(SharedSurf, reinterpret_cast<DXGI_OUTDUPL_MOVE_RECT*>(MetaData), MoveCount, OffsetX, OffsetY, DeskDesc, Desc.Width, Desc.Height);
		//		if (Ret != DUPL_RETURN_SUCCESS)
		//		{
		//			return Ret;
		//		}
		//	}

		//	if (DirtyCount)
		//	{
		//		Ret = CopyDirty(Data->Frame, SharedSurf, reinterpret_cast<RECT*>(Data->MetaData + (Data->MoveCount * sizeof(DXGI_OUTDUPL_MOVE_RECT))), Data->DirtyCount, OffsetX, OffsetY, DeskDesc);
		//	}
		//}

		//ID3D11ShaderResourceView* ShaderResource = nullptr;
		//hr = device->CreateShaderResourceView(SrcSurface, &ShaderDesc, &ShaderResource);
		//if (FAILED(hr))
		//{
		//	return ProcessFailure(m_Device, L"Failed to create shader resource view for dirty rects", L"Error", hr, SystemTransitionsExpectedErrors);
		//}

		D3D11_TEXTURE2D_DESC m_AcquiredDesktopImageDesc;
		m_AcquiredDesktopImage->GetDesc(&m_AcquiredDesktopImageDesc);


		DXGI_MAPPED_RECT lockedRect;
		hr = m_DeskDupl->MapDesktopSurface(&lockedRect);


		std::vector<uint8_t> transcodedData;

		if (DXGI_ERROR_UNSUPPORTED == hr) {
			
			
			ID3D11Texture2D *texture = nullptr;
			D3D11_MAPPED_SUBRESOURCE resource;

			D3D11_TEXTURE2D_DESC textdesc;
			textdesc.Width = m_AcquiredDesktopImageDesc.Width;
			textdesc.Height = m_AcquiredDesktopImageDesc.Height;
			textdesc.MipLevels = 1;
			textdesc.ArraySize = 1;
			textdesc.Format = m_AcquiredDesktopImageDesc.Format;// DXGI_FORMAT_B8G8R8A8_UNORM; 
			textdesc.SampleDesc.Count = 1; /* MultiSampling, we can use 1 as we're just downloading an existing one. */
			textdesc.SampleDesc.Quality = 0; /* "" */
			textdesc.Usage = D3D11_USAGE_STAGING;
			textdesc.BindFlags = 0;
			textdesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			textdesc.MiscFlags = 0;

			hr = device->CreateTexture2D(&textdesc, nullptr, &texture);

			deviceContext->CopyResource(texture, m_AcquiredDesktopImage);

			//UINT subresource = D3D11CalcSubresource(0, 0 ,0);
			hr = deviceContext->Map(texture, 0, D3D11_MAP_READ, 0, &resource);

			std::cout << time.Since(now) << " Transcode start " << std::endl;
			transcodedData = GetData(reinterpret_cast<uint8_t*>(resource.pData), m_AcquiredDesktopImageDesc.Height, m_AcquiredDesktopImageDesc.Width);
			deviceContext->Unmap(texture, 0);
		}
		else {
			std::cout << time.Since(now) << " Transcode start " << std::endl;
			transcodedData = GetData(lockedRect.pBits, m_AcquiredDesktopImageDesc.Height, m_AcquiredDesktopImageDesc.Width);
			m_DeskDupl->UnMapDesktopSurface();
		}

		std::cout << time.Since(now) << "Transcode End. Encode Start " << std::endl;
		_encoder.AddFrame(transcodedData.data());


		//free(pBits);

		std::cout << time.Since(now) << " Encode Done " << std::endl;

		std::cout << time.Since(now) << " Frame Finished " << std::endl;

		auto now2 = std::chrono::system_clock().now();
		auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(now2 - now).count();
		if (elapsedTime < screenCaptureTime) {
			std::this_thread::sleep_for(std::chrono::milliseconds(screenCaptureTime - elapsedTime));
			std::cout << "Waited:" << std::to_string(screenCaptureTime - elapsedTime) << std::endl;
		}

		m_AcquiredDesktopImage->Release();
		m_AcquiredDesktopImage = nullptr;
	}

	m_DeskDupl->Release();
	_encoder.Finish();

}

std::vector<uint8_t> DesktopDuplicationCapture::GetData(uint8_t* data, uint16_t height, uint16_t width)
{

	std::vector<uint8_t> result;

	auto begin = data;

	for (int y = 0; y< height; y++)
	{
		for (auto x = 0; x < width; x++) {
			auto b = begin++;
			auto g = begin++;
			auto r = begin++;
			auto reserved = begin++;

			result.push_back(*r);
			result.push_back(*g);
			result.push_back(*b);

		}
	}

	std::ofstream dump;
	dump.open("d:\\Idei\\POC\\ScreenCapture\\output\\result_DD.h264.dump", std::ofstream::binary);
	if (dump.fail()) {
		std::cout << strerror(errno) << std::endl;
		return result;
	}

	dump.write(reinterpret_cast<char*>(data), width * height * 3);
	dump.close();

	return result;
}