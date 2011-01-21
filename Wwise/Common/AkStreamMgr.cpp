//////////////////////////////////////////////////////////////////////
//
// AkStreamMgr.cpp
//
// Stream manager Windows-specific implementation:
// Device factory.
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkStreamMgr.h"
#include <AK/Tools/Common/AkMonitorError.h>
#include "AkStreamingDefaults.h"

// Factory products.
#include "AkDeviceBlocking.h"
#include "AkDeviceDeferredLinedUp.h"
#include <wchar.h>
#include <stdio.h>

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Declaration of the one and only global pointer to the stream manager.
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
AKSTREAMMGR_API AK::IAkStreamMgr * AK::IAkStreamMgr::m_pStreamMgr = NULL;

//-----------------------------------------------------------------------
// Global variables.
//-----------------------------------------------------------------------
AK::StreamMgr::IAkFileLocationResolver * AK::StreamMgr::CAkStreamMgr::m_pFileLocationResolver = NULL;
AK::StreamMgr::CAkStreamMgr::AkDeviceArray AK::StreamMgr::CAkStreamMgr::m_arDevices;
AkMemPoolId AK::StreamMgr::CAkStreamMgr::m_streamMgrPoolId = AK_INVALID_POOL_ID;
#ifndef AK_OPTIMIZED
	AkInt32 AK::StreamMgr::CAkStreamMgr::m_iNextStreamID = 0;
#endif

//-----------------------------------------------------------------------------
// Factory.
//-----------------------------------------------------------------------------
AK::IAkStreamMgr * AK::StreamMgr::Create( 
    const AkStreamMgrSettings &	in_settings		// Stream manager initialization settings.
    )
{
    // Check memory manager.
    if ( !AK::MemoryMgr::IsInitialized() )
    {
        AKASSERT( !"Memory manager does not exist" );
        return NULL;
    }

    // Factory.
    AKASSERT( AK::IAkStreamMgr::m_pStreamMgr == NULL || !"CreateStreamMgr( ) should be called only once" );
    if ( AK::IAkStreamMgr::m_pStreamMgr == NULL )
    {
        // Create stream manager.
        if ( CAkStreamMgr::m_streamMgrPoolId == AK_INVALID_POOL_ID )
        {
            // Create stream manager objects pool.
            CAkStreamMgr::m_streamMgrPoolId = AK::MemoryMgr::CreatePool( NULL,
                                                    in_settings.uMemorySize,
                                                    AK_STM_OBJ_POOL_BLOCK_SIZE,
                                                    AkMalloc );
        }
	    if ( CAkStreamMgr::m_streamMgrPoolId == AK_INVALID_POOL_ID )
		{
            AKASSERT( !"Stream manager pool creation failed" );
			return NULL;
		}
		AK_SETPOOLNAME(CAkStreamMgr::m_streamMgrPoolId,L"Stream Manager");
        
        // Instantiate stream manager.
        CAkStreamMgr * pStreamMgr = AkNew( CAkStreamMgr::m_streamMgrPoolId, CAkStreamMgr() );
        
        // Initialize.
        if ( pStreamMgr != NULL )
        {
            if ( pStreamMgr->Init( in_settings ) != AK_Success )
            {
                // Failed. Clean up.
                AKASSERT( !"Failed intializing stream manager" );
                pStreamMgr->Destroy( );
                pStreamMgr = NULL;
            }
        }

        // If instantiation failed, need to destroy stm mgr pool. 
        if ( pStreamMgr == NULL )
        {
            AKVERIFY( AK::MemoryMgr::DestroyPool( CAkStreamMgr::m_streamMgrPoolId ) == AK_Success );
        }
    }
        
	AKASSERT( AK::IAkStreamMgr::m_pStreamMgr != NULL );
    return AK::IAkStreamMgr::m_pStreamMgr;
}

void AK::StreamMgr::GetDefaultSettings(
	AkStreamMgrSettings &		out_settings
	)
{
	out_settings.uMemorySize             = AK_DEFAULT_STM_OBJ_POOL_SIZE;
}

void AK::StreamMgr::GetDefaultDeviceSettings(
	AkDeviceSettings &			out_settings
	)
{
	out_settings.pIOMemory				= NULL;
	out_settings.uIOMemorySize			= AK_DEFAULT_DEVICE_IO_POOL_SIZE;
	out_settings.uIOMemoryAlignment		= AK_REQUIRED_IO_POOL_ALIGNMENT;
	out_settings.ePoolAttributes		= AK_DEFAULT_BLOCK_ALLOCATION_TYPE;

	out_settings.uGranularity			= AK_DEFAULT_DEVICE_GRANULARITY;
	out_settings.uSchedulerTypeFlags	= AK_DEFAULT_DEVICE_SCHEDULER;
	
	AKPLATFORM::AkGetDefaultThreadProperties( out_settings.threadProperties );
	
	// I/O thread uses a thread priority above normal.
	out_settings.threadProperties.nPriority	= AK_DEFAULT_DEVICE_THREAD_PRIORITY;

	out_settings.fTargetAutoStmBufferLength = AK_DEFAULT_DEVICE_BUFFERING_LENGTH;
	out_settings.uIdleWaitTime				= AK_DEFAULT_IDLE_WAIT_TIME;
	out_settings.uMaxConcurrentIO			= AK_DEFAULT_MAX_CONCURRENT_IO;
}

AK::StreamMgr::IAkFileLocationResolver * AK::StreamMgr::GetFileLocationResolver()
{
	AKASSERT( AK::IAkStreamMgr::m_pStreamMgr 
			|| !"Trying to get file location resolver before StreamManager was created" );
	return CAkStreamMgr::m_pFileLocationResolver;
}

void AK::StreamMgr::SetFileLocationResolver(
	AK::StreamMgr::IAkFileLocationResolver *	in_pFileLocationResolver
	)
{
	AKASSERT( AK::IAkStreamMgr::m_pStreamMgr 
			|| !"Trying to set file location handler before StreamManager was created" );
	CAkStreamMgr::m_pFileLocationResolver = in_pFileLocationResolver;
}

// Device creation.
AkDeviceID AK::StreamMgr::CreateDevice(
    const AkDeviceSettings &	in_settings,		// Device settings.
	IAkLowLevelIOHook *			in_pLowLevelHook	// Device specific low-level I/O hook.
    )
{
    return static_cast<CAkStreamMgr*>(AK::IAkStreamMgr::Get())->CreateDevice( in_settings, in_pLowLevelHook );
}
AKRESULT AK::StreamMgr::DestroyDevice(
    AkDeviceID                  in_deviceID         // Device ID.
    )
{
    return static_cast<CAkStreamMgr*>(AK::IAkStreamMgr::Get())->DestroyDevice( in_deviceID );
}


//--------------------------------------------------------------------
// class CAkStreamMgr
//--------------------------------------------------------------------
using namespace AK::StreamMgr;

void CAkStreamMgr::Destroy()
{
    Term( );

    // Destroy singleton.
    AKASSERT( AK::MemoryMgr::IsInitialized() &&
              m_streamMgrPoolId != AK_INVALID_POOL_ID );
    if ( AK::MemoryMgr::IsInitialized() &&
         m_streamMgrPoolId != AK_INVALID_POOL_ID )
    {
        AkDelete( m_streamMgrPoolId, this );
    }

    // Destroy stream manager pool.
    AKVERIFY( AK::MemoryMgr::DestroyPool( m_streamMgrPoolId ) == AK_Success );
    m_streamMgrPoolId = AK_INVALID_POOL_ID;
}

CAkStreamMgr::CAkStreamMgr()
{
    // Assign global pointer.
    m_pStreamMgr = this;
}

CAkStreamMgr::~CAkStreamMgr()
{
    // Reset global pointer.
    m_pStreamMgr = NULL;
}


// Initialise/Terminate.
//-------------------------------------
AKRESULT CAkStreamMgr::Init(
    const AkStreamMgrSettings & /*in_settings*/
    )
{
	return AK_Success;
}

void CAkStreamMgr::Term()
{
	CAkStreamMgr::m_pFileLocationResolver = NULL;
	
    // Destroy devices remaining.
    AkDeviceArray::Iterator it = m_arDevices.Begin( );
    while ( it != m_arDevices.End( ) )
    {
		if ( (*it) )
			(*it)->Destroy( );
        ++it;
    }
    m_arDevices.Term( );
}

//-----------------------------------------------------------------------------
// Device management.
// Warning: These functions are not thread safe.
//-----------------------------------------------------------------------------
// Device creation.
AkDeviceID CAkStreamMgr::CreateDevice(
    const AkDeviceSettings &	in_settings,		// Device settings.
	IAkLowLevelIOHook *			in_pLowLevelHook	// Device specific low-level I/O hook.
    )
{
	AkDeviceID newDeviceID = AK_INVALID_DEVICE_ID;

	// Find first available slot.
	for ( AkUInt32 uSlot = 0; uSlot < m_arDevices.Length(); ++uSlot )
	{
		if ( !m_arDevices[uSlot] )
		{
			newDeviceID = uSlot;
			break;
		}
	}

	if ( AK_INVALID_DEVICE_ID == newDeviceID )
	{
		// Create slot.
		if ( !m_arDevices.AddLast( NULL ) )
		{
			AKASSERT( !"Could not add new device to list" );
			return AK_INVALID_DEVICE_ID;
		}
		newDeviceID = (AkDeviceID)m_arDevices.Length() - 1;
		m_arDevices.Last() = NULL;
	}

    CAkDeviceBase * pNewDevice = NULL;
    AKRESULT eResult = AK_Fail;

    // Device factory.
    if ( in_settings.uSchedulerTypeFlags & AK_SCHEDULER_BLOCKING )
    {
        // AK_SCHEDULER_BLOCKING
        pNewDevice = AkNew( m_streamMgrPoolId, CAkDeviceBlocking( in_pLowLevelHook ) );
        if ( pNewDevice != NULL )
        {
            eResult = pNewDevice->Init( 
				in_settings,
				newDeviceID );
        }
        AKASSERT( eResult == AK_Success || !"Cannot initialize IO device" );
    }
    else if ( in_settings.uSchedulerTypeFlags & AK_SCHEDULER_DEFERRED_LINED_UP )
    {
        // AK_SCHEDULER_DEFERRED_LINED_UP.
        pNewDevice = AkNew( m_streamMgrPoolId, CAkDeviceDeferredLinedUp( in_pLowLevelHook ) );
        if ( pNewDevice != NULL )
        {
            eResult = pNewDevice->Init( 
				in_settings,
				newDeviceID );
        }
        AKASSERT( eResult == AK_Success || !"Cannot initialize IO device" );
    }
    else
    {
        AKASSERT( !"Invalid device type" );
        return AK_INVALID_DEVICE_ID;
    }

	if ( AK_Success == eResult )
		m_arDevices[newDeviceID] = pNewDevice;
	else
    {
		// Handle failure. At this point we have a valid device ID (index in array).
        if ( pNewDevice )
            pNewDevice->Destroy();
        return AK_INVALID_DEVICE_ID;
    }

    return newDeviceID;
}

// Warning: This function is not thread safe. No stream should exist for that device when it is destroyed.
AKRESULT   CAkStreamMgr::DestroyDevice(
    AkDeviceID          in_deviceID         // Device ID.
    )
{
    if ( (AkUInt32)in_deviceID >= m_arDevices.Length() 
		|| !m_arDevices[in_deviceID] )
    {
        return AK_InvalidParameter;
    }

    m_arDevices[in_deviceID]->Destroy();
	m_arDevices[in_deviceID] = NULL;

    return AK_Success;
}

// Global pool cleanup: dead streams.
// Since the StreamMgr's global pool is shared across all devices, they all need to perform
// dead handle clean up. The device that calls this method will also be asked to kill one of
// its tasks.
void CAkStreamMgr::ForceCleanup(
	CAkDeviceBase * in_pCallingDevice,		// Calling device: if specified, the task with the lowest priority for this device will be killed.
	AkPriority in_priority					// Priority of the new task if applicable. Pass AK_MAX_PRIORITY to ignore.
	)
{
	for ( AkUInt32 uDevice = 0; uDevice < m_arDevices.Length(); uDevice++ )
    {
	    m_arDevices[uDevice]->ForceCleanup( in_pCallingDevice == m_arDevices[uDevice], in_priority );
	}
}


// Stream creation interface.
// ------------------------------------------------------


// Standard stream open methods.
// -----------------------------

// String overload.
AKRESULT CAkStreamMgr::CreateStd(
    const AkOSChar*     in_pszFileName,     // Application defined string (title only, or full path, or code...).
    AkFileSystemFlags * in_pFSFlags,        // Special file system flags. Can pass NULL.
    AkOpenMode          in_eOpenMode,       // Open mode (read, write, ...).
    IAkStdStream *&     out_pStream,		// Returned interface to a standard stream.
	bool				in_bSyncOpen		// If true, force the Stream Manager to open file synchronously. Otherwise, it is left to its discretion.
    )
{
    // Check parameters.
    if ( in_pszFileName == NULL )
    {
        AKASSERT( !"Invalid file name" );
        return AK_InvalidParameter;
    }

	AKASSERT( m_pFileLocationResolver
			|| !"File location resolver was not set on the Stream Manager" );

	// Set AkFileSystemFlags::bIsAutomaticStream if flags are specified.
	if ( in_pFSFlags )
		in_pFSFlags->bIsAutomaticStream = false;

    AkFileDesc fileDesc;
	bool bSyncOpen = in_bSyncOpen;
    AKRESULT eRes = m_pFileLocationResolver->Open( 
		in_pszFileName,
		in_eOpenMode,
		in_pFSFlags,
		bSyncOpen,
		fileDesc );
    if ( eRes != AK_Success )
    {
#ifndef AK_OPTIMIZED
        // HACK: Hide monitoring errors for banks that are not found in bIsLanguageSpecific directory.
        if ( in_pFSFlags &&
             in_pFSFlags->uCompanyID == AKCOMPANYID_AUDIOKINETIC &&
             in_pFSFlags->uCodecID == AKCODECID_BANK && 
             in_pFSFlags->bIsLanguageSpecific )
             return eRes;

        // Monitor error.
		size_t uLen = sizeof( AkOSChar ) * ( AKPLATFORM::OsStrLen( in_pszFileName ) + 64 );
		AkOSChar * pMsg = (AkOSChar *) AkAlloca( uLen );
		OS_PRINTF( pMsg, ( AK_FileNotFound == eRes ) ? AKTEXT("File not found: %s") : AKTEXT("Cannot open file: %s"),  in_pszFileName );
		AK::Monitor::PostString( pMsg, AK::Monitor::ErrorLevel_Error );
#endif
        return eRes;
    }

	CAkDeviceBase * pDevice = GetDevice( fileDesc.deviceID );
    if ( !pDevice )
	{
		AKASSERT( !"File Location Resolver returned an invalid device ID" );
        return AK_Fail;
	}

	IAkStdStream * pStream = NULL;
	CAkStmTask * pTask = pDevice->CreateStd( 
		fileDesc,
        in_eOpenMode,
		pStream );

	if ( !pTask )
	{
		if ( bSyncOpen )
			pDevice->GetLowLevelHook()->Close( fileDesc );
		return AK_Fail;
	}
	
	if ( bSyncOpen )
	{
		pTask->SetFileOpen();
	}
	else
	{
		// Debug check sync flag.
		AKASSERT( !in_bSyncOpen || !"Cannot defer open when asked for synchronous" );

		// Low-level File Location Resolver wishes to open the file later. Create a
		// DeferredFileOpenRecord for it.
		if ( pTask->SetDeferredFileOpen( in_pszFileName, in_pFSFlags, in_eOpenMode ) != AK_Success )
		{
			// Could not set info for deferred open. Mark this task for clean up.
			pTask->Kill();
			return AK_Fail;
		}
	}

	out_pStream = pStream;
	return AK_Success;
}

// ID overload.
AKRESULT CAkStreamMgr::CreateStd(
    AkFileID            in_fileID,          // Application defined ID.
    AkFileSystemFlags * in_pFSFlags,        // Special file system flags. Can pass NULL.
    AkOpenMode          in_eOpenMode,       // Open mode (read, write, ...).
    IAkStdStream *&     out_pStream,		// Returned interface to a standard stream.
	bool				in_bSyncOpen		// If true, force the Stream Manager to open file synchronously. Otherwise, it is left to its discretion.
    )
{
	AKASSERT( m_pFileLocationResolver
			|| !"File location resolver was not set on the Stream Manager" );

	// Set AkFileSystemFlags::bIsAutomaticStream if flags are specified.
	if ( in_pFSFlags )
		in_pFSFlags->bIsAutomaticStream = false;

    AkFileDesc fileDesc;
	bool bSyncOpen = in_bSyncOpen;
    AKRESULT eRes = m_pFileLocationResolver->Open( 
		in_fileID,
		in_eOpenMode,
		in_pFSFlags,
		bSyncOpen,
		fileDesc );

    if ( eRes != AK_Success )
    {
#ifndef AK_OPTIMIZED
        // HACK: Hide monitoring errors for banks that are not found in bIsLanguageSpecific directory.
        if ( in_pFSFlags &&
             in_pFSFlags->uCompanyID == AKCOMPANYID_AUDIOKINETIC &&
             in_pFSFlags->uCodecID == AKCODECID_BANK && 
             in_pFSFlags->bIsLanguageSpecific )
             return eRes;

        // Monitor error.
		size_t uLen = 64;
		AkOSChar * pMsg = (AkOSChar *) AkAlloca( uLen );
		OS_PRINTF( pMsg, ( AK_FileNotFound == eRes ) ? AKTEXT("File not found: %u") : AKTEXT("Cannot open file: %u"),  in_fileID );
		AK::Monitor::PostString( pMsg, AK::Monitor::ErrorLevel_Error );
#endif
        return eRes;
    }

    CAkDeviceBase * pDevice = GetDevice( fileDesc.deviceID );
    if ( !pDevice )
	{
		AKASSERT( !"File Location Resolver returned an invalid device ID" );
        return AK_Fail;
	}

    IAkStdStream * pStream = NULL;
	CAkStmTask * pTask = pDevice->CreateStd( 
		fileDesc,
        in_eOpenMode,
		pStream );

	if ( !pTask )
	{
		if ( bSyncOpen )
			pDevice->GetLowLevelHook()->Close( fileDesc );
		return AK_Fail;
	}
	
	if ( bSyncOpen )
	{
		pTask->SetFileOpen();
	}
	else
	{
		// Debug check sync flag.
		AKASSERT( !in_bSyncOpen || !"Cannot defer open when asked for synchronous" );

		// Low-level File Location Resolver wishes to open the file later. Create a
		// DeferredFileOpenRecord for it.
		if ( pTask->SetDeferredFileOpen( in_fileID, in_pFSFlags, in_eOpenMode ) != AK_Success )
		{
			// Could not set info for deferred open. Mark this task for clean up.
			pTask->Kill();
			return AK_Fail;
		}
	}

	out_pStream = pStream;
	return AK_Success;
}


// Automatic stream open methods.
// ------------------------------

// String overload.
AKRESULT CAkStreamMgr::CreateAuto(
    const AkOSChar*             in_pszFileName,     // Application defined string (title only, or full path, or code...).
    AkFileSystemFlags *         in_pFSFlags,        // Special file system flags. Can pass NULL.
    const AkAutoStmHeuristics & in_heuristics,      // Streaming heuristics.
    AkAutoStmBufSettings *      in_pBufferSettings, // Stream buffer settings. Pass NULL to use defaults (recommended).
    IAkAutoStream *&            out_pStream,		// Returned interface to an automatic stream.
	bool						in_bSyncOpen		// If true, force the Stream Manager to open file synchronously. Otherwise, it is left to its discretion.
    )
{
    // Check parameters.
    if ( in_pszFileName == NULL )
    {
        AKASSERT( !"Invalid file name" );
        return AK_InvalidParameter;
    }
    if ( in_heuristics.fThroughput < 0 ||
         in_heuristics.priority < AK_MIN_PRIORITY ||
         in_heuristics.priority > AK_MAX_PRIORITY )
    {
        AKASSERT( !"Invalid automatic stream heuristic" );
        return AK_InvalidParameter;
    }

	AKASSERT( m_pFileLocationResolver
			|| !"File location resolver was not set on the Stream Manager" );

	// Set AkFileSystemFlags::bIsAutomaticStream if flags are specified.
	if ( in_pFSFlags )
		in_pFSFlags->bIsAutomaticStream = true;

    AkFileDesc fileDesc;
	bool bSyncOpen = in_bSyncOpen;
    AKRESULT eRes = m_pFileLocationResolver->Open( 
		in_pszFileName,
		AK_OpenModeRead,  // Always read from an autostream.
		in_pFSFlags,
		bSyncOpen,
		fileDesc );

    if ( eRes != AK_Success )
    {
#ifndef AK_OPTIMIZED
        // Monitor error.
		size_t uLen = sizeof( AkOSChar ) * ( AKPLATFORM::OsStrLen( in_pszFileName ) + 64 );
		AkOSChar * pMsg = (AkOSChar *) AkAlloca( uLen );
		OS_PRINTF( pMsg, ( AK_FileNotFound == eRes ) ? AKTEXT("File not found: %s") : AKTEXT("Cannot open file: %s"),  in_pszFileName );
		AK::Monitor::PostString( pMsg, AK::Monitor::ErrorLevel_Error );
#endif
        return eRes;
    }

    CAkDeviceBase * pDevice = GetDevice( fileDesc.deviceID );
    if ( !pDevice )
	{
		AKASSERT( !"File Location Resolver returned an invalid device ID" );
        return AK_Fail;
	}

    IAkAutoStream * pStream = NULL;
	CAkStmTask * pTask = pDevice->CreateAuto( 
		fileDesc,
        in_heuristics,
        in_pBufferSettings,
		pStream );

	if ( !pTask )
	{
		if ( bSyncOpen )
			pDevice->GetLowLevelHook()->Close( fileDesc );
		return AK_Fail;
	}
	
	if ( bSyncOpen )
	{
		pTask->SetFileOpen();
	}
	else
	{
		// Debug check sync flag.
		AKASSERT( !in_bSyncOpen || !"Cannot defer open when asked for synchronous" );

		// Low-level File Location Resolver wishes to open the file later. Create a
		// DeferredFileOpenRecord for it.
		if ( pTask->SetDeferredFileOpen( in_pszFileName, in_pFSFlags, AK_OpenModeRead ) != AK_Success )
		{
			// Could not set info for deferred open. Mark this task for clean up.
			pTask->Kill();
			return AK_Fail;
		}
	}

	out_pStream = pStream;
	return AK_Success;
}

// ID overload.
AKRESULT CAkStreamMgr::CreateAuto(
    AkFileID                    in_fileID,          // Application defined ID.
    AkFileSystemFlags *         in_pFSFlags,        // Special file system flags. Can pass NULL.
    const AkAutoStmHeuristics & in_heuristics,      // Streaming heuristics.
    AkAutoStmBufSettings *      in_pBufferSettings, // Stream buffer settings. Pass NULL to use defaults (recommended).
	IAkAutoStream *&            out_pStream,		// Returned interface to an automatic stream.
	bool						in_bSyncOpen		// If true, force the Stream Manager to open file synchronously. Otherwise, it is left to its discretion.
    )
{
    // Check parameters.
    if ( in_heuristics.fThroughput < 0 ||
         in_heuristics.priority < AK_MIN_PRIORITY ||
         in_heuristics.priority > AK_MAX_PRIORITY )
    {
        AKASSERT( !"Invalid automatic stream heuristic" );
        return AK_InvalidParameter;
    }

	AKASSERT( m_pFileLocationResolver
			|| !"File location resolver was not set on the Stream Manager" );

	// Set AkFileSystemFlags::bIsAutomaticStream if flags are specified.
	if ( in_pFSFlags )
		in_pFSFlags->bIsAutomaticStream = true;

    AkFileDesc fileDesc;
	bool bSyncOpen = in_bSyncOpen;
    AKRESULT eRes = m_pFileLocationResolver->Open( 
		in_fileID,
		AK_OpenModeRead,  // Always read from an autostream.
		in_pFSFlags,
		bSyncOpen,
		fileDesc );

    if ( eRes != AK_Success )
    {
#ifndef AK_OPTIMIZED
        // Monitor error.
		size_t uLen = 64;
		AkOSChar * pMsg = (AkOSChar *) AkAlloca( uLen );
		OS_PRINTF( pMsg, ( AK_FileNotFound == eRes ) ? AKTEXT("File not found: %u") : AKTEXT("Cannot open file: %u"),  in_fileID );
		AK::Monitor::PostString( pMsg, AK::Monitor::ErrorLevel_Error );
#endif
        return eRes;
    }

    CAkDeviceBase * pDevice = GetDevice( fileDesc.deviceID );
    if ( !pDevice )
	{
		AKASSERT( !"File Location Resolver returned an invalid device ID" );
        return AK_Fail;
	}

    IAkAutoStream * pStream = NULL;
	CAkStmTask * pTask = pDevice->CreateAuto( 
		fileDesc,
        in_heuristics,
        in_pBufferSettings,
		pStream );

	if ( !pTask )
	{
		if ( bSyncOpen )
			pDevice->GetLowLevelHook()->Close( fileDesc );
		return AK_Fail;
	}
	
	if ( bSyncOpen )
	{
		pTask->SetFileOpen();
	}
	else
	{
		// Debug check sync flag.
		AKASSERT( !in_bSyncOpen || !"Cannot defer open when asked for synchronous" );

		// Low-level File Location Resolver wishes to open the file later. Create a
		// DeferredFileOpenRecord for it.
		if ( pTask->SetDeferredFileOpen( in_fileID, in_pFSFlags, AK_OpenModeRead ) != AK_Success )
		{
			// Could not set info for deferred open. Mark this task for clean up.
			pTask->Kill();
			return AK_Fail;
		}
	}

	out_pStream = pStream;
	return AK_Success;
}

// Profiling interface.
// --------------------------------------------------------------------
IAkStreamMgrProfile * CAkStreamMgr::GetStreamMgrProfile()
{
#ifndef AK_OPTIMIZED
    return this;
#else
	return NULL;
#endif
}

#ifndef AK_OPTIMIZED
// Device enumeration.
AkUInt32 CAkStreamMgr::GetNumDevices()
{
	// Find real number of stream devices.
	AkUInt32 uNumDevices = 0;
	for ( AkUInt32 uSlot = 0; uSlot < m_arDevices.Length(); ++uSlot )
	{
		if ( m_arDevices[uSlot] )
			++uNumDevices;
	}

    return uNumDevices;
}

IAkDeviceProfile * CAkStreamMgr::GetDeviceProfile( 
    AkUInt32 in_uDeviceIndex    // [0,numStreams[
    )
{
    if ( in_uDeviceIndex >= m_arDevices.Length() )
    {
        AKASSERT( !"Invalid device index" );
        return NULL;
    }
    
	// Convert device index to device ID.
	for ( AkUInt32 uDeviceID = 0; uDeviceID < m_arDevices.Length(); ++uDeviceID )
	{
		if ( !m_arDevices[uDeviceID] )
			++in_uDeviceIndex;	// NULL. Skip.
		else if ( in_uDeviceIndex == uDeviceID )
			return m_arDevices[uDeviceID];
	}

	AKASSERT( !"Invalid device index" );
    return NULL;
}

AKRESULT CAkStreamMgr::StartMonitoring()
{
    for ( AkUInt32 u=0; u<m_arDevices.Length( ); u++ )
    {
		if ( m_arDevices[u] )
        	AKVERIFY( m_arDevices[u]->StartMonitoring( ) == AK_Success );
    }
    return AK_Success;
}

void CAkStreamMgr::StopMonitoring()
{
    for ( AkUInt32 u=0; u<m_arDevices.Length( ); u++ )
    {
        if ( m_arDevices[u] )
        	m_arDevices[u]->StopMonitoring( );
    }
}

#endif
