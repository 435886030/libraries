//////////////////////////////////////////////////////////////////////
//
// AkPendingTransfer.h
//
// Pending transfer object used by the deferred lined-up device.
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#ifndef _AK_PENDING_TRANSFER_H_
#define _AK_PENDING_TRANSFER_H_

#include "AkStreamMgr.h"

namespace AK
{
namespace StreamMgr
{
	class CAkStmTask;

	struct CAkPendingTransfer
	{
	public:
		enum TransferStatusType
		{
			TransferStatus_Pending,
			TransferStatus_Completed,
			TransferStatus_Cancelled
		};

		void Prepare( 
			CAkStmTask * in_pOwner, 
			const AkUInt64 in_uExpectedFilePosition,
			void * in_pBuffer,
			const AkUInt64 in_uPosition,
			AkUInt32 in_uBufferSize,
			AkUInt32 in_uRequestedSize,
			AkIOCallback in_pCallback
			)
		{
			uExpectedFilePosition = in_uExpectedFilePosition;
			pOwner = in_pOwner;
			eStatus = TransferStatus_Pending;
			bWasLLIOCancelCalled = false;
			info.pBuffer = in_pBuffer;
			StartPosition( in_uPosition );
			info.uBufferSize = in_uBufferSize;
			info.uRequestedSize = in_uRequestedSize;
			info.uSizeTransferred = 0;
			info.pCallback = in_pCallback;
			info.pCookie = this;	// Keep transfer object in cookie.
			info.pUserData = NULL;
		}

		// Returns true if the transfer's data can be added to the stream, in CAkStmTask::Update(). 
		// If it returns false, the data is flushed. 
		// 
		inline bool DoStoreData(
			const AkUInt64 & in_uPositionAfterRequest	// Absolute position in file of the end of buffer.
			)
		{
			return !WasCancelled()
				&& ( in_uPositionAfterRequest >= uExpectedFilePosition );
		}

		inline bool WasCancelled()
		{
			return ( TransferStatus_Cancelled == eStatus );
		}

		inline bool WasCompleted()
		{
			return ( TransferStatus_Completed == eStatus );
		}

		// Set transfer status to 'cancelled'. Call this method before calling Cancel().
		inline void TagAsCancelled()
		{
			AKASSERT ( !WasCompleted() );
			eStatus = TransferStatus_Cancelled;
		}

		inline void TagAsCompleted()
		{
			AKASSERT( TransferStatus_Pending == eStatus );
			eStatus = TransferStatus_Completed;
		}

		// Cancel a transfer: calls Cancel() on the Low-Level IO if required to do so.
		// This transfer must have been tagged as 'cancelled' before calling this method.
		inline void Cancel( 
			AkFileDesc & in_fileDesc, 
			IAkIOHookDeferred * in_pLowLevelHook, 
			bool in_bCallLowLevelIO, 
			bool & io_bAllCancelled 
			)
		{
			AKASSERT ( WasCancelled() );
			
			if ( in_bCallLowLevelIO )
			{
				if ( !bWasLLIOCancelCalled )
				{
					in_pLowLevelHook->Cancel( in_fileDesc, info, io_bAllCancelled );
				}
				else
				{
					// Cancel() was already called. Clear io_bAllCancelled, as we still need to 
					// ask the Low-Level IO to cancel next transfers, if applicable.
					io_bAllCancelled = false;
				}
			}
			bWasLLIOCancelCalled = true;
		}

		// Get transfer buffer.
		inline void * GetBuffer()
		{
			return info.pBuffer;
		}

		inline AkUInt64 StartPosition() const
		{
			return info.uFilePosition;
		}
		inline void StartPosition( AkUInt64 in_uPosition )
		{
			info.uFilePosition = in_uPosition;
		}
		inline AkUInt64 EndPosition() const
		{
			return uExpectedFilePosition;
		}
		inline CAkStmTask *	Owner() const
		{
			return pOwner;
		}

	private:
		AkUInt64		uExpectedFilePosition;	// Expected file position after transfer completed successfully.
		CAkStmTask *	pOwner;					// Owner task.
	public:
		CAkPendingTransfer *	pNextTransfer;	// Pointer to next transfer (required by AkListbareXX): PendingTransfersList or CancelledTransfersList.
		AkAsyncIOTransferInfo	info;			// Asynchronous transfer info.
	private:
		AkUInt32 /*TransferStatusType*/	eStatus	:3;	// Status type. Starts as Pending, then becomes completed or cancelled.
		AkUInt32		bWasLLIOCancelCalled	:1;	// This bit is set when IAkLowLevelIO::Cancel() is called (to avoid calling it more than once).
	};

	// Next item policy for list bare.
	struct AkListBareNextTransfer
	{
		static AkForceInline CAkPendingTransfer *& Get( CAkPendingTransfer * in_pItem ) 
		{
			return in_pItem->pNextTransfer;
		}
	};
}
}
#endif // _AK_PENDING_TRANSFER_H_

