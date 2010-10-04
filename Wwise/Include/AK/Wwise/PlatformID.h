//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

/// \file
/// Unique identifiers for platforms in the Wwise authoring application.

#ifndef AK_WWISE_PLATFORMID_H
#define AK_WWISE_PLATFORMID_H

namespace PlatformID
{
    // {6E0CB257-C6C8-4c5c-8366-2740DFC441EB}
    const GUID Windows = { 0x6E0CB257, 0xC6C8, 0x4c5c, { 0x83, 0x66, 0x27, 0x40, 0xDF, 0xC4, 0x41, 0xEB } };

    // {E0C09284-6F61-43dc-9C9D-D8047E47AB3B}
    const GUID XBox360 = { 0xE0C09284, 0x6F61, 0x43dc, { 0x9C, 0x9D, 0xD8, 0x04, 0x7E, 0x47, 0xAB, 0x3B } };

	// {D85DACB3-8FDB-4aba-8C8A-1F46AFB35366}
    const GUID PS3 = { 0xD85DACB3, 0x8FDB, 0x4aba, { 0x8C, 0x8A, 0x1F, 0x46, 0xAF, 0xB3, 0x53, 0x66 } };

    // {A11C9D5D-C4ED-42af-99E3-0376D0E11620}
	const GUID Wii = { 0xa11c9d5d, 0xc4ed, 0x42af, { 0x99, 0xe3, 0x3, 0x76, 0xd0, 0xe1, 0x16, 0x20 } };

	// {9C6217D5-DD11-4795-87C1-6CE02853C540}
	const GUID Mac = { 0x9c6217d5, 0xdd11, 0x4795, { 0x87, 0xc1, 0x6c, 0xe0, 0x28, 0x53, 0xc5, 0x40 } };

	/// Returns true if the given platform has Big Endian byte ordering. 
	inline bool IsPlatformBigEndian( const GUID & in_guidPlatform )
	{
		return in_guidPlatform == PlatformID::Wii 
			|| in_guidPlatform == PlatformID::PS3 
			|| in_guidPlatform == PlatformID::XBox360;
	}
}

#endif // AK_WWISE_PLATFORMID_H
