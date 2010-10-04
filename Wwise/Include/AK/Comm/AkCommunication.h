//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

/// \file 
/// The main communication interface (between the in-game sound engine and
/// authoring tool).
/// \sa
/// - \ref initialization_comm
/// - \ref termination_comm

#ifndef _AK_COMMUNICATION_H
#define _AK_COMMUNICATION_H

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkMemoryMgr.h>

/// Platform-independent initialization settings of communication module between the Wwise sound engine
/// and authoring tool.
/// \sa 
/// - AK::Comm::Init()
struct AkCommSettings
{
	AkUInt32	uPoolSize;		///< Size of the communication pool, in bytes. 
};

namespace AK
{
	namespace Comm
	{
		///////////////////////////////////////////////////////////////////////
		/// @name Initialization
		//@{

		/// Initializes the communication module. When this is called, and AK::SoundEngine::RenderAudio()
		/// is called periodically, you may use the authoring tool to connect to the sound engine.
		/// \warning This function must be called after the sound engine and memory manager have been properly initialized.
		/// \return AK_Success if the Init was successful, AK_Fail otherwise.
		/// \sa
		/// - \ref initialization_comm
        extern AKSOUNDENGINE_API AKRESULT Init(
			const AkCommSettings &	in_settings	///< Initialization settings.
			);

		/// Gets the communication module's default initialization settings values.
		/// \sa
		/// - \ref initialization_comm 
		/// - AK::Comm::Init()
		extern AKSOUNDENGINE_API void GetDefaultInitSettings(
            AkCommSettings &	out_settings	///< Returned default intialization settings.
		    );
		
		/// Terminates the communication module.
		/// \warning This function must be called before the memory manager is terminated.
		/// \sa
		/// - \ref termination_comm 
        extern AKSOUNDENGINE_API void Term();

        //@}
	}
}

#endif // _AK_COMMUNICATION_H
