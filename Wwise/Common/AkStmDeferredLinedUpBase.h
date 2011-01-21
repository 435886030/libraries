//////////////////////////////////////////////////////////////////////
//
// AkStmDeferredLinedUpBase.h
//
// Template layer that adds pending transfer handling services to 
// stream objects that should be used in deferred devices.
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#ifndef _AK_STM_DEFERRED_LINEDUP_H_
#define _AK_STM_DEFERRED_LINEDUP_H_

#include "AkDeviceBase.h"
#include <AK/Tools/Common/AkListBare.h>
#include <AK/Tools/Common/AkAutoLock.h>
#include <AK/Tools/Common/AkMonitorError.h>
#include "AkPendingTransfer.h"

namespace AK
{
namespace StreamMgr
{

	//-----------------------------------------------------------------------------
    // Name: CAkDeviceDeferredLinedUpBase
    // Desc: Base implementation of the deferred lined-up scheduler.
    //-----------------------------------------------------------------------------
    class CAkDeviceDeferredLinedUpBase : public CAkDeviceBase
    {
    public:

		CAkDeviceDeferredLinedUpBase( IAkLowLevelIOHook * in_pLowLevelHook )
			:CAkDeviceBase( in_pLowLevelHook ) {}
		virtual ~CAkDeviceDeferredLinedUpBase() {}
		
		// Get/release cached transfer objects. CAkPendingTransfers are cached at device initialization
		// because falling out-of-memory when trying to get a transfer object has dramatic consequences
		// on system behaviour: the high-priority IO thread will keep on trying to execute I/O.
		// IMPORTANT: The CAkIOThread is locked inside Get/ReleaseTransferObject.
		inline CAkPendingTransfer * GetTransferObject()
		{
			AkAutoLock<CAkIOThread> transferCache( *this );
			CAkPendingTransfer * pTransferObj = m_listFreeTransferObjs.First();
			AKASSERT( pTransferObj || !"Not enough cached transfer objects" );
			m_listFreeTransferObjs.RemoveFirst();
			return pTransferObj;
		}
		inline void ReleaseTransferObject( CAkPendingTransfer * in_pTransferObj )
		{
			AkAutoLock<CAkIOThread> transferCache( *this );
			m_listFreeTransferObjs.AddFirst( in_pTransferObj );
		}

    protected:

		typedef AkListBareLight<CAkPendingTransfer, AkListBareNextTransfer> FreeTransfersList;
        FreeTransfersList	m_listFreeTransferObjs;	// List of free transfers.
    };

	template <class TStmBase>
	class CAkStmDeferredLinedUpBase : public TStmBase
	{
	public:

		// Override CanBeDestroyed: only if no transfer is pending.
		virtual bool	CanBeDestroyed();

		// Update task after I/O transfer.
		// Handles completed requests that were not received in the order they were sent.
        virtual void Update(
			void *		in_pCookie,			// Cookie: transfer object. NULL if transfer object was not created.
			const AkUInt64 in_uPosition,	// Absolute file position of transfer.
            void *      in_pBuffer,			// Address of data.
			AkUInt32    in_uActualIOSize,	// Size available for writing/reading.
			AKRESULT	in_eIOResult		// AK_Success if IO was successful, AK_Cancelled if IO was cancelled, AK_Fail otherwise.
            );
		
	protected:

		CAkStmDeferredLinedUpBase();
		virtual ~CAkStmDeferredLinedUpBase();

		// Updates all "completed" transfers whose updating was deferred because they did not arrive in
		// the order in which they were sent.
		void UpdateCompletedTransfers();

		// Removes transfer request that just completed. Need to pass the position and size of the 
		// transfer (after processing and validation) in order to correct the virtual position.
		void PopTransferRequest(
			CAkPendingTransfer *	in_pTransfer,				// Transfer object to remove.
			const AkUInt64			in_uPositionAfterRequest,	// Absolute position in file of the end of buffer.
			bool					in_bStoreData				// True if there was no error.
            );
		
		// Returns the expected transfer size (that is, takes into account the end of file not being aligned with the
		// required transfer size.
		AkAsyncIOTransferInfo * PushTransferRequest(
			void *				in_pBuffer,			// Buffer for transfer.
			const AkUInt64		in_uPosition,		// Position in file (absolute).
			AkUInt32			in_uSize,			// Required transfer size.
			bool				in_bWriteOp,		// True if write operation.
			AkUInt32 &			out_uExpectedTransferSize,	// Expected transfer size (takes into account the end of 
															// file not being aligned with the required transfer size.
			bool &				out_bWillReachEof	// Returned as true if this transfer will hit the EOF boundary.
			);

		// Mark all pending transfers as cancelled and notify Low-Level IO.
		// Skip buffers whose position is smaller than in_uMaxKeepPosition. Flush all the rest
		// (flush prefetched loops). Specify 0 to flush all.
		void _CancelPendingTransfers(
			const AkUInt64 in_uMaxKeepPosition = 0	// Request position under which requests should not be flushed.
			);

		// Cancel and destroy a transfer that was already completed (already returned from the Low-Level IO).
		// IMPORTANT: the transfer must be dequeued from any list before calling this function.
		// IMPORTANT: the transfer will be destroyed therein.
		void CancelCompleted( 
			CAkPendingTransfer * in_pTransfer 
			);
			
		// Returns true if the given transfer object is the first PENDING transfer of the queue.
		bool IsOldestPendingTransfer( 
			CAkPendingTransfer * in_pTransfer 
			);
		// Returns the oldest completed transfer in the queue.
		// Cancelled transfers are skipped. If the first transfer that is not cancelled is still pending,
		// or if there are no completed transfers, it returns NULL.
		CAkPendingTransfer * GetOldestCompletedTransfer();

		// Push a transfer in the list of cancelled transfers.
		// Sets transfer's status to 'cancelled'.
		inline void AddToCancelledList(
			CAkPendingTransfer * in_pTransfer
			)
		{
			in_pTransfer->TagAsCancelled();
			m_listCancelledXfers.AddFirst( in_pTransfer );
		}

	protected:
		// IMPORTANT: Transfers are enqueued by the head: the latest transfer is the first of the queue.
		typedef AkListBare<CAkPendingTransfer, AkListBareNextTransfer> PendingTransfersList;
        PendingTransfersList	m_listPendingXfers;	// List of pending transfers.

		// IMPORTANT: Both lists use the same "next transfer" member, because a transfer can only be 
		// in one of these lists at a time.
		typedef AkListBareLight<CAkPendingTransfer, AkListBareNextTransfer> CancelledTransfersList;
        CancelledTransfersList	m_listCancelledXfers;	// List of cancelled transfers.
	};

#include "AkStmDeferredLinedUpBase.inl"
}
}
#endif	//_AK_STM_DEFERRED_LINEDUP_H_
