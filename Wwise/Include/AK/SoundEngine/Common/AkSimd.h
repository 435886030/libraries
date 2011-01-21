//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkSimd.h

/// \file 
/// Simd definitions.

#ifndef _AK_SIMD_H_
#define _AK_SIMD_H_

// Platform-specific section.
//----------------------------------------------------------------------------------------------------

#if defined( WIN32 ) || defined ( WIN64 )
	//----------------------------------------------------------------------------------------------------
	// Windows
	//----------------------------------------------------------------------------------------------------
	#include <AK/SoundEngine/Platforms/Windows/AkSimd.h>
#elif defined( __APPLE__ )
	//----------------------------------------------------------------------------------------------------
	// Mac
	//----------------------------------------------------------------------------------------------------
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR
	#include <AK/SoundEngine/Platforms/iPhone/AkSimd.h>
#else

#include <AK/SoundEngine/Platforms/Mac/AkSimd.h>
#endif
#elif defined( XBOX360 )
	//----------------------------------------------------------------------------------------------------
	// XBOX360
	//----------------------------------------------------------------------------------------------------
	#include <AK/SoundEngine/Platforms/XBox360/AkSimd.h>
#elif defined (__PPU__) || defined (__SPU__)
	//----------------------------------------------------------------------------------------------------
	// PS3
	//----------------------------------------------------------------------------------------------------
	#include <AK/SoundEngine/Platforms/PS3/AkSimd.h>
#elif defined( RVL_OS )
	//----------------------------------------------------------------------------------------------------
	// Wii
	//----------------------------------------------------------------------------------------------------
	#include <AK/SoundEngine/Platforms/Wii/AkSimd.h>
#else
	#error Unsupported platform, or platform-specific SIMD not defined
#endif

#endif  //_AK_DATA_TYPES_H_
