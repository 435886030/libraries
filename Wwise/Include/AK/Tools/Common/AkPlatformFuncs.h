//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkPlatformFuncs.h

/// \file 
/// Platform-dependent functions definition.

#ifndef _AK_TOOLS_COMMON_AKPLATFORMFUNCS_H
#define _AK_TOOLS_COMMON_AKPLATFORMFUNCS_H

#include <AK/SoundEngine/Common/AkTypes.h>

#ifdef AK_WIN
#include <AK/Tools/Win32/AkPlatformFuncs.h>

#elif defined (XBOX360)
#include <AK/Tools/XBox360/AkPlatformFuncs.h>

#elif defined (AK_PS3)
#include <AK/Tools/PS3/AkPlatformFuncs.h>

#elif defined (RVL_OS)
#include <AK/Tools/Wii/AkPlatformFuncs.h>

#elif defined (AK_MAC)
#include <AK/Tools/Mac/AkPlatformFuncs.h>

#else
#error AkPlatformFuncs.h: Undefined platform
#endif

#endif // _AK_TOOLS_COMMON_AKPLATFORMFUNCS_H
