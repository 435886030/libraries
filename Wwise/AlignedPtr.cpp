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

#include "AlignedPtr.h"

//
// The required alignment depends on the platform.
// The alignment requirement is 16 bytes for banks on Windows.
// If this requirement is not respected under Windows, there may be performance issues.
//

#define WIN32_BANK_MEMORY_ALIGNMENT	16

#define BANK_ALIGNMENT				WIN32_BANK_MEMORY_ALIGNMENT


AlignedPtr::AlignedPtr()
		:ptr( NULL )
		,realPtr( NULL )
{
}

void AlignedPtr::Alloc( AkUInt32 in_uSize )
{
	if( realPtr == NULL )
	{
		realPtr = ( unsigned char* )malloc( in_uSize + BANK_ALIGNMENT );
		if( realPtr )
		{
			ptr = ( realPtr + BANK_ALIGNMENT ) - ( (UINT_PTR)realPtr % BANK_ALIGNMENT );
		}
	}
}

void AlignedPtr::Free()
{
	if( ptr )
	{
		free( realPtr );
		ptr = NULL;
		realPtr = NULL;
	}
}