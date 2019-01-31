// ScreenCapture.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "mfxvideo.h"   /* The SDK include file */ 
#include "mfxvideo++.h" /* Optional for C++ development */ 
#include "mfxplugin.h" /* Plugin development */ 
#include <thread>
#include <chrono>
#include <iostream>
#include <fstream>


void ProcessFrame(mfxFrameSurface1* frame) {
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
		outputFile.write(reinterpret_cast<char*>(ptr + i * pitch),width);

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

void ConfigureVideoParams(mfxVideoParam & conf) {

	memset(&conf, 0, sizeof(conf));
	conf.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
	conf.mfx.CodecId = MFX_CODEC_CAPTURE;
	conf.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
	conf.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
	conf.mfx.FrameInfo.Width = conf.mfx.FrameInfo.CropW = 1920;
	conf.mfx.FrameInfo.Height = 1088;
	conf.mfx.FrameInfo.CropH = 1080;

}

void ConfigureEncodeParams(mfxVideoParam & conf) {

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
	conf.mfx.FrameInfo.FrameRateExtN = 4; // FrameRate is N/D, 4 is the minimal for ffplay
	conf.mfx.FrameInfo.FrameRateExtD = 1;
}

mfxFrameSurface1** CreateSurfaces(mfxU16 suggestedFrame, mfxU16 width, mfxU16 height, mfxFrameInfo  &frameInfo) {
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

//static const mfxPluginUID  MFX_PLUGINID_CAPTURE_HW =	  { { 0x22, 0xd6, 0x2c, 0x07, 0xe6, 0x72, 0x40, 0x8f, 0xbb, 0x4c, 0xc2, 0x0e, 0xd7, 0xa0, 0x53, 0xe4 } };
void Intel() {
	mfxSession session; 
	mfxVersion version{ 23,1 };
	auto res = MFXInit(MFX_IMPL_HARDWARE, &version, &session);
	res = MFXVideoUSER_Load(session, &MFX_PLUGINID_CAPTURE_HW, version.Major); //&MFX_PLUGINID_CAPTURE_HW
	res = MFXVideoUSER_Load(session, &MFX_PLUGINID_H264LA_HW, version.Major); //&MFX_PLUGINTYPE_VIDEO_ENCODE
	mfxVideoParam in;	// input parameters structure 
	mfxVideoParam encode;	// output parameters structure

	ConfigureVideoParams(in);
	ConfigureEncodeParams(encode);
	
	/* allocate structures and fill input parameters structure, zero unused fields */ 
	res = MFXVideoDECODE_Query(session, &in, &in); 
	res = MFXVideoENCODE_Query(session, &encode, &encode);
	/* check supported parameters */ 
	mfxFrameAllocRequest request;
	mfxFrameAllocRequest requestEncode;

	res = MFXVideoDECODE_QueryIOSurf(session, &in, &request); 
	res = MFXVideoENCODE_QueryIOSurf(session, &encode, &requestEncode);
	//allocate_pool_of_frame_surfaces(request.NumFrameSuggested); 


	mfxFrameSurface1* outSurface = nullptr;

	mfxSyncPoint syncp;
	mfxSyncPoint syncpEncode;

	mfxFrameSurface1** pmfxSurfaces = CreateSurfaces(request.NumFrameSuggested, in.mfx.FrameInfo.Width, in.mfx.FrameInfo.Height, in.mfx.FrameInfo);
	mfxFrameSurface1** pmfxSurfacesEncode = CreateSurfaces(requestEncode.NumFrameSuggested, encode.mfx.FrameInfo.Width, encode.mfx.FrameInfo.Height, encode.mfx.FrameInfo);


	res = MFXVideoDECODE_Init(session, &in);
	res = MFXVideoENCODE_Init(session, &encode);

	mfxBitstream stream;
	memset(&stream, 0, sizeof(stream));
	stream.MaxLength = requestEncode.NumFrameSuggested * requestEncode.Info.BufferSize;
	stream.Data = new mfxU8[stream.MaxLength];
	//stream.Data = new mfxU8[requestEncode.Info.BufferSize*3*6];
	//stream.DataLength = requestEncode.Info.BufferSize*3*6;


	int idx = 0;
	for (;;) {  
		//find_unlocked_surface_from_the_pool(&work);  
		res=MFXVideoDECODE_DecodeFrameAsync(session, NULL, pmfxSurfacesEncode[idx], &outSurface, &syncp);  
		if (res==MFX_ERR_MORE_SURFACE) continue;  
		idx++;

		if (res==MFX_ERR_NONE) {   
			//res = MFXVideoCORE_SyncOperation(session, syncp, -1);
			//ProcessFrame(outSurface);

			mfxStatus encodeStat = MFX_ERR_MORE_DATA;
			/*while (encodeStat != MFX_ERR_NONE) */{
				res = MFXVideoENCODE_EncodeFrameAsync(session, NULL, outSurface, &stream, &syncpEncode);
				//no more frame
				res = MFXVideoENCODE_EncodeFrameAsync(session, NULL, NULL, &stream, &syncpEncode);
				encodeStat = res;
				if (res != MFX_ERR_MORE_DATA) {

					res = MFXVideoCORE_SyncOperation(session, syncpEncode, -1);

					std::ofstream outputFile;
					outputFile.open("d:\\Idei\\POC\\ScreenCapture\\x64\\Debug\\res.h264", std::ofstream::binary);
					if (!outputFile.fail()) {
						outputFile.write(reinterpret_cast<char*>(stream.Data), stream.DataLength);
					}
					else {
						std::cout << strerror(errno) << std::endl;
					}
					outputFile.close();
					idx = 0;
					break;
				}
			}
		}

		std::this_thread::sleep_for(std::chrono::seconds(10));
	} 
	MFXVideoDECODE_Close(session); 
	MFXVideoENCODE_Close(session);
	//free_pool_of_frame_surfaces(); 
	MFXClose(session);
}



int main()
{
	Intel();
	return 0;
}
