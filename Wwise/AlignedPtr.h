//////////////////////////////////////////////////////////////////////
//
//
// Wwise sound engine integration demo. The purpose of this code
// is to demonstrate how to use the Wwise sound engine within
// your own game.
//
// Refer to the Wwise SDK documentation for more details.
//
// Copyright (c) 2006-2009 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <AK/Tools/Common/AkPlatformFuncs.h>

// This helper make aligned data on BANK_ALIGNMENT boundaries
// The implementation is platform specific.
class AlignedPtr
{
public:
	AlignedPtr();

	void* GetPtr(){ return ptr; }

	void Alloc( AkUInt32 in_uSize );

	void Free();

private:
	void* ptr;
	unsigned char* realPtr;
};