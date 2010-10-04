//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkLock.h

/// \file 
/// Platform independent synchronization services for plug-ins.

#ifndef _AK_TOOLS_COMMON_AKLOCK_H
#define _AK_TOOLS_COMMON_AKLOCK_H

#ifdef AK_WIN
#include <AK/Tools/Win32/AkLock.h>

#elif defined (XBOX360)
#include <AK/Tools/XBox360/AkLock.h>

#elif defined (AK_PS3)
#include <AK/Tools/PS3/AkLock.h>

#elif defined (RVL_OS)
#include <AK/Tools/Wii/AkLock.h>

#elif defined (AK_MAC)
#include <AK/Tools/Mac/AkLock.h>

#else
#error AkLock.h: Undefined platform
#endif

#endif // _AK_TOOLS_COMMON_AKLOCK_H
