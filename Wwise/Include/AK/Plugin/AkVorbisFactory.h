//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkVorbisFactory.h

/// \file
/// Codec plug-in unique ID and creation functions (hooks) necessary to register the Vorbis codec in the sound engine.

#ifndef _AK_VORBISFACTORY_H_
#define _AK_VORBISFACTORY_H_

#ifdef AKSOUNDENGINE_DLL

#ifndef __APPLE__
// VorbisDecoder
	#ifdef AKVORBISDECODER_EXPORTS
		#define AKVORBISDECODER_API __declspec(dllexport) ///< Vorbis decoder API exportation definition
	#else
		#define AKVORBISDECODER_API __declspec(dllimport) ///< Vorbis decoder API exportation definition
#endif // Export
#else
	// Todo : Check that we need AKVORBISDECODER_API
	//#define DLL_PUBLIC __attribute__ ((visibility("default")))
    //#define DLL_LOCAL  __attribute__ ((visibility("hidden")))
#define AKVORBISDECODER_API
#endif
#else

#define AKVORBISDECODER_API

#endif // AKSOUNDENGINE_DLL

const unsigned long AKCODECID_VORBIS = 4;	///< Unique ID of the Vorbis codec plug-in

class IAkSoftwareCodec;
/// Prototype of the Vorbis codec bank source creation function.
AKVORBISDECODER_API IAkSoftwareCodec* CreateVorbisBankPlugin( 
	void* in_pCtx			///< Bank source decoder context
	);

/// Prototype of the Vorbis codec file source creation function.
AKVORBISDECODER_API IAkSoftwareCodec* CreateVorbisFilePlugin( 
	void* in_pCtx 			///< File source decoder context
	);

/*
Use the following code to register this codec:

	AK::SoundEngine::RegisterCodec(
		AKCOMPANYID_AUDIOKINETIC,
		AKCODECID_VORBIS,
		CreateVorbisFilePlugin,
		CreateVorbisBankPlugin );
*/

#endif // _AK_VORBISFACTORY_H_
