//////////////////////////////////////////////////////////////////////
//
// AkStmDeferredLinedUpBase.inl
//
// Template layer that adds pending transfer handling services to 
// stream objects that should be used in deferred devices.
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#include <AK/Tools/Common/AkAutoLock.h>

using namespace AK;

// Callback from Low-Level I/O.
static void LLIOCallback( 
	AkAsyncIOTransferInfo * in_pTransferInfo,	// Pointer to the AkAsyncIOTransferInfo structure that was passed to corresponding Read() or Write() call.
	AKRESULT		in_eResult			// Result of transfer: AK_Success or AK_Fail (stream becomes invalid).
	)
{
	if ( !in_pTransferInfo )
	{
		AKASSERT( !"Invalid cookie" );
		return;
	}

	// Clean error code from Low-Level IO.
    if ( in_eResult != AK_Success )
	{
		in_eResult = AK_Fail;
		AK_MONITOR_ERROR( AK::Monitor::ErrorCode_IODevice );
	}

	// Transfer object was stored in pCookie.
	CAkPendingTransfer * pTransferObject = (CAkPendingTransfer*)in_pTransferInfo->pCookie;
	AKASSERT( pTransferObject || !"Invalid transfer object: corrupted cookie" );
	CAkStmTask * pTask = pTransferObject->Owner();
	AKASSERT( pTask || !"Invalid transfer object: task is null" );

	// Update transfer info if it was not done by the Low-LevelIO.

	pTask->Update( 
		pTransferObject, 
		pTransferObject->StartPosition(), 
		pTransferObject->GetBuffer(),
		pTransferObject->info.uSizeTransferred,
		in_eResult );
}

template <class TStmBase>
CAkStmDeferredLinedUpBase<TStmBase>::CAkStmDeferredLinedUpBase()
{
}

template <class TStmBase>
CAkStmDeferredLinedUpBase<TStmBase>::~CAkStmDeferredLinedUpBase()
{
	m_listPendingXfers.Term();
	m_listCancelledXfers.Term();
}

// Override CanBeDestroyed: only if no transfer is pending.
// Sync: Ensure that another thread is not accessing the pending transfers array at the same time.
template <class TStmBase>
bool CAkStmDeferredLinedUpBase<TStmBase>::CanBeDestroyed()
{
	AkAutoLock<CAkLock> pendingXfers( TStmBase::m_lockStatus );

	AKASSERT( m_listPendingXfers.IsEmpty() );	// Should have been cancelled already.

	// Wait until the Low-Level I/O has finished cancelling all I/O transfers.
	return ( m_listCancelledXfers.IsEmpty() );
}

// Override Update() to handle completed requests that were not received in the order they were sent.
template <class TStmBase>
void CAkStmDeferredLinedUpBase<TStmBase>::Update(
	void *		in_pCookie,				// Cookie: transfer object. NULL if transfer object was not created.
	const AkUInt64 in_uPosition,		// Absolute file position of transfer.
    void *      in_pBuffer,             // Address of data.
	AkUInt32    in_uActualIOSize,       // Size available for writing/reading.
	AKRESULT	in_eIOResult			// AK_Success if IO was successful, AK_Cancelled if IO was cancelled, AK_Fail otherwise.
    )
{
	// Lock status.
	AkAutoLock<CAkLock> update( TStmBase::m_lockStatus );

	// If I/O was successful and this transfer should not be flushed, check if it is the first PENDING
	// one in the queue. If it isn't, tag it as "completed" but let it there without updating.
	// After updating, always check if the first transfer was tagged as completed in order to resolve it.

	CAkPendingTransfer * pTransfer = (CAkPendingTransfer*)in_pCookie;

	// in_pCookie must be set to valid transfer reference if IO was successful.
	AKASSERT( in_eIOResult != AK_Success || pTransfer );

	bool bStoreData = ( AK_Success == in_eIOResult 
					&& pTransfer->DoStoreData( in_uPosition + in_uActualIOSize ) );

	if ( bStoreData
		&& !IsOldestPendingTransfer( pTransfer ) )
	{
		// Tag it as completed. Resolve later.
		pTransfer->TagAsCompleted();

		// Note: do not decrement IO count for transfers completed out-of-order. It still counts 
		// as a transfer that is pending in the Low-Level IO (the latter should have completed them
		// in the correct order anyway). Using this policy, we are able to bound the number of pending
		// transfer (thus to pre-allocate them).
		// IO count is decremented in UpdateCompletedTransfers().
	}
	else
	{
		AkUInt32 uPositionOffset = TStmBase::UpdatePosition( in_uPosition, in_pBuffer, in_uActualIOSize, bStoreData );

		if ( pTransfer )
		{
			bool bWasCancelled = pTransfer->WasCancelled();

			PopTransferRequest( pTransfer, in_uPosition + uPositionOffset, bStoreData );

			// If transfer was not cancelled, update all "completed" transfers whose updating was deferred because 
			// they did not arrive in the order in which they were sent.
			// However, if the transfer was cancelled, the pending list is not inspected to resolve "completed"
			// transfers, because an older transfer cannot be cancelled while a new transfer is not.
			// NOTE: Also, methods that cancel many transfers iterate through the list and call this method:
			// they count on the fact that the list will remain intact, apart from the transfer they are cancelling.
			if ( !bWasCancelled )
				UpdateCompletedTransfers();
		}

		TStmBase::UpdateTaskStatus( in_eIOResult );

		TStmBase::m_pDevice->DecrementIOCount();
	}
}

template <class TStmBase>
void CAkStmDeferredLinedUpBase<TStmBase>::UpdateCompletedTransfers()
{
	// Update while old transfers were completed but not processed yet.
	CAkPendingTransfer * pOldestTransfer = GetOldestCompletedTransfer();
	while ( pOldestTransfer )
	{
		AkUInt64 uPosition = pOldestTransfer->StartPosition();
		AkUInt32 uPositionOffset = TStmBase::UpdatePosition( 
			uPosition,
			pOldestTransfer->GetBuffer(),
			pOldestTransfer->info.uSizeTransferred,
			true );
		PopTransferRequest( pOldestTransfer, uPosition + uPositionOffset, true );

		TStmBase::m_pDevice->DecrementIOCount();

		pOldestTransfer = GetOldestCompletedTransfer();
	}
}

// Returns the expected transfer size (that is, takes into account the end of file not being aligned with the
// required transfer size.
template <class TStmBase>
AkAsyncIOTransferInfo * CAkStmDeferredLinedUpBase<TStmBase>::PushTransferRequest(
	void *				in_pBuffer,			// Buffer for transfer.
	const AkUInt64		in_uPosition,		// Position of file (absolute).
	AkUInt32			in_uSize,			// Required transfer size.
	bool				in_bWriteOp,		// True if write operation.
	AkUInt32 &			out_uExpectedTransferSize,	// Expected transfer size (takes into account the end of 
													// file and client transfer size).
	bool &				out_bWillReachEof	// Returned as true if this transfer will hit the EOF boundary.
	)
{
	AkUInt64 uPositionOfEof = TStmBase::m_fileDesc.iFileSize + TStmBase::m_fileDesc.uSector * TStmBase::m_uLLBlockSize;
	
	AKASSERT( in_uPosition < uPositionOfEof
			|| in_bWriteOp
			|| !"Preparing a 0 byte transfer is illegal" );
		
	AkUInt64 uExpectedFilePosition;

	if ( ( in_uPosition + in_uSize ) <= uPositionOfEof || in_bWriteOp )
	{
		uExpectedFilePosition = in_uPosition + in_uSize;
		out_uExpectedTransferSize = in_uSize;
		out_bWillReachEof = false;
	}
	else
	{
		uExpectedFilePosition = uPositionOfEof;
		out_uExpectedTransferSize = (AkUInt32)( uPositionOfEof - in_uPosition );	// Truncated at EOF.
		out_bWillReachEof = true;
	}
		
	CAkPendingTransfer * pTransfer = ((CAkDeviceDeferredLinedUpBase*)TStmBase::m_pDevice)->GetTransferObject();
	
	// Prepare transfer request.
	pTransfer->Prepare( 
		this, 
		uExpectedFilePosition,
		in_pBuffer,
		in_uPosition,
		in_uSize,
		out_uExpectedTransferSize,
		LLIOCallback );
	
	m_listPendingXfers.AddLast( pTransfer );
	
	return &pTransfer->info;
}

template <class TStmBase>
void CAkStmDeferredLinedUpBase<TStmBase>::PopTransferRequest(
	CAkPendingTransfer *	in_pTransfer,				// Transfer object to remove.
	const AkUInt64 			in_uPositionAfterRequest,	// Absolute position in file of the end of buffer.
	bool					in_bStoreData				// True if there was no error.
	)
{
	// Search the cancelled list if it was cancelled. Otherwise, it MUST be the oldest transfer (unless there was an IO error).
	if ( !in_pTransfer->WasCancelled() )
	{
		if ( in_bStoreData 
			|| m_listPendingXfers.First() == in_pTransfer )
		{
			AKASSERT( m_listPendingXfers.First() == in_pTransfer );
			m_listPendingXfers.RemoveFirst();
		}
		else
		{
#ifdef _DEBUG
			bool bFound = false;
#endif
			// If there was an error, this transfer could be in any order. Search it.
			PendingTransfersList::IteratorEx it = m_listPendingXfers.BeginEx();
			while ( it != m_listPendingXfers.End() )
			{
				if ( (*it) == in_pTransfer )
				{
					m_listPendingXfers.Erase( it );
#ifdef _DEBUG
					bFound = true;
#endif
					break;
				}
				++it;
			}
		}
	}
	else
	{
#ifdef _DEBUG
		bool bFound = false;
#endif
		CancelledTransfersList::IteratorEx it = m_listCancelledXfers.BeginEx();
		while ( it != m_listCancelledXfers.End() )
		{
			if ( (*it) == in_pTransfer )
			{
				m_listCancelledXfers.Erase( it );
#ifdef _DEBUG
				bFound = true;
#endif
				break;
			}
			++it;
		}
		AKASSERT( bFound || !"Could not find transfer object to dequeue" );
	}

	// Notify stream object that a transfer has been removed from the list.
	AKASSERT( in_pTransfer->EndPosition() >= in_uPositionAfterRequest );
	TStmBase::OnTransferRemoved( in_pTransfer->EndPosition(), in_uPositionAfterRequest );

	((CAkDeviceDeferredLinedUpBase*)TStmBase::m_pDevice)->ReleaseTransferObject( in_pTransfer );
}

template <class TStmBase>
void CAkStmDeferredLinedUpBase<TStmBase>::_CancelPendingTransfers(
	const AkUInt64 in_uMaxKeepPosition	/*=0*/ // Request position under which requests should not be flushed.
	)
{
	// Start by moving all transfers in the cancelled list, tagging them as cancelled, 
	// then call Cancel() on the Low-Level IO (the latter could decide to cancel them all at once, 
	// so we must have them tagged appropriately before calling the Low-Level IO).
	// If the transfer was already completed, update now.

	bool bAllCancelled = true;
	{
		PendingTransfersList::IteratorEx it = m_listPendingXfers.BeginEx();

		// Skip transfers that should be kept.
		while ( it != m_listPendingXfers.End()
				&& (*it)->StartPosition() < in_uMaxKeepPosition )
		{
			CAkPendingTransfer * pTransfer = (*it);

			// Keep this transfer.
			++it;
			bAllCancelled = false;

			if ( pTransfer->EndPosition() >= in_uMaxKeepPosition )
			{
				// This was the last transfer to keep. 
				break;
			}
		}

		// (it) now points to the most recent buffer that should be flushed.

		while ( it != m_listPendingXfers.End() )
		{
			CAkPendingTransfer * pTransfer = *it;
			AKASSERT( !pTransfer->WasCancelled() || !"A cancelled transfer is in the pending queue" );
			it = m_listPendingXfers.Erase( it );

			if ( !pTransfer->WasCompleted() )
			{
				AddToCancelledList( pTransfer );
			}
			else
			{
				CancelCompleted( pTransfer );
			}
			
		}
	}

	{
		bool bCallLowLevelIO = true;
		CancelledTransfersList::Iterator it = m_listCancelledXfers.Begin();
		while ( it != m_listCancelledXfers.End() )
		{
			// IMPORTANT: Cache reference before calling CancelTransfer because it could be dequeued 
			// inside this function: The Low-Level IO can call Update() directly from this thread.
			CAkPendingTransfer * pTransfer = (*it);
			++it;
			// This notifies the Low-Level IO or calls Update directly if transfer was already completed.
			pTransfer->Cancel( 
				TStmBase::m_fileDesc, 
				static_cast<IAkIOHookDeferred*>( TStmBase::m_pDevice->GetLowLevelHook() ),
				bCallLowLevelIO, 
				bAllCancelled );
			bCallLowLevelIO = !bAllCancelled;
		}
	}
}

// Cancel and destroy a transfer that was already completed (already returned from the Low-Level IO).
// IMPORTANT: the transfer must be dequeued from any list before calling this function.
// IMPORTANT: the transfer will be destroyed therein.
template <class TStmBase>
void CAkStmDeferredLinedUpBase<TStmBase>::CancelCompleted( 
	CAkPendingTransfer * in_pTransfer 
	)
{
	AKASSERT( in_pTransfer->WasCompleted() );

	// Update position with bStoreData = false (resources will be freed if applicable).
	TStmBase::UpdatePosition( 
		in_pTransfer->StartPosition(), 
		in_pTransfer->GetBuffer(), 
		in_pTransfer->info.uSizeTransferred, 
		false );
	
	// This transfer was removed from a list, and will not be reenqueued to be 
	// removed again: update stream.
	TStmBase::OnTransferRemoved( in_pTransfer->EndPosition(), in_pTransfer->StartPosition() );

	// Destroy.
	((CAkDeviceDeferredLinedUpBase*)TStmBase::m_pDevice)->ReleaseTransferObject( in_pTransfer );
}

// Returns true if the given transfer object is the oldest PENDING transfer of the queue.
template <class TStmBase>
bool CAkStmDeferredLinedUpBase<TStmBase>::IsOldestPendingTransfer( 
	CAkPendingTransfer * in_pTransfer 
	)
{
	AKASSERT( !in_pTransfer->WasCancelled() );

	// The oldest transfer is the first transfer in the queue. It cannot be cancelled
	// (would not be in this list) nor completed (would have been processed already).
	AKASSERT( !m_listPendingXfers.First()->WasCancelled() 
			&& !m_listPendingXfers.First()->WasCompleted() );
	return ( m_listPendingXfers.First() == in_pTransfer );
}

template <class TStmBase>
CAkPendingTransfer * CAkStmDeferredLinedUpBase<TStmBase>::GetOldestCompletedTransfer()
{
	if ( m_listPendingXfers.Length() > 0 )
	{
		// The oldest transfer is the first transfer in the queue. 
		// Return it if it is completed, NULL if it is still pending.
		AKASSERT( !m_listPendingXfers.First()->WasCancelled() );
		if ( m_listPendingXfers.First()->WasCompleted() )
			return m_listPendingXfers.First();
	}
	return NULL;
}

