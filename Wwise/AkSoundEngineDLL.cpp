//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkSoundEngineDLL.h"
#include <assert.h>
#include <malloc.h>

#include <AK/SoundEngine/Common/AkMemoryMgr.h>
#include <AK/SoundEngine/Common/IAkStreamMgr.h>
#include <AK/Plugin/AkVorbisFactory.h>
#include "AkDefaultIOHookBlocking.h"

#ifndef AK_OPTIMIZED
#include <AK/Comm/AkCommunication.h>
#endif

// Defines.

// Default memory manager settings.
#define COMM_POOL_SIZE			(256 * 1024)
#define COMM_POOL_BLOCK_SIZE	(48)

namespace AK
{
	void * AllocHook( size_t in_size )
	{
		return malloc( in_size );
	}
	void FreeHook( void * in_ptr )
	{
		free( in_ptr );
	}
	void * VirtualAllocHook(
		void * in_pMemAddress,
		size_t in_size,
		DWORD in_dwAllocationType,
		DWORD in_dwProtect
		)
	{
		return VirtualAlloc( in_pMemAddress, in_size, in_dwAllocationType, in_dwProtect );
	}
	void VirtualFreeHook( 
		void * in_pMemAddress,
		size_t in_size,
		DWORD in_dwFreeType
		)
	{
		VirtualFree( in_pMemAddress, in_size, in_dwFreeType );
	}

    namespace SOUNDENGINE_DLL
    {
        CAkDefaultIOHookBlocking m_lowLevelIO;

        //-----------------------------------------------------------------------------------------
        // Sound Engine initialization.
        //-----------------------------------------------------------------------------------------
        AKRESULT Init( 
            AkMemSettings *     in_pMemSettings,
            AkStreamMgrSettings * in_pStmSettings,
            AkDeviceSettings *  in_pDefaultDeviceSettings,
            AkInitSettings *    in_pSettings,
            AkPlatformInitSettings * in_pPlatformSettings,
			AkMusicSettings *	in_pMusicSettings
            )
        {
            // Check required arguments.
            if ( !in_pMemSettings ||
                 !in_pStmSettings ||
                 !in_pDefaultDeviceSettings )
            {
                assert( !"Invalid arguments" );
                return AK_InvalidParameter;
            }

            // Create and initialise an instance of our memory manager.
            if ( MemoryMgr::Init( in_pMemSettings ) != AK_Success )
            {
                assert( !"Could not create the memory manager." );
                return AK_Fail;
            }

            // Create and initialise an instance of the default stream manager.
			if ( !StreamMgr::Create( *in_pStmSettings ) )
            {
                assert( !"Could not create the Stream Manager" );
                return AK_Fail;
            }

            // Create an IO device.
			if ( m_lowLevelIO.Init( *in_pDefaultDeviceSettings ) != AK_Success )
            {
                assert( !"Cannot create streaming I/O device" );
                return AK_Fail;
            }
            
			// Initialize sound engine.
			if ( SoundEngine::Init( in_pSettings, in_pPlatformSettings ) != AK_Success )
            {
                assert( !"Cannot initialize sound engine" );
                return AK_Fail;
            }

			// Initialize music engine.
			if ( MusicEngine::Init( in_pMusicSettings ) != AK_Success )
            {
                assert( !"Cannot initialize music engine" );
                return AK_Fail;
            }

#ifndef AK_OPTIMIZED
			// Initialize communication.
			AkCommSettings settingsComm;
			AK::Comm::GetDefaultInitSettings( settingsComm );
			if ( AK::Comm::Init( settingsComm ) != AK_Success )
			{
				assert( !"Cannot initialize music communication" );
				return AK_Fail;
			}
#endif // AK_OPTIMIZED

			// Register Vorbis codec
			SoundEngine::RegisterCodec( AKCOMPANYID_AUDIOKINETIC, 
				AKCODECID_VORBIS, 
				CreateVorbisFilePlugin, 
				CreateVorbisBankPlugin );

			return AK_Success;
        }

        //-----------------------------------------------------------------------------------------
        // Sound Engine termination.
        //-----------------------------------------------------------------------------------------
        void Term( )
        {
#ifndef AK_OPTIMIZED
			Comm::Term();
#endif // AK_OPTIMIZED

			MusicEngine::Term();

			SoundEngine::Term();

			m_lowLevelIO.Term();
            if ( IAkStreamMgr::Get() )
            	IAkStreamMgr::Get()->Destroy();
            	
	        MemoryMgr::Term();
        }

        //-----------------------------------------------------------------------------------------
        // Sound Engine periodic update.
        //-----------------------------------------------------------------------------------------
		void Tick( )
		{
			SoundEngine::RenderAudio( );
		}

        //-----------------------------------------------------------------------------------------
        // Access to LowLevelIO's file localization.
        //-----------------------------------------------------------------------------------------
        // File system interface.
		AKRESULT SetBasePath(
			const AkOSChar*   in_pszBasePath
			)
		{
			return m_lowLevelIO.SetBasePath( in_pszBasePath );
		}
		AKRESULT SetBankPath(
			const AkOSChar*   in_pszBankPath
			)
		{
			return m_lowLevelIO.SetBankPath( in_pszBankPath );
		}
		AKRESULT SetAudioSrcPath(
			const AkOSChar*   in_pszAudioSrcPath
			)
		{
			return m_lowLevelIO.SetAudioSrcPath( in_pszAudioSrcPath );
		}
		AKRESULT SetLangSpecificDirName(
			const AkOSChar*   in_pszDirName
			)
		{
			return m_lowLevelIO.SetLangSpecificDirName( in_pszDirName );
		}
    }
}
