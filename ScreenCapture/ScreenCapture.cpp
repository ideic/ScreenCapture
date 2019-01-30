// ScreenCapture.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "mfxvideo.h"   /* The SDK include file */ 
#include "mfxvideo++.h" /* Optional for C++ development */ 
#include "mfxplugin.h" /* Plugin development */ 
#include <thread>
#include <chrono>
#include <iostream>


void ProcessFrame(mfxFrameSurface1* frame) {
	std::cout << "Frame Width:" << frame->Info.Width << " Frame Height:" << frame->Info.Height << "Frame TimeStamp:" << frame->Data.TimeStamp << std::endl;
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
//static const mfxPluginUID  MFX_PLUGINID_CAPTURE_HW =	  { { 0x22, 0xd6, 0x2c, 0x07, 0xe6, 0x72, 0x40, 0x8f, 0xbb, 0x4c, 0xc2, 0x0e, 0xd7, 0xa0, 0x53, 0xe4 } };
void Intel() {
	mfxSession session; 
	mfxVersion version{ 17,1 };
	auto res = MFXInit(MFX_IMPL_HARDWARE, &version, &session);
	res = MFXVideoUSER_Load(session, &MFX_PLUGINID_CAPTURE_HW, version.Major); //&MFX_PLUGINID_CAPTURE_HW
	mfxVideoParam in;	// input parameters structure 
	mfxVideoParam out;	// output parameters structure

	ConfigureVideoParams(in);
	
	/* allocate structures and fill input parameters structure, zero unused fields */ 
	res = MFXVideoDECODE_Query(session, &in, &in); 
	/* check supported parameters */ 
	mfxFrameAllocRequest request;

	res = MFXVideoDECODE_QueryIOSurf(session, &in, &request); 
	//allocate_pool_of_frame_surfaces(request.NumFrameSuggested); 

	auto suggestedType = request.Type; //MFX_MEMTYPE_FROM_DECODE  MFX_MEMTYPE_DXVA2_DECODER_TARGET MFX_MEMTYPE_EXTERNAL_FRAME
	auto suggestedFrame = request.NumFrameSuggested;

	mfxFrameSurface1 work;
	mfxFrameSurface1* outSurface = nullptr;
	mfxSyncPoint syncp;

	mfxFrameSurface1** pmfxSurfaces = new mfxFrameSurface1 *[suggestedFrame];
	mfxU8 bitsPerPixel = 12;        // NV12 format is a 12 bits per pixel format
	mfxU32 surfaceSize = in.mfx.FrameInfo.Width * in.mfx.FrameInfo.Height * bitsPerPixel / 8;
	mfxU8* surfaceBuffers = (mfxU8*) new mfxU8[surfaceSize * suggestedFrame];


	for (int i = 0; i < suggestedFrame; i++) {
		pmfxSurfaces[i] = new mfxFrameSurface1;
		memset(pmfxSurfaces[i], 0, sizeof(mfxFrameSurface1));
		memcpy(&(pmfxSurfaces[i]->Info), &(in.mfx.FrameInfo), sizeof(mfxFrameInfo));
		pmfxSurfaces[i]->Data.Y = &surfaceBuffers[surfaceSize * i];
		pmfxSurfaces[i]->Data.U = pmfxSurfaces[i]->Data.Y + in.mfx.FrameInfo.Width * in.mfx.FrameInfo.Height;
		pmfxSurfaces[i]->Data.V = pmfxSurfaces[i]->Data.U + 1;
		pmfxSurfaces[i]->Data.Pitch = in.mfx.FrameInfo.Width;
	}

	res = MFXVideoDECODE_Init(session, &in); 
	for (;;) {  
		//find_unlocked_surface_from_the_pool(&work);  
		res=MFXVideoDECODE_DecodeFrameAsync(session, NULL, pmfxSurfaces[0], &outSurface, &syncp);  
		if (res==MFX_ERR_MORE_SURFACE) continue;  
		// other error handling  
		if (res==MFX_ERR_NONE) {   
			res = MFXVideoCORE_SyncOperation(session, syncp, -1);   
			ProcessFrame(outSurface);  
		}

		std::this_thread::sleep_for(std::chrono::seconds(30));
	} 
	MFXVideoDECODE_Close(session); 
	//free_pool_of_frame_surfaces(); 
	MFXClose(session);
}



int main()
{
	Intel();
	return 0;
}
