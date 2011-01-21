//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// IAkMotionMixBus.h

#ifndef _IMOTIONBUS_H
#define _IMOTIONBUS_H

#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include <AK/SoundEngine/Common/AkCommonDefs.h>
#include <AK/SoundEngine/Common/IAkPlugin.h>

#ifdef RVL_OS
#define AK_FEEDBACK_SAMPLE_RATE 83		
#else
#define AK_FEEDBACK_SAMPLE_RATE 375
#endif

#if defined( AK_WIN ) || defined( AK_MAC )

namespace AkAudioLibSettings
{
	extern AkUInt32 g_pipelineCoreFrequency;
};

#define AK_CORE_SAMPLERATE		AkAudioLibSettings::g_pipelineCoreFrequency

#else

#define AK_CORE_SAMPLERATE		DEFAULT_NATIVE_FREQUENCY

#endif

//Maximum frames per buffer.  Derived from the maximum audio frames.
#ifdef RVL_OS
#define AK_FEEDBACK_MAX_FRAMES_PER_BUFFER 2
#else
#define AK_FEEDBACK_MAX_FRAMES_PER_BUFFER (AkUInt16)(AK_NUM_VOICE_REFILL_FRAMES * AK_FEEDBACK_SAMPLE_RATE / AK_CORE_SAMPLERATE )
#endif

class IAkMotionMixBus : public AK::IAkPlugin
{
public:
	virtual AKRESULT 	Init(AK::IAkPluginMemAlloc * in_pAllocator, AkPlatformInitSettings * io_pPDSettings, AkUInt8 in_iPlayer, void * in_pDevice = NULL) = 0;

	virtual AKRESULT	MixAudioBuffer( AkAudioBuffer &io_rBuffer ) = 0;
	virtual AKRESULT	MixFeedbackBuffer( AkAudioBuffer &io_rBuffer ) = 0;
	virtual AKRESULT	RenderData() = 0;
	virtual void		CommandTick() = 0;
	virtual void		Stop() = 0;

	virtual AkReal32	GetPeak() = 0;
	virtual bool		IsStarving() = 0;
	virtual bool		IsActive() = 0;
	virtual AkChannelMask GetMixingFormat() = 0;
	virtual void		SetMasterVolume(AkReal32 in_fVol) = 0;

	virtual void		StartOutputCapture(const AkOSChar* in_CaptureFileName) = 0;
	virtual void		StopOutputCapture() = 0;
};
#endif
