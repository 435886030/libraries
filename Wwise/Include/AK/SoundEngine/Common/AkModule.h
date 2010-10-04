//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

/// \file 
/// Audiokinetic's definitions and factory of overridable Stream Manager module.

#ifndef __AK_SOUNDENGINE_COMMON_AKMODULE_H__
#define __AK_SOUNDENGINE_COMMON_AKMODULE_H__

#if defined (WIN32) || defined (WIN64)
#include <AK/SoundEngine/Platforms/Windows/AkModule.h>

#elif defined (AK_MAC)
#include <AK/SoundEngine/Platforms/Mac/AkModule.h>

#elif defined (XBOX360)
#include <AK/SoundEngine/Platforms/XBox360/AkModule.h>

#elif defined (AK_PS3)
#include <AK/SoundEngine/Platforms/PS3/AkModule.h>

#elif defined (RVL_OS)
#include <AK/SoundEngine/Platforms/Wii/AkModule.h>

#else
#error AkModule.h: Platform is not defined 

#endif

#endif // __AK_SOUNDENGINE_COMMON_AKMODULE_H__
