//////////////////////////////////////////////////////////////////////
//
// AkDeviceBlocking.h
//
// Win32 Blocking Scheduler Device implementation.
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkDeviceBlocking.h"
#include <AK/Tools/Common/AkAutoLock.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>
#include <AK/Tools/Common/AkMonitorError.h>
#include <AK/SoundEngine/Common/AkStreamMgrModule.h>
using namespace AK;
using namespace AK::StreamMgr;

CAkDeviceBlocking::CAkDeviceBlocking(
	IAkLowLevelIOHook *	in_pLowLevelHook
	)
: CAkDeviceBase( in_pLowLevelHook )
{
}

CAkDeviceBlocking::~CAkDeviceBlocking( )
{
}


// Stream creation interface.
// --------------------------------------------------------

// Standard stream.
CAkStmTask * CAkDeviceBlocking::CreateStd(
    AkFileDesc &		in_fileDesc,        // Application defined ID.
    AkOpenMode          in_eOpenMode,       // Open mode (read, write, ...).
    IAkStdStream *&     out_pStream         // Returned interface to a standard stream.    
    )
{
    out_pStream = NULL;
    
    AKRESULT eResult = AK_Fail;

	CAkStdStmBlocking * pNewStm = AkNew( CAkStreamMgr::GetObjPoolID(), CAkStdStmBlocking() );
    
    // If not enough memory to create stream, ask for cleanup and try one more time.
	if ( !pNewStm )
	{
		// Could be because there are dead streams lying around in a device. Force clean and try again.
		CAkStreamMgr::ForceCleanup( this, AK_MAX_PRIORITY );
		pNewStm = AkNew( CAkStreamMgr::GetObjPoolID(), CAkStdStmBlocking() );
	}

    if ( pNewStm != NULL )
    {
        eResult = pNewStm->Init( 
			this, 
			in_fileDesc, 
			in_eOpenMode );
    }
    else
	{
        eResult = AK_InsufficientMemory;
	}

    if ( AK_Success == eResult )
	{
		AddTask( pNewStm );
		out_pStream = pNewStm;
		return pNewStm;
	}
	else
    {
		// --------------------------------------------------------
		// Failed. Clean up.
		// --------------------------------------------------------
    	if ( pNewStm != NULL )
       		pNewStm->InstantDestroy();
    }
	return NULL;
}

// Automatic stream
CAkStmTask * CAkDeviceBlocking::CreateAuto(
    AkFileDesc &				in_fileDesc,        // Application defined ID.
    const AkAutoStmHeuristics & in_heuristics,      // Streaming heuristics.
    AkAutoStmBufSettings *      in_pBufferSettings, // Stream buffer settings. Pass NULL to use defaults (recommended).
    IAkAutoStream *&            out_pStream         // Returned interface to an automatic stream.
    )
{
    AKASSERT( in_heuristics.fThroughput >= 0 &&
              in_heuristics.priority >= AK_MIN_PRIORITY &&
              in_heuristics.priority <= AK_MAX_PRIORITY );

    out_pStream = NULL;
    
#ifndef AK_OPTIMIZED
	if ( m_streamIOPoolId == AK_INVALID_POOL_ID )
    {
		AKASSERT( !"Streaming pool does not exist: cannot create automatic stream" );
		AK_MONITOR_ERROR( AK::Monitor::ErrorCode_CannotStartStreamNoMemory );
        return NULL;
    }
#endif

	AKRESULT eResult;
    CAkAutoStmBlocking * pNewStm = AkNew( CAkStreamMgr::GetObjPoolID(), CAkAutoStmBlocking() );
	
	// If not enough memory to create stream, ask for cleanup and try one more time.
	if ( !pNewStm )
	{
		// Could be because there are dead streams lying around in a device. Force clean and try again.
		CAkStreamMgr::ForceCleanup( this, in_heuristics.priority );
		pNewStm = AkNew( CAkStreamMgr::GetObjPoolID(), CAkAutoStmBlocking() );
	}

	if ( pNewStm != NULL )
	{
		eResult = pNewStm->Init( 
			this,
			in_fileDesc,
			in_heuristics,
			in_pBufferSettings,
			m_uGranularity
			);
	}
	else
	{
		eResult = AK_InsufficientMemory;
	}
	
    if ( AK_Success == eResult )
    {
		AddTask( pNewStm );
		out_pStream = pNewStm;
		return pNewStm;
	}
	else
	{
		// --------------------------------------------------------
		// Failed. Clean up.
		// --------------------------------------------------------
        if ( pNewStm != NULL )
            pNewStm->InstantDestroy();
        out_pStream = NULL;
    }

    return NULL;
}

// Scheduler.

// Finds the next task to be executed,
// posts the request to Low-Level IO and blocks until it is completed,
// updates the task.
void CAkDeviceBlocking::PerformIO()
{
    void * pBuffer;
	AkReal32 fOpDeadline;

    CAkStmTask * pTask = SchedulerFindNextTask( pBuffer, fOpDeadline );

    if ( pTask )
    {
        AKASSERT( pBuffer );    // If scheduler chose a task, it must have provided a valid buffer.

        // Execute.
        ExecuteTask( pTask,
                     pBuffer,
					 fOpDeadline );
    }
}

// Execute task that was chosen by scheduler.
void CAkDeviceBlocking::ExecuteTask( 
    CAkStmTask *	in_pTask,
    void *			in_pBuffer,
	AkReal32		in_fOpDeadline
    )
{
    AKASSERT( in_pTask != NULL );

	// Handle deferred opening.
	AKRESULT eResult = in_pTask->EnsureFileIsOpen();
	if ( eResult != AK_Success )
	{
		// Deferred open failed. Updade/Kill this task and bail out.
		in_pTask->Update( 
			NULL,	// Cookie is NULL: the transfer was not prepared.
			0,
			in_pBuffer,  
			0,
			AK_Fail );
		return;
	}

    // Get info for IO.
    AkFileDesc * pFileDesc;
    AkIOTransferInfo info;
	
    if ( !in_pTask->TransferInfo( 
			pFileDesc,				// Stream's associated file descriptor.
			info.uFilePosition,	
			info.uBufferSize,
			info.uRequestedSize ) )
	{
		// Transfer was cancelled at the last minute (for e.g. the client Destroy()ed the stream.
		// Update as "cancelled" and bail out.
		in_pTask->Update( 
			NULL,	// Cookie is NULL: the transfer was not prepared.
			0, 
			in_pBuffer,
			0,
			AK_Cancelled );
		return;
	}
    AKASSERT( info.uRequestedSize > 0 &&
    		  info.uRequestedSize <= m_uGranularity );
	info.uSizeTransferred = 0;

	// Store current transfer position for conditional cancels.
	m_uCurTransferPosition = info.uFilePosition;

	AkIoHeuristics heuristics;
	heuristics.priority = in_pTask->Priority();
	heuristics.fDeadline = in_fOpDeadline;

    // Read or write?
    if ( in_pTask->IsWriteOp( ) )
    {
        // Write.
        eResult = static_cast<IAkIOHookBlocking*>( m_pLowLevelHook )->Write( 
            *pFileDesc,
			heuristics,
            in_pBuffer,
            info );
    }
    else
    {
        // Read.
        eResult = static_cast<IAkIOHookBlocking*>( m_pLowLevelHook )->Read( 
            *pFileDesc,
			heuristics,
            in_pBuffer,
            info );
    }

    // Monitor errors.
#ifndef AK_OPTIMIZED
    if ( eResult != AK_Success )
		AK_MONITOR_ERROR( AK::Monitor::ErrorCode_IODevice );
#endif

    // Update: Locks object, increments size and sets status, unlocks, calls back if needed.
    // According to actual size, success flag and EOF, status could be set to completed, pending or error.
    // Release IO buffer.
	// Use a non-NULL cookie to indicate that a transfer has been prepared.
    in_pTask->Update( 
		(void*)1,
		info.uFilePosition,
        in_pBuffer,  
        info.uSizeTransferred,
        eResult );

}

//-----------------------------------------------------------------------------
// Name: class CAkStdStmBlocking
// Desc: Standard stream implementation.
//-----------------------------------------------------------------------------
CAkStdStmBlocking::CAkStdStmBlocking()
: m_bTransferInProgress( false )
{
}

CAkStdStmBlocking::~CAkStdStmBlocking()
{
}

// Destruction. The object is destroyed and the interface becomes invalid.
// Sync: 
// Status lock. Released if we need to wait for transfers to complete.
void CAkStdStmBlocking::Destroy()
{
    // If an operation is pending, the scheduler might be executing it. This method must not return until it 
    // is complete: lock I/O for this task.
	m_lockStatus.Lock();

    // Allow destruction.
	SetToBeDestroyed();

	if ( m_bTransferInProgress )
	{
		// Stop asking to be scheduled.
		SetStatus( AK_StmStatusCancelled );

		// Wait for current transfer completion.
		SetBlockedStatus();
		m_lockStatus.Unlock();
		m_pDevice->WaitForIOCompletion( this );
	}
	else
	{
		// Stop asking to be scheduled.
		SetStatus( AK_StmStatusCancelled );
		m_lockStatus.Unlock();
	}
}

// Cancel. If Pending, sets its status to Cancelled. Otherwise it returns right away.
// Sync: Status lock.
void CAkStdStmBlocking::Cancel()
{
	m_lockStatus.Lock();

	if ( m_bTransferInProgress )
    {
		// Stop asking for IO.
		SetStatus( AK_StmStatusCancelled );

		// Wait for current transfer completion.
		SetBlockedStatus();
		m_lockStatus.Unlock();
		m_pDevice->WaitForIOCompletion( this );
	}
	else
	{
		// Set status. Semaphore will be released.
		SetStatus( AK_StmStatusCancelled );
		m_lockStatus.Unlock();
	}
}

// Transfer info. Returns everything that is needed by the scheduler to perform a data transfer with the
// Low-Level IO. Must be called if and only if this task was chosen.
// Sync: Locks stream's status.
bool CAkStdStmBlocking::TransferInfo( 
    AkFileDesc *& out_pFileDesc,    // Stream's associated file descriptor.
    AkUInt64 &  out_uPosition,		// Position in file (absolute).
    AkUInt32 &  out_uBufferSize,	// Buffer size available for transfer.
	AkUInt32 &  out_uRequestSize	// Exact transfer size.
	)
{
    out_pFileDesc = &m_fileDesc;

    // Lock status.
    AkAutoLock<CAkLock> atomicPosition( m_lockStatus );

	// Status is locked: last chance to bail out if the stream was destroyed by client.
	if ( m_bIsToBeDestroyed || !ReadyForIO() )
		return false;

	// Required transfer size is the buffer size for this stream.
    // Slice request to granularity.
	AKASSERT( m_uBufferSize >= m_uActualSize );
	out_uBufferSize = ( m_uBufferSize - m_uActualSize );
	if ( out_uBufferSize > m_pDevice->GetGranularity() )
        out_uBufferSize = m_pDevice->GetGranularity();

	// If position is dirty, set file position to client position.
    if ( m_bIsPositionDirty )
    {
        m_uFilePosition = GetFileOffset() + m_uCurPosition;
        // Reset position dirty flag.
    	m_bIsPositionDirty = false;
    }

	out_uPosition = m_uFilePosition;

	// Requested size and buffer size is always identical with standard streams, except at the end of file.
	if ( !m_bIsWriteOp )
	{
		AKASSERT( GetFileEndPosition() > out_uPosition ); // Cannot read after the end of file. Would have been caught in CAkStdStmBase::ExecuteOp().
		AkUInt32 uMaxReadSize = (AkUInt32)( GetFileEndPosition() - out_uPosition );
		out_uRequestSize = AkMin( out_uBufferSize, uMaxReadSize );
	}
	else
		out_uRequestSize = out_uBufferSize;

	m_bTransferInProgress = true;
	return true;
}

// Update stream object after I/O.
void CAkStdStmBlocking::Update(
	void *		in_pCookie,			// Cookie for device specific usage: With blocking device, it NULL if the transfer was not prepared, non-NULL otherwise.
	const AkUInt64 in_uPosition,	// Absolute file position of transfer.
    void *      in_pBuffer,			// Address of data.
	AkUInt32    in_uActualIOSize,	// Size available for writing/reading.
	AKRESULT	in_eIOResult		// AK_Success if IO was successful, AK_Cancelled if IO was cancelled, AK_Fail otherwise.
    )
{
	// Lock status.
    AkAutoLock<CAkLock> update( m_lockStatus );
	
	UpdatePosition( in_uPosition, in_pBuffer, in_uActualIOSize, ( AK_Success == in_eIOResult ) );

	UpdateTaskStatus( in_eIOResult );

	m_bTransferInProgress = false;
}

// Try get I/O buffer.
// Returns the address supplied by the user, into which the data transfer should be performed.
void * CAkStdStmBlocking::TryGetIOBuffer()
{
    AKASSERT( m_pBuffer );
    return (AkUInt8*)m_pBuffer + m_uActualSize;
}

// Compute task's deadline for next operation.
// Sync: None: if m_uActualSize is changed in the meantime, the scheduler could take a suboptimal decision.
AkReal32 CAkStdStmBlocking::EffectiveDeadline()
{
	AkUInt32 uGranularity = m_pDevice->GetGranularity();
	AkUInt32 uNumTransfersRemaining = ( m_uBufferSize - m_uActualSize + uGranularity - 1 ) / uGranularity;
	AKASSERT( uNumTransfersRemaining > 0 );
	AkReal32 fDeadline = ( m_fDeadline / uNumTransfersRemaining ) - AKPLATFORM::Elapsed( m_pDevice->GetTime( ), m_iIOStartTime );
    return ( fDeadline > 0 ? fDeadline : 0 );
}


//-----------------------------------------------------------------------------
// Name: class CAkAutoStmBlocking
// Desc: Automatic stream implementation.
//-----------------------------------------------------------------------------
CAkAutoStmBlocking::CAkAutoStmBlocking()
: m_bTransferInProgress( false )
{
#ifdef _DEBUG
	m_uExpectedTransferSize = 0;
#endif
}

CAkAutoStmBlocking::~CAkAutoStmBlocking()
{
}

AKRESULT CAkAutoStmBlocking::Init( 
	CAkDeviceBase *             in_pDevice,         // Owner device.
	const AkFileDesc &          in_fileDesc,        // File descriptor.
	const AkAutoStmHeuristics & in_heuristics,      // Streaming heuristics.
	AkAutoStmBufSettings *      in_pBufferSettings, // Stream buffer settings.
	AkUInt32                    in_uGranularity     // Device's I/O granularity.
	)
{
	// NOTE: This is initialized to 0. If file doesn't start at 0, then the "sequential" flag will be false
	// for first transfer.
	m_uExpectedFilePosition = 0;
	m_bHasTransferPending = false;
	return CAkAutoStmBase::Init( in_pDevice, in_fileDesc, in_heuristics, in_pBufferSettings, in_uGranularity );
}

bool CAkAutoStmBlocking::TransferInfo( 
    AkFileDesc *& out_pFileDesc,    // Stream's associated file descriptor.
    AkUInt64 &  out_uPosition,		// Position in file (absolute).
	AkUInt32 &  out_uBufferSize,	// Buffer size available for transfer.
	AkUInt32 &  out_uRequestSize	// Exact transfer size.
    )
{
    out_pFileDesc = &m_fileDesc;

    // Lock status.
    AkAutoLock<CAkLock> atomicPosition( m_lockStatus );

	// Status is locked: last chance to bail out if the stream was destroyed by client.
	if ( m_bIsToBeDestroyed || !ReadyForIO() )
		return false;

	// Required transfer size is the buffer size for this stream.
    // Slice request to granularity.
	if ( m_uBufferSize > m_pDevice->GetGranularity( ) )
        out_uBufferSize = m_pDevice->GetGranularity( );
    else
        out_uBufferSize = m_uBufferSize;

	
	// Compute (absolute) file position for transfer.
	
	// Get position of last transfer request that was sent to Low-Level IO.
	out_uPosition = GetVirtualFilePosition();	
	
	// Handle loop buffering.
	if ( m_uLoopEnd 
		&& out_uPosition >= m_uLoopEnd + GetFileOffset() )
	{
		// Read at the beginning of the loop region. Snap to Low-Level block size.
		out_uPosition = m_uLoopStart + GetFileOffset();
	}

	// OPTIM move m_uExpectedFilePosition to (blocking) device.

	// "Enqueue" new transfer request.
	out_uRequestSize = PushTransferRequest( out_uPosition, out_uBufferSize );
	AKASSERT( out_uRequestSize > 0 );
#ifdef _DEBUG
	m_uExpectedTransferSize = out_uRequestSize;
#endif

	// Update status.
	m_uVirtualBufferingSize += out_uRequestSize;
	UpdateSchedulingStatus();

	// Reset timer. Time count since last transfer starts now.
    m_iIOStartTime = m_pDevice->GetTime();

	m_bTransferInProgress = true;

	return true;
}

// Update stream object after I/O.
void CAkAutoStmBlocking::Update(
	void *		in_pCookie,			// Cookie for device specific usage: With blocking device, it NULL if the transfer was not prepared, non-NULL otherwise.
	const AkUInt64 in_uPosition,	// Absolute file position of transfer.
    void *      in_pBuffer,			// Address of data.
	AkUInt32    in_uActualIOSize,	// Size available for writing/reading.
	AKRESULT	in_eIOResult		// AK_Success if IO was successful, AK_Cancelled if IO was cancelled, AK_Fail otherwise.
    )
{
	// Lock status.
    AkAutoLock<CAkLock> update( m_lockStatus );

	bool bStoreData = ( AK_Success == in_eIOResult 
						&& m_bHasTransferPending
						&& ( in_uPosition + in_uActualIOSize ) >= m_uExpectedFilePosition );

	AkUInt32 uPositionOffset = UpdatePosition( in_uPosition, in_pBuffer, in_uActualIOSize, bStoreData );
	
	m_bHasTransferPending = false;
#ifdef _DEBUG
	m_uExpectedTransferSize = 0;
#endif
	
	// Correct virtual buffering size.
	const AkUInt64 uPositionAfterRequest = in_uPosition + uPositionOffset;
	
	// A transfer was prepared if in_pCookie != NULL. Notify task that it is now removed.
	if ( in_pCookie )	
		OnTransferRemoved( m_uExpectedFilePosition, uPositionAfterRequest );

	UpdateTaskStatus( in_eIOResult );

	m_bTransferInProgress = false;
}

// Try to remove a buffer from this stream's list of buffers. Fails if user owns them all.
// Sync: Status.
void * CAkAutoStmBlocking::PopIOBuffer( )
{
    // Lock status.
    AkAutoLock<CAkLock> stmBufferGate( m_lockStatus );

	// Lock scheduler for memory access.
	// Important: to avoid any deadlock, we need to ensure that the scheduler is 
	// unlocked before this stream's status.
    m_pDevice->Lock();

    // Now that memory is locked, try to get a buffer again.
    void * pBuffer = AK::MemoryMgr::GetBlock( m_pDevice->GetIOPoolID() );

    if ( pBuffer )
    {
        // A buffer was freed by the time we acquired the memory lock. Leave.
		m_pDevice->Unlock();
        return pBuffer;
    }

	// In a blocking device, there cannot be a transfer pending for this task while the 
	// scheduler is trying to pop a buffer.
	AKASSERT( !m_bTransferInProgress );

    // Memory is still full. Try to remove a buffer from this task.
	// But do not remove if
	// 1) All buffers are already granted to client.
	// 2) Removing a buffer would take this stream under target buffering.

	if ( m_listBuffers.IsEmpty() )
	{
		// This task cannot let a buffer go. Notify scheduler that memory is blocked for a while.
		m_pDevice->NotifyMemIdle();
		m_pDevice->Unlock();
		return NULL;
	}

	AkUInt32 uSizeToRemove = m_listBuffers.Last()->uDataSize;
    if ( m_uVirtualBufferingSize >= uSizeToRemove
		&& !NeedsBuffering( m_uVirtualBufferingSize - uSizeToRemove ) )
    {
		// In a blocking device, there cannot be a transfer pending for this task while the 
		// scheduler is trying to pop a buffer.
		//AKASSERT( !CancelLastTransfer() );
		
		// Remove a buffer from the buffers list.
		m_uVirtualBufferingSize -= uSizeToRemove;

		AkStmBuffer * pLastStmBuffer = m_listBuffers.Last();
        pBuffer = pLastStmBuffer->pBuffer;
        AKASSERT( pBuffer );

        m_listBuffers.Remove( pLastStmBuffer );	// note: requires search.
		m_pDevice->ReleaseCachedBufferHolder( pLastStmBuffer );

		// Update status: Correct position. Reset EOF flag and restart if necessary.
		UpdateSchedulingStatus();
    }
    else
    {
        // This task cannot let a buffer go. Notify scheduler that memory is blocked for a while.
        AKASSERT( !pBuffer );
        m_pDevice->NotifyMemIdle( );
    }

    m_pDevice->Unlock();
    return pBuffer;
}

AkUInt64 CAkAutoStmBlocking::GetVirtualFilePosition()
{
	// Must be locked.

	if ( m_bHasTransferPending )
		return m_uExpectedFilePosition;
	else if ( m_listBuffers.Length() > m_uNextToGrant )
		return ( m_listBuffers.Last()->uPosition + m_listBuffers.Last()->uDataSize );
	else
		return m_uNextExpectedUserPosition;	
}

// Returns the expected transfer size (that is, takes into account the end of file not being aligned with the
// required transfer size.
AkUInt32 CAkAutoStmBlocking::PushTransferRequest(
	const AkUInt64		in_uPosition,		// Position in file (absolute).
	AkUInt32			in_uSize			// Required transfer size.
	)
{
	AKASSERT( in_uPosition < (AkUInt64)( m_fileDesc.iFileSize + m_fileDesc.uSector*m_uLLBlockSize ) 
			|| !"Preparing a 0 byte transfer is illegal" );

	AkUInt32 uExpectedTransferSize;
	AkUInt64 uPositionOfEof = m_fileDesc.iFileSize + m_fileDesc.uSector*m_uLLBlockSize;

	if ( ( in_uPosition + in_uSize ) <= uPositionOfEof )
	{
		m_uExpectedFilePosition = in_uPosition + in_uSize;
		uExpectedTransferSize = in_uSize;
	}
	else
	{
		m_uExpectedFilePosition = uPositionOfEof;
		uExpectedTransferSize = (AkUInt32)( uPositionOfEof - in_uPosition );	// Truncated at EOF.
	}
	m_bHasTransferPending = true;
	
	return uExpectedTransferSize;
}

// Cancel pending transfer.
// Skip if transfer's position is smaller than in_uMaxKeepPosition.
void CAkAutoStmBlocking::CancelPendingTransfers(
	const AkUInt64 in_uMaxKeepPosition	// Request position under which requests should not be flushed.
	)
{
	if ( m_bHasTransferPending )
	{
		if ( ((CAkDeviceBlocking*)(m_pDevice))->CurTransferPosition() >= in_uMaxKeepPosition 
			|| m_uExpectedFilePosition < in_uMaxKeepPosition )
		{
			m_bHasTransferPending = false;
		}
		// otherwise keep it.
	}
	// There is no Cancel counterpart for blocking reads on IAkLowLevelIO.
}

// Returns true if and only if there was a transfer pending that could be cancelled.
bool CAkAutoStmBlocking::CancelLastTransfer()
{
	if ( m_bHasTransferPending )
	{
		// There is no Cancel counterpart for blocking reads on IAkLowLevelIO.
		m_bHasTransferPending = false;
		return true;
	}
	return false;
}

#ifdef _DEBUG
void CAkAutoStmBlocking::CheckVirtualBufferingConsistency()
{
	AkUInt32 uVirtualBuffering = 0;
	AkUInt32 uNumGranted = m_uNextToGrant;
	
	AkBufferList::Iterator it = m_listBuffers.Begin();
	while ( it != m_listBuffers.End() )
	{
		if ( uNumGranted == 0 ) // Skip buffers granted.
			uVirtualBuffering += (*it)->uDataSize;
		else 
			uNumGranted--;
		
		++it;
	}
	uVirtualBuffering += m_uExpectedTransferSize;
	AKASSERT( uVirtualBuffering == m_uVirtualBufferingSize );
}
#endif
