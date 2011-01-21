//////////////////////////////////////////////////////////////////////
//
// AkDeviceDeferredLinedUp.h
//
// Win32 Deferred Scheduler Device implementation.
// Requests to low-level are sent in a lined-up fashion.
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkDeviceDeferredLinedUp.h"
#include <AK/Tools/Common/AkAutoLock.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>
#include <AK/SoundEngine/Common/AkStreamMgrModule.h>

using namespace AK;
using namespace AK::StreamMgr;

//--------------------------------------------------------------------
// Defines.
//--------------------------------------------------------------------

CAkDeviceDeferredLinedUp::CAkDeviceDeferredLinedUp(
	IAkLowLevelIOHook *	in_pLowLevelHook
	)
: CAkDeviceDeferredLinedUpBase( in_pLowLevelHook )
, m_pXferObjMem( NULL )
{
}

CAkDeviceDeferredLinedUp::~CAkDeviceDeferredLinedUp( )
{
}

AKRESULT CAkDeviceDeferredLinedUp::Init( 
	const AkDeviceSettings &	in_settings,
	AkDeviceID					in_deviceID 
	)
{
	m_listFreeTransferObjs.Init();
	if ( 0 == in_settings.uMaxConcurrentIO )
	{
		AKASSERT( !"Invalid number of concurrent IO tranfers" );
		return AK_InvalidParameter;
	}

	AKRESULT eResult = CAkDeviceBase::Init( in_settings, in_deviceID );
	if ( AK_Success == eResult )
	{
		// Cache all transfer objects needed.

		m_pXferObjMem = (CAkPendingTransfer*)AkAlloc( CAkStreamMgr::GetObjPoolID(), in_settings.uMaxConcurrentIO * sizeof( CAkPendingTransfer ) );
		if ( !m_pXferObjMem )
			return AK_Fail;

		CAkPendingTransfer * pXferObj = m_pXferObjMem;
		CAkPendingTransfer * pXferObjEnd = pXferObj + in_settings.uMaxConcurrentIO;
		do
		{
			m_listFreeTransferObjs.AddFirst( pXferObj++ );
		}
		while ( pXferObj < pXferObjEnd );
	}
	return AK_Success;
}

void CAkDeviceDeferredLinedUp::Destroy()
{
	CAkIOThread::Term();

	if ( m_pXferObjMem )
	{
		m_listFreeTransferObjs.RemoveAll();
		AkFree( CAkStreamMgr::GetObjPoolID(), m_pXferObjMem );
	}
	m_listFreeTransferObjs.Term();
	CAkDeviceBase::Destroy();
}

// Stream creation interface,
// because we need to initialize specialized stream objects.
// ---------------------------------------------------------------
// Standard stream.
CAkStmTask * CAkDeviceDeferredLinedUp::CreateStd(
    AkFileDesc &		in_fileDesc,        // Application defined ID.
    AkOpenMode          in_eOpenMode,       // Open mode (read, write, ...).
    IAkStdStream *&     out_pStream         // Returned interface to a standard stream.    
    )
{
    out_pStream = NULL;
    
    AKRESULT eResult = AK_Fail;

    CAkStdStmDeferredLinedUp * pNewStm = AkNew( CAkStreamMgr::GetObjPoolID(), CAkStdStmDeferredLinedUp() );
	
	// If not enough memory to create stream, ask for cleanup and try one more time.
	if ( !pNewStm )
	{
		// Could be because there are dead streams lying around in a device. Force clean and try again.
		CAkStreamMgr::ForceCleanup( this, AK_MAX_PRIORITY );
		pNewStm = AkNew( CAkStreamMgr::GetObjPoolID(), CAkStdStmDeferredLinedUp() );
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
CAkStmTask * CAkDeviceDeferredLinedUp::CreateAuto(
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

    // Instantiate new stream object.
    AKRESULT eResult;
	CAkAutoStmDeferredLinedUp * pNewStm = AkNew( CAkStreamMgr::GetObjPoolID(), CAkAutoStmDeferredLinedUp() );
	
	// If not enough memory to create stream, ask for cleanup and try one more time.
	if ( !pNewStm )
	{
		// Could be because there are dead streams lying around in a device. Force clean and try again.
		CAkStreamMgr::ForceCleanup( this, in_heuristics.priority );
		pNewStm = AkNew( CAkStreamMgr::GetObjPoolID(), CAkAutoStmDeferredLinedUp() );
	}

	if ( pNewStm != NULL )
	{
		eResult = pNewStm->Init( 
			this,
			in_fileDesc,
			in_heuristics,
			in_pBufferSettings,
			m_uGranularity );                                
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

// This device's implementation of PerformIO(), called by the I/O thread.
void CAkDeviceDeferredLinedUp::PerformIO( )
{
    void * pBuffer;
	AkReal32 fOpDeadline;
    CAkStmTask * pTask = SchedulerFindNextTask( pBuffer, fOpDeadline );

    if ( pTask )
    {
        AKASSERT( pBuffer );    // If scheduler chose a task, it must have provided a valid buffer.
        
        // Post task to Low-Level IO.
        ExecuteTask( pTask, 
                     pBuffer,
					 fOpDeadline );
    }
}

// Execute task chosen by scheduler.
void CAkDeviceDeferredLinedUp::ExecuteTask( 
    CAkStmTask *	in_pTask,
    void *			in_pBuffer,
	AkReal32		in_fOpDeadline
    )
{
	AKASSERT( in_pTask != NULL );

	IncrementIOCount();

	// Handle deferred opening.
	AKRESULT eResult = in_pTask->EnsureFileIsOpen();
	if ( eResult != AK_Success )
	{
		// Deferred open failed. Updade/Kill this task and bail out.
		in_pTask->Update( 
			NULL,
			0,
			in_pBuffer,  
			0,
			AK_Fail );
		return;
	}
    
    // Get info for IO.
    AkFileDesc * pFileDesc;
	AkAsyncIOTransferInfo * pInfo = in_pTask->TransferInfo( in_pBuffer, pFileDesc );
	if ( !pInfo )
	{
		// Not enough small object memory to enqueue a transfer in the LowLevelIO! Update and bail out.
		// Cancel this request.
		in_pTask->Update( 
			NULL,
			0, 
			in_pBuffer,
			0,
			AK_Cancelled );

		// Force clean up. 
		// NOTE: The thread might run without rest to perform another I/O, but they will all fail. 
		// On the other hand, we do not want to kill the task that has the lowest priority, because
		// before memory is actually freed, this thread will likely have killed all its tasks...
		CAkStreamMgr::ForceCleanup( NULL, AK_MAX_PRIORITY );
		return;
	}

	AkIoHeuristics heuristics;
	heuristics.priority = in_pTask->Priority();
	heuristics.fDeadline = in_fOpDeadline;
    
    // Read or write?
    if ( in_pTask->IsWriteOp( ) )
    {
        // Write.
        eResult = static_cast<IAkIOHookDeferred*>( m_pLowLevelHook )->Write( 
            *pFileDesc, 
			heuristics, 
			*pInfo );
    }
    else
    {
        // Read.
        eResult = static_cast<IAkIOHookDeferred*>( m_pLowLevelHook )->Read( 
			*pFileDesc, 
			heuristics, 
			*pInfo );
    }

	if ( eResult != AK_Success )
	{
        // Error in Read() (cannot be a cancellation). Update task now.
		AK_MONITOR_ERROR( AK::Monitor::ErrorCode_IODevice );

		CAkPendingTransfer * pTransfer = (CAkPendingTransfer*)(pInfo->pCookie);
	    in_pTask->Update( 
			pTransfer,
			pTransfer->StartPosition(), 
	        in_pBuffer,
			0,
			eResult );
    }
}

//-----------------------------------------------------------------------------
// Stream objects specificity.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Name: class CAkStdStmDeferredLinedUp
// Desc: Overrides methods for deferred lined-up device specificities.
//-----------------------------------------------------------------------------
CAkStdStmDeferredLinedUp::CAkStdStmDeferredLinedUp()
{
}

CAkStdStmDeferredLinedUp::~CAkStdStmDeferredLinedUp()
{
}

AKRESULT CAkStdStmDeferredLinedUp::Init(
    CAkDeviceBase *     in_pDevice,         // Owner device.
    const AkFileDesc &  in_fileDesc,        // File descriptor.
    AkOpenMode          in_eOpenMode        // Open mode.
    )
{
	// Cannot fail.
	m_listPendingXfers.Init();
	m_listCancelledXfers.Init();

	return CAkStdStmBase::Init(
		in_pDevice,
        in_fileDesc,
        in_eOpenMode );
}

AKRESULT CAkStdStmDeferredLinedUp::Read(
    void *          in_pBuffer,         // User buffer address. 
    AkUInt32        in_uReqSize,        // Requested read size.
    bool            in_bWait,           // Block until operation is complete.
    AkPriority      in_priority,        // Heuristic: operation priority.
    AkReal32        in_fDeadline,       // Heuristic: operation deadline (s).
    AkUInt32 &      out_uSize           // Size actually read.
    )
{
	m_uCumulTransferSize = 0;
	return CAkStdStmBase::Read(
		in_pBuffer,         // User buffer address. 
		in_uReqSize,        // Requested read size.
		in_bWait,           // Block until operation is complete.
		in_priority,        // Heuristic: operation priority.
		in_fDeadline,       // Heuristic: operation deadline (s).
		out_uSize           // Size actually read.
		);
}
AKRESULT CAkStdStmDeferredLinedUp::Write(
    void *          in_pBuffer,         // User buffer address. 
    AkUInt32        in_uReqSize,        // Requested read size.
    bool            in_bWait,           // Block until operation is complete.
    AkPriority      in_priority,        // Heuristic: operation priority.
    AkReal32        in_fDeadline,       // Heuristic: operation deadline (s).
    AkUInt32 &      out_uSize           // Size actually read.
    )
{
	m_uCumulTransferSize = 0;
	return CAkStdStmBase::Write(
		in_pBuffer,         // User buffer address. 
		in_uReqSize,        // Requested read size.
		in_bWait,           // Block until operation is complete.
		in_priority,        // Heuristic: operation priority.
		in_fDeadline,       // Heuristic: operation deadline (s).
		out_uSize           // Size actually read.
		);
}

void CAkStdStmDeferredLinedUp::Cancel()
{
	m_lockStatus.Lock();

	// Note: rely on the pending transfers list instead of the status ("idle" is used to 
	// avoid waking up the scheduler; it does not mean that there is no transfer in progress).
	if ( !m_listPendingXfers.IsEmpty()
		|| !m_listCancelledXfers.IsEmpty() )
	{
		// Stop asking for IO.
		SetStatus( AK_StmStatusCancelled );
		SetBlockedStatus();
		_CancelPendingTransfers();
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


// Info access.
// Sync: None. Status query.
// Override GetStatus(): return "pending" if we are in fact Idle, but transfers are pending.
AkStmStatus CAkStdStmDeferredLinedUp::GetStatus()           // Get operation status.
{
	AkAutoLock<CAkLock> status( m_lockStatus );
	if ( !m_listPendingXfers.IsEmpty() )
		return AK_StmStatusPending;
    return m_eStmStatus;
}

// Override destroy:
// - Cancel pending transfers
// - Lock all operations with scheduler and status locks, in the correct order.
void CAkStdStmDeferredLinedUp::Destroy()
{
	// If an operation is pending, the scheduler might be executing it. This method must not return until it 
    // is complete: lock I/O for this task.
	m_lockStatus.Lock();

	SetToBeDestroyed();

	// Stop asking to be scheduled.
	SetStatus( AK_StmStatusCancelled );

	// Note: rely on the pending transfers list instead of the status ("cancelled" is used to 
	// avoid waking up the scheduler; it does not mean that there is no transfer in progress).
	if ( !m_listPendingXfers.IsEmpty()
		|| !m_listCancelledXfers.IsEmpty() )
	{
		// Some tasks are still waiting to be Updated. Wait for them.
		SetBlockedStatus();
		_CancelPendingTransfers();

		m_lockStatus.Unlock();

		m_pDevice->WaitForIOCompletion( this );
    }
	else
		m_lockStatus.Unlock();
}

// Asynchronous counterpart for async devices.
AkAsyncIOTransferInfo * CAkStdStmDeferredLinedUp::TransferInfo(
	void *				in_pBuffer,			// Buffer for transfer.
	AkFileDesc *&		out_pFileDesc		// Stream's associated file descriptor.
	)
{
	out_pFileDesc = &m_fileDesc;

    // Lock status.
    AkAutoLock<CAkLock> atomicPosition( m_lockStatus );

	// Status is locked: last chance to bail out if the stream was destroyed by client.
	if ( m_bIsToBeDestroyed || !ReadyForIO() )
		return NULL;

	// If position is dirty, set file position to client position.
    if ( m_bIsPositionDirty )
    {
        m_uFilePosition = GetFileOffset() + m_uCurPosition;
        // Reset position dirty flag.
    	m_bIsPositionDirty = false;
    }

	AkUInt64 uPosition = GetFileOffset() + m_uCurPosition + m_uCumulTransferSize;

	// Required transfer size is the buffer size for this stream.
    // Slice request to granularity.
	// Cannot overshoot client buffer.
	// NOTE: m_uBufferSize is the original client request size.
	AKASSERT( m_uBufferSize >= m_uCumulTransferSize );
	AkUInt32 uMaxSize = m_uBufferSize - m_uCumulTransferSize;

	bool bLastTransfer = uMaxSize <= m_pDevice->GetGranularity();
	AkUInt32 uSize = ( bLastTransfer ) ? uMaxSize : m_pDevice->GetGranularity();

    AkUInt32 uExpectedTransferSize;
	bool bWillReachEof;
	AkAsyncIOTransferInfo * pXferInfo = PushTransferRequest(
		in_pBuffer,			// Buffer for transfer.
		uPosition,			// Position in file (absolute).
		uSize,				// Required transfer size.
		m_bIsWriteOp,		// Operation type.
		uExpectedTransferSize,
		bWillReachEof );
	AKASSERT( uExpectedTransferSize > 0 );

	if ( pXferInfo )
    {
		// Check if we client request will complete after this transfer.
		m_uCumulTransferSize += uExpectedTransferSize;
		if ( bWillReachEof || bLastTransfer  )
		{
			// Yes. Set as "idle", in order to stop this stream from being scheduled for I/O.
			SetStatus( AK_StmStatusIdle );
		}
    }

	return pXferInfo;
}

// TryGetIOBuffer(): return address of user buffer for the next IO transfer.
void * CAkStdStmDeferredLinedUp::TryGetIOBuffer()
{
    AKASSERT( m_pBuffer );
    return (AkUInt8*)m_pBuffer + m_uCumulTransferSize;
}

// Compute task's deadline for next operation.
// Sync: None: if m_uCumulTransferSize is changed in the meantime, the scheduler could take a suboptimal decision.
AkReal32 CAkStdStmDeferredLinedUp::EffectiveDeadline()
{
	AkUInt32 uGranularity = m_pDevice->GetGranularity();
	AkUInt32 uNumTransfersRemaining = ( m_uBufferSize - m_uCumulTransferSize + uGranularity - 1 ) / uGranularity;
	AKASSERT( uNumTransfersRemaining > 0 );
    AkReal32 fDeadline = ( m_fDeadline / uNumTransfersRemaining ) - AKPLATFORM::Elapsed( m_pDevice->GetTime(), m_iIOStartTime );
    return ( fDeadline > 0 ? fDeadline : 0 );
}

//-----------------------------------------------------------------------------
// Name: class CAkAutoStmDeferredLinedUp
// Desc: Base automatic stream implementation.
//-----------------------------------------------------------------------------
CAkAutoStmDeferredLinedUp::CAkAutoStmDeferredLinedUp()
{
}

CAkAutoStmDeferredLinedUp::~CAkAutoStmDeferredLinedUp()
{
}


AKRESULT CAkAutoStmDeferredLinedUp::Init( 
    CAkDeviceBase *             in_pDevice,         // Owner device.
    const AkFileDesc &          in_pFileDesc,       // File descriptor.
    const AkAutoStmHeuristics & in_heuristics,      // Streaming heuristics.
    AkAutoStmBufSettings *      in_pBufferSettings, // Stream buffer settings. Pass NULL to use defaults (recommended).
    AkUInt32                    in_uGranularity     // Device's I/O granularity.
    )
{
	// Cannot fail.
	m_listPendingXfers.Init();
	m_listCancelledXfers.Init();

	return CAkAutoStmBase::Init(
		in_pDevice,
		in_pFileDesc,
		in_heuristics,
		in_pBufferSettings,
		in_uGranularity );
}

// Asynchronous counterpart for async devices.
AkAsyncIOTransferInfo * CAkAutoStmDeferredLinedUp::TransferInfo(
	void *				in_pBuffer,			// Buffer for transfer.
	AkFileDesc *&		out_pFileDesc		// Stream's associated file descriptor.
	)
{
	out_pFileDesc = &m_fileDesc;

    // Lock status.
    AkAutoLock<CAkLock> atomicPosition( m_lockStatus );

	// Status is locked: last chance to bail out if the stream was destroyed by client.
	if ( m_bIsToBeDestroyed || !ReadyForIO() )
		return NULL;

	AkUInt64 uPosition;
	AkUInt32 uSize;

	// Required transfer size is the buffer size for this stream.
    // Slice request to granularity.
	if ( m_uBufferSize > m_pDevice->GetGranularity() )
        uSize = m_pDevice->GetGranularity();
    else
        uSize = m_uBufferSize;

	
	// Compute (absolute) file position for transfer.
	
	// Get position of last transfer request that was sent to Low-Level IO.
	uPosition = GetVirtualFilePosition();	

	// Handle loop buffering.
	if ( m_uLoopEnd 
		&& uPosition >= m_uLoopEnd + GetFileOffset() )
	{
		// Read at the beginning of the loop region. Snap to Low-Level block size.
		uPosition = m_uLoopStart + GetFileOffset();
	}

	// Enqueue new transfer request.
	
	AkUInt32 uExpectedTransferSize;
	bool bUnused;
	AkAsyncIOTransferInfo * pXferInfo = PushTransferRequest( 
		in_pBuffer, 
		uPosition, 
		uSize, 
		false, 
		uExpectedTransferSize, 
		bUnused );
	if ( pXferInfo )
    {
		AKASSERT( uExpectedTransferSize > 0 );

		m_uVirtualBufferingSize += uExpectedTransferSize;
		UpdateSchedulingStatus();

		// Reset timer. Time count since last transfer starts now.
		m_iIOStartTime = m_pDevice->GetTime();
    }
	return pXferInfo;
}

AkUInt64 CAkAutoStmDeferredLinedUp::GetVirtualFilePosition()
{
	// Must be locked.

	// Find the most recent transfer that was not cancelled (can be pending or completed).
	if ( m_listPendingXfers.Last() )
		return m_listPendingXfers.Last()->EndPosition();

	// Could not find a pending transfer that was not cancelled. Check in our list of buffers.
	if ( m_listBuffers.Length() > m_uNextToGrant )
		return ( m_listBuffers.Last()->uPosition + m_listBuffers.Last()->uDataSize );
	else
		return m_uNextExpectedUserPosition;	
}

// Cancel all pending transfers.
// Skip buffers whose position is smaller than in_uMaxKeepPosition. Flush all the rest
// (flush prefetched loops). Specify 0 to flush all.		
void CAkAutoStmDeferredLinedUp::CancelPendingTransfers(
	const AkUInt64 in_uMaxKeepPosition	// Request position under which requests should not be flushed.
	)
{
	_CancelPendingTransfers( in_uMaxKeepPosition );
}

// Returns true if and only if there was a transfer pending that could be cancelled.
bool CAkAutoStmDeferredLinedUp::CancelLastTransfer()
{
	// Get the most recent transfer and cancel it (can be pending or completed):
	// Put in cancelled queue, tag it, notify the LowLevelIO.
	if ( !m_listPendingXfers.IsEmpty() )
	{
		CAkPendingTransfer * pTransfer = m_listPendingXfers.First();
		AKASSERT( !pTransfer->WasCancelled() || !"A cancelled transfer is in the pending queue" );
		m_listPendingXfers.RemoveFirst();

		if ( !pTransfer->WasCompleted() )
		{
			AddToCancelledList( pTransfer );
			bool bCancelAll = false;
			pTransfer->Cancel( m_fileDesc, (IAkIOHookDeferred*)m_pDevice->GetLowLevelHook(), true, bCancelAll );
		}
		else
	    {
			CancelCompleted( pTransfer );
    	}
		return true;
	}
	return false;
}

// Try to remove a buffer from this stream's list of buffers. Fails if user owns them all.
// NOTE: This implementation differs from the blocking device's because we must also check 
// pending transfers.
// Sync: Status.
void * CAkAutoStmDeferredLinedUp::PopIOBuffer( )
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

	if ( m_listPendingXfers.IsEmpty() && m_listBuffers.IsEmpty() )
	{
		// This task cannot let a buffer go. Notify scheduler that memory is blocked for a while.
		m_pDevice->NotifyMemIdle();
		m_pDevice->Unlock();
		return NULL;
	}

    // Memory is still full. Try to remove a buffer from this task.
	// But do not remove if
	// 1) All buffers are already granted to client.
	// 2) Removing a buffer would take this stream under target buffering.

	// Buffering size if removal is successful is that of latest pending transfer if applicable,
	// or latest stored buffer otherwise.
	AkUInt32 uSizeToRemove;
	if ( m_listPendingXfers.Last() )
		uSizeToRemove = m_listPendingXfers.Last()->info.uRequestedSize;
	else
		uSizeToRemove = m_listBuffers.Last()->uDataSize;

	// Because the virtual buffering size is only updated when transfers complete (cancelled or not),
	// we need to compute virtual buffering size value which takes currently cancelled transfers 
	// into account.
	AkUInt32 uVirtualBufferingSize = m_uVirtualBufferingSize;
	CancelledTransfersList::Iterator it = m_listCancelledXfers.Begin();
	while ( it != m_listCancelledXfers.End() )
	{
		AKASSERT( uVirtualBufferingSize >= (*it)->info.uRequestedSize );
		uVirtualBufferingSize -= (*it)->info.uRequestedSize;
		++it;
	}

    if ( uVirtualBufferingSize >= uSizeToRemove
		&& !NeedsBuffering( uVirtualBufferingSize - uSizeToRemove ) )
    {
		// Our virtual size could consist of pending transfers. Try cancelling the latest transfer.
		// If it doesn't work (because there are no transfer to cancel, then remove a buffer from the 
		// buffers list.
		if ( !CancelLastTransfer() )
		{
			// No transfer to cancel: remove a buffer.
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
			// Transfer was cancelled. A buffer will be freed eventually. 
			// Perhaps it was freed during call to CancelTransfer(). Try again.
			pBuffer = AK::MemoryMgr::GetBlock( m_pDevice->GetIOPoolID() );
			if ( !pBuffer )
			{
				// No buffer was freed while cancelling. It will occur later, from another thread.
				// We can safely notify scheduler that memory is blocked in the meantime.
				m_pDevice->NotifyMemIdle();
            }
    	}
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

#ifdef _DEBUG
void CAkAutoStmDeferredLinedUp::CheckVirtualBufferingConsistency()
{
	AkUInt32 uVirtualBuffering = 0;
	AkUInt32 uNumGranted = m_uNextToGrant;
	{
		AkBufferList::Iterator it = m_listBuffers.Begin();
		while ( it != m_listBuffers.End() )
		{
			if ( uNumGranted == 0 ) // Skip buffers granted.
				uVirtualBuffering += (*it)->uDataSize;
			else 
				uNumGranted--;
			++it;
		}
	}
	{
		PendingTransfersList::Iterator it = m_listPendingXfers.Begin();
		while ( it != m_listPendingXfers.End() )
		{
			uVirtualBuffering += (AkUInt32)( (*it)->EndPosition() - (*it)->StartPosition() );
			++it;
		}
	}
	{
		// Also add all transfers marked as cancelled but not removed from task yet.
		CancelledTransfersList::Iterator it = m_listCancelledXfers.Begin();
		while ( it != m_listCancelledXfers.End() )
		{
			uVirtualBuffering += (AkUInt32)( (*it)->EndPosition() - (*it)->StartPosition() );
			++it;
		}
	}
	AKASSERT( uVirtualBuffering == m_uVirtualBufferingSize );
}
#endif
