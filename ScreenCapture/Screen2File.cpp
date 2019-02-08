#include "stdafx.h"
#include "Screen2File.h"
#include <iostream>
#include <fstream>
#include <thread>

Screen2File::Screen2File(uint8_t framrate, std::string output): _framrate(framrate), _output(output), _terminate(false)
{
	
	mfxVersion version{ 23,1 };
	auto res = MFXInit(MFX_IMPL_HARDWARE, &version, &_session);

	if (res != MFX_ERR_NONE) {
		throw std::runtime_error("Cannot Initialize  Intel MFX Error Code" + std::to_string(res));
	}

	res = MFXVideoUSER_Load(_session, &MFX_PLUGINID_CAPTURE_HW, version.Major); //&MFX_PLUGINID_CAPTURE_HW

	if (res != MFX_ERR_NONE) {
		throw std::runtime_error("Cannot Load Plugin MFX_PLUGINID_CAPTURE_HW Error: " + std::to_string(res));
	}

	res = MFXVideoUSER_Load(_session, &MFX_PLUGINID_H264LA_HW, version.Major); //&MFX_PLUGINTYPE_VIDEO_ENCODE
	if (res != MFX_ERR_NONE) {
		throw std::runtime_error("Cannot Load Plugin MFX_PLUGINID_H264LA_HW Error: " + std::to_string(res));
	}

}


Screen2File::~Screen2File()
{
	MFXVideoDECODE_Close(_session);
	MFXVideoENCODE_Close(_session);
	MFXClose(_session);
}

void Screen2File::ConfigureScreenCaptureParams(mfxVideoParam & conf) {

	memset(&conf, 0, sizeof(conf));
	conf.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
	conf.mfx.CodecId = MFX_CODEC_CAPTURE;
	conf.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
	conf.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
	conf.mfx.FrameInfo.Width = conf.mfx.FrameInfo.CropW = 1920;
	conf.mfx.FrameInfo.Height = 1088;
	conf.mfx.FrameInfo.CropH = 1080;
}

void Screen2File::ConfigureH264Params(mfxVideoParam & conf) {

	memset(&conf, 0, sizeof(conf));

	conf.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
	conf.mfx.CodecId = MFX_CODEC_AVC;
	conf.mfx.TargetUsage = MFX_TARGETUSAGE_BEST_QUALITY;
	//conf.RateControlMethod = MFX_RATECONTROL_VBR;
	conf.mfx.TargetKbps = 10466;

	// For encoder, Width must be a multiple of 16
	// Height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
	conf.mfx.FrameInfo.Width = 1920;
	conf.mfx.FrameInfo.Height = 1088;
	conf.mfx.FrameInfo.CropW = 1920;
	conf.mfx.FrameInfo.CropH = 1080;
	conf.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
	conf.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
	conf.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
	conf.mfx.FrameInfo.FrameRateExtN = _framrate; // FrameRate is N/D, 4 is the minimal for ffplay
	conf.mfx.FrameInfo.FrameRateExtD = 1;
}

void Screen2File::StartCapture()
{
	mfxVideoParam screenCaptureParam;	// input parameters structure 
	mfxVideoParam h264VideoParam;	// output parameters structure

	ConfigureScreenCaptureParams(screenCaptureParam);
	ConfigureH264Params(h264VideoParam);

	/* allocate structures and fill input parameters structure, zero unused fields */
	auto res = MFXVideoDECODE_Query(_session, &screenCaptureParam, &screenCaptureParam);

	if (res != MFX_ERR_NONE && res != MFX_WRN_PARTIAL_ACCELERATION) {
		std::cout << " MFXVideoDECODE_Query screenCapture failed with code" << std::to_string(res) << std::endl;
		return;
	}

	res = MFXVideoENCODE_Query(_session, &h264VideoParam, &h264VideoParam);

	if (res != MFX_ERR_NONE && res != MFX_WRN_PARTIAL_ACCELERATION) {
		std::cout << " MFXVideoDECODE_Query h264VideoParam failed with code" << std::to_string(res) << std::endl;
		return;
	}

	/* check supported parameters */
	mfxFrameAllocRequest requestScreenCapture;
	mfxFrameAllocRequest requestH264;

	res = MFXVideoDECODE_QueryIOSurf(_session, &screenCaptureParam, &requestScreenCapture);

	if (res != MFX_ERR_NONE) {
		std::cout << "MFXVideoDECODE_QueryIOSurf screenCapture failed with code" << std::to_string(res) << std::endl;
		return;
	}

	res = MFXVideoENCODE_QueryIOSurf(_session, &h264VideoParam, &requestH264);

	if (res != MFX_ERR_NONE) {
		std::cout << "MFXVideoDECODE_QueryIOSurf h264VideoParam failed with code" << std::to_string(res) << std::endl;
		return;
	}

	mfxFrameSurface1** pmfxSurfaces = CreateSurfaces(requestH264.NumFrameSuggested, h264VideoParam.mfx.FrameInfo.Width, h264VideoParam.mfx.FrameInfo.Height, h264VideoParam.mfx.FrameInfo);


	res = MFXVideoDECODE_Init(_session, &screenCaptureParam);

	if (res != MFX_ERR_NONE && res != MFX_WRN_PARTIAL_ACCELERATION) {
		std::cout << " MFXVideoDECODE_Init ScreenCapture failed with code" << std::to_string(res) << std::endl;
		return;
	}

	res = MFXVideoENCODE_Init(_session, &h264VideoParam);

	if (res != MFX_ERR_NONE && res != MFX_WRN_PARTIAL_ACCELERATION) {
		std::cout << " MFXVideoDECODE_Init h264VideoParam failed with code" << std::to_string(res) << std::endl;
		return;
	}


	mfxFrameSurface1* outSurface = nullptr;

	mfxSyncPoint syncpScreenCapture;
	mfxSyncPoint syncpH264;


	mfxBitstream stream;
	stream.Data = nullptr;

	ResetStream(stream, requestH264.NumFrameSuggested, requestH264.Info.BufferSize);

	int idx = 0;

	std::ofstream outputFile;
	outputFile.open(_output, std::ofstream::binary);
	if (outputFile.fail()) {
		std::cout << strerror(errno) << std::endl;
		return;
	}

	while (!_terminate) {

		//find_unlocked_surface_from_the_pool(&work);  
		res = MFXVideoDECODE_DecodeFrameAsync(_session, NULL, pmfxSurfaces[idx], &outSurface, &syncpScreenCapture);

		if (res == MFX_ERR_MORE_SURFACE) continue;
		idx++;

		if (res == MFX_ERR_NONE) {
			//res = MFXVideoCORE_SyncOperation(_session, syncpScreenCapture, -1); // no matter we call this or not
			//ProcessFrame(outSurface);

			res = MFXVideoENCODE_EncodeFrameAsync(_session, NULL, outSurface, &stream, &syncpH264);
			//no more frame
			if (res == MFX_ERR_MORE_DATA)
			{
				res = MFXVideoENCODE_EncodeFrameAsync(_session, NULL, NULL, &stream, &syncpH264);
			}
			if (res != MFX_ERR_MORE_DATA) {

				res = MFXVideoCORE_SyncOperation(_session, syncpH264, -1);

				outputFile.write(reinterpret_cast<char*>(stream.Data), stream.DataLength);

				idx = 0;
				//ResetStream(stream, requestH264.NumFrameSuggested, requestH264.Info.BufferSize);
			}
		}

		// Only to be aligned with the frame rate
		std::this_thread::sleep_for(std::chrono::seconds(60 / _framrate));
	}

	outputFile.close();

	delete[] stream.Data;

	delete[] pmfxSurfaces;
	
}

void Screen2File::StopCapture()
{
	_terminate = true;
}

mfxFrameSurface1** Screen2File::CreateSurfaces(mfxU16 suggestedFrame, mfxU16 width, mfxU16 height, mfxFrameInfo  &frameInfo) {
	mfxFrameSurface1** pmfxSurfaces = new mfxFrameSurface1 *[suggestedFrame];
	mfxU8 bitsPerPixel = 12;        // NV12 format is a 12 bits per pixel format
	mfxU32 surfaceSize = width * height * bitsPerPixel / 8;
	mfxU8* surfaceBuffers = (mfxU8*) new mfxU8[surfaceSize * suggestedFrame];


	for (int i = 0; i < suggestedFrame; i++) {
		pmfxSurfaces[i] = new mfxFrameSurface1;
		memset(pmfxSurfaces[i], 0, sizeof(mfxFrameSurface1));
		memcpy(&(pmfxSurfaces[i]->Info), &(frameInfo), sizeof(mfxFrameInfo));
		pmfxSurfaces[i]->Data.Y = &surfaceBuffers[surfaceSize * i];
		pmfxSurfaces[i]->Data.U = pmfxSurfaces[i]->Data.Y + width * height;
		pmfxSurfaces[i]->Data.V = pmfxSurfaces[i]->Data.U + 1;
		pmfxSurfaces[i]->Data.Pitch = width;
	}

	return pmfxSurfaces;
}

void Screen2File::ResetStream(mfxBitstream &stream, mfxU16 numberofSuggestedFrame, mfxU64 bufferSize) {

	if (stream.Data != nullptr) {
		delete[] stream.Data;
	}
	memset(&stream, 0, sizeof(stream));
	stream.MaxLength = numberofSuggestedFrame * bufferSize;
	stream.Data = new mfxU8[stream.MaxLength];

	//stream.Data = new mfxU8[requestEncode.Info.BufferSize*3*6];
	//stream.DataLength = requestEncode.Info.BufferSize*3*6;

}

void Screen2File::ProcessFrame(mfxFrameSurface1* frame) {
	std::cout << "Frame Width:" << frame->Info.Width << " Frame Height:" << frame->Info.Height << "Frame TimeStamp:" << frame->Data.TimeStamp << std::endl;
	auto  yv12 = MFX_FOURCC_NV12;
	auto frameFourCC = frame->Info.FourCC;

	auto pitch = frame->Data.Pitch;
	mfxU8* ptr;
	ptr = frame->Data.Y + (frame->Info.CropX) + (frame->Info.CropY) * pitch;

	auto width = frame->Info.CropW;
	auto height = frame->Info.CropH;

	std::ofstream outputFile;
	outputFile.open("d:\\Idei\\POC\\ScreenCapture\\x64\\Debug\\res.raw", std::ofstream::binary);

	if (outputFile.fail()) {
		std::cout << strerror(errno) << std::endl;
		return;
	};
	for (int i = 0; i < height; i++)
	{
		outputFile.write(reinterpret_cast<char*>(ptr + i * pitch), width);

	}

	// write UV data
	height >>= 1;
	ptr = frame->Data.UV + (frame->Info.CropX) + (frame->Info.CropY >> 1) * pitch;

	for (int i = 0; i < height; i++)
	{
		outputFile.write(reinterpret_cast<char*>(ptr + i * pitch), width);
	}


	outputFile.close();
}