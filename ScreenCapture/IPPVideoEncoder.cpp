#include "stdafx.h"
#include "IPPVideoEncoder.h"
#include <iostream>
#include <ippi.h>
#include <ippcc.h>
#include <algorithm>

IPPVideoEncoder::IPPVideoEncoder()
{
	mfxVersion version{ 23,1 };
	auto res = MFXInit(MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D11, &version, &_session);

	if (res != MFX_ERR_NONE) {
		throw std::runtime_error("Cannot Initialize  Intel MFX Error Code" + std::to_string(res));
	}

	res = MFXVideoUSER_Load(_session, &MFX_PLUGINID_H264LA_HW, version.Major); //&MFX_PLUGINTYPE_VIDEO_ENCODE
	if (res != MFX_ERR_NONE) {
		throw std::runtime_error("Cannot Load Plugin MFX_PLUGINID_H264LA_HW Error: " + std::to_string(res));
	}


}


IPPVideoEncoder::~IPPVideoEncoder()
{
	MFXVideoDECODE_Close(_session);
	MFXVideoENCODE_Close(_session);
	MFXClose(_session);
}

void IPPVideoEncoder::ConfigureH264Params(int width, int height, int fpsrate, int bitrate) {

	memset(&_videoParam, 0, sizeof(_videoParam));

	_videoParam.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
	_videoParam.mfx.CodecId = MFX_CODEC_AVC;
	_videoParam.mfx.TargetUsage = MFX_TARGETUSAGE_BEST_QUALITY;
	_videoParam.mfx.TargetKbps = 512;

	// For encoder, Width must be a multiple of 16
	// Height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
	_videoParam.mfx.FrameInfo.Width = width;
	_videoParam.mfx.FrameInfo.Height = height+8;
	_videoParam.mfx.FrameInfo.CropW = width;
	_videoParam.mfx.FrameInfo.CropH = height;
	_videoParam.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
	_videoParam.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
	_videoParam.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
	_videoParam.mfx.FrameInfo.FrameRateExtN = fpsrate; // FrameRate is N/D, 4 is the minimal for ffplay
	_videoParam.mfx.FrameInfo.FrameRateExtD = 1;

}

void IPPVideoEncoder::CreateSurfaces()
{
	/* check supported parameters */
	mfxFrameAllocRequest requestH264;

	auto res = MFXVideoENCODE_QueryIOSurf(_session, &_videoParam, &requestH264);

	if (res != MFX_ERR_NONE) {
		throw std::runtime_error("MFXVideoDECODE_QueryIOSurf h264VideoParam failed with code" + std::to_string(res));
	}

	//Allocate Surfaces
	_numSurfaces = requestH264.NumFrameSuggested;
	_bufferSize = requestH264.Info.BufferSize;

	//mfxFrameAllocResponse mfxResponse;
	//res = _allocator.Alloc(_allocator.pthis, &requestH264, &mfxResponse);

	if (res != MFX_ERR_NONE) {
		throw std::runtime_error("MFXFrameAllocator failed with code" + std::to_string(res));
	}

	mfxU16 width = _videoParam.mfx.FrameInfo.Width;
	mfxU16 height = _videoParam.mfx.FrameInfo.CropH;


	mfxU32 surfaceSize = width * height + width * height /2;
	mfxU8* surfaceBuffers = (mfxU8*) new mfxU8[surfaceSize * _numSurfaces];


	// Allocate surface headers (mfxFrameSurface1) for decoder
	_pmfxSurfaces = new mfxFrameSurface1 *[_numSurfaces];
	for (int i = 0; i < _numSurfaces; i++) {
		_pmfxSurfaces[i] = new mfxFrameSurface1;
		
		memset(_pmfxSurfaces[i], 0, sizeof(mfxFrameSurface1));
		memcpy(&(_pmfxSurfaces[i]->Info), &(_videoParam.mfx.FrameInfo),	sizeof(mfxFrameInfo));

		_pmfxSurfaces[i]->Data.Y = &surfaceBuffers[surfaceSize * i];
		_pmfxSurfaces[i]->Data.CbCr = _pmfxSurfaces[i]->Data.Y + width * height;
		//_pmfxSurfaces[i]->Data.V = _pmfxSurfaces[i]->Data.U + 1;
		_pmfxSurfaces[i]->Data.Pitch = width;

		//_pmfxSurfaces[i]->Data.MemId = mfxResponse.mids[i];
	}
}

void IPPVideoEncoder::Init(int width, int height, int fpsrate, int bitrate, std::string outputFileName)
{
	ConfigureH264Params(width, height, fpsrate, bitrate);

	//Validates input parameter
	auto res = MFXVideoENCODE_Query(_session, &_videoParam, &_videoParam);

	if (res != MFX_ERR_NONE && res != MFX_WRN_PARTIAL_ACCELERATION) {
		std::cout << " MFXVideoDECODE_Query h264VideoParam failed with code" << std::to_string(res) << std::endl;
		return;
	}


	CreateSurfaces();

	res = MFXVideoENCODE_Init(_session, &_videoParam);

	if (res != MFX_ERR_NONE && res != MFX_WRN_PARTIAL_ACCELERATION) {
		
		return;
	}

	ResetStream(_numSurfaces, _bufferSize);
	outputFile.open(outputFileName, std::ofstream::binary);
	if (outputFile.fail()) {
		std::cout << "Failed to open file" << strerror(errno) << std::endl;
		return;
	}

	_initTime = std::chrono::system_clock::now();
}

void IPPVideoEncoder::AddFrame(uint8_t * data)
{
	mfxU16 idx = 0;
	for (idx; idx < _numSurfaces; idx++)
	{
		if (!_pmfxSurfaces[idx]->Data.Locked) {
			break;
		}
	}

	if (idx >= _numSurfaces) {
		std::cout << " There is no unlocked surface" << std::endl;
		return;
	}

	mfxFrameSurface1* inSurface = _pmfxSurfaces[idx];

	mfxU16 width = _videoParam.mfx.FrameInfo.Width;
	mfxU16 height = _videoParam.mfx.FrameInfo.CropH;
	uint32_t screenSize = width * height; 

	int dstStep[3]{ width, width / 2, width / 2 };
	int srcStep = width * 4;
	IppiSize roiSize = { width, height };

	auto status = ippiBGRToYCbCr420_8u_AC4P2R(data, srcStep, inSurface->Data.Y, width, inSurface->Data.CbCr, width, roiSize);

	inSurface->Data.TimeStamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - _initTime).count() *90;

	auto res = MFXVideoENCODE_EncodeFrameAsync(_session, NULL, inSurface, &_stream, &_syncpH264);

	//if (res == MFX_ERR_MORE_DATA)
	//{
	//	res = MFXVideoENCODE_EncodeFrameAsync(_session, NULL, NULL, &_stream, &_syncpH264);
	//}

	if (res != MFX_ERR_MORE_DATA) {

		res = MFXVideoCORE_SyncOperation(_session, _syncpH264, -1);

		if (res != MFX_ERR_NONE) {
			std::cout << "Errror at sync" + std::to_string(res) << std::endl;
		}

		outputFile.write(reinterpret_cast<char*>(_stream.Data), _stream.DataLength);

		ResetStream();
	}
}

void IPPVideoEncoder::Finish()
{
	outputFile.close();
	delete[] _stream.Data;
}

void IPPVideoEncoder::ResetStream(mfxU16 numberofSuggestedFrame, mfxU64 bufferSize) {

	if (_stream.Data != nullptr) {
		delete[] _stream.Data;
	}
	memset(&_stream, 0, sizeof(_stream));
	_stream.MaxLength = (numberofSuggestedFrame) * bufferSize;
	_stream.Data = new mfxU8[_stream.MaxLength];
}

void IPPVideoEncoder::ResetStream() {

	if (_stream.Data != nullptr) {
		delete[] _stream.Data;
	}

	auto size = _stream.MaxLength;
	memset(&_stream, 0, sizeof(_stream));
	_stream.MaxLength = size;
	_stream.Data = new mfxU8[_stream.MaxLength];
}