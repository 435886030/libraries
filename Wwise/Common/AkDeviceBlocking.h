//////////////////////////////////////////////////////////////////////
//
// AkDeviceBlocking.h
//
// Win32 specific Blocking Scheduler Device implementation.
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////
#ifndef _AK_DEVICE_BLOCKING_H_
#define _AK_DEVICE_BLOCKING_H_

#include "AkDeviceBase.h"

namespace AK
{
namespace StreamMgr
{

    //-----------------------------------------------------------------------------
    // Name: CAkDeviceBlocking
    // Desc: Implementation of the Blocking Scheduler device.
    //-----------------------------------------------------------------------------
    class CAkDeviceBlocking : public CAkDeviceBase
    {
    public:

        CAkDeviceBlocking( 
			IAkLowLevelIOHook *	in_pLowLevelHook
			);
        virtual ~CAkDeviceBlocking();

		// Stream creation interface.
        // --------------------------------------------------------

        // Standard stream.
        virtual CAkStmTask * CreateStd(
            AkFileDesc &		in_fileDesc,        // Application defined ID.
            AkOpenMode          in_eOpenMode,       // Open mode (read, write, ...).
			IAkStdStream *&     out_pStream         // Returned interface to a standard stream.
            );

        
        // Automatic stream
        virtual CAkStmTask * CreateAuto(
            AkFileDesc &				in_fileDesc,        // Application defined ID.
            const AkAutoStmHeuristics & in_heuristics,      // Streaming heuristics.
            AkAutoStmBufSettings *      in_pBufferSettings, // Stream buffer settings. Pass NULL to use defaults (recommended).
			IAkAutoStream *&            out_pStream         // Returned interface to an automatic stream.
            );

		inline AkUInt64 CurTransferPosition()
		{
			return m_uCurTransferPosition;
		}

    protected:

        // This device's implementation of PerformIO().
        void PerformIO();

        // Execute task chosen by scheduler.
        void ExecuteTask( 
            CAkStmTask *	in_pTask,
			void *			in_pBuffer,
			AkReal32		in_fOpDeadline
            );

	protected:
		AkUInt64 m_uCurTransferPosition;
    };

	//-----------------------------------------------------------------------------
    // Name: class CAkStdStmBlocking
    // Desc: Standard stream implementation.
    //-----------------------------------------------------------------------------
    class CAkStdStmBlocking : public CAkStdStmBase
    {
	public:
		 CAkStdStmBlocking();
		 virtual ~CAkStdStmBlocking();

	protected:

		// User interface.
		// ---------------------------------

		// Closes stream. The object is destroyed and the interface becomes invalid.
        virtual void Destroy();

		// Cancel.
        virtual void Cancel();


		// Task interface.
		// ---------------------------------

		// Get information for data transfer. 
        virtual bool TransferInfo( 
            AkFileDesc *& out_pFileDesc,    // Stream's associated file descriptor.
            AkUInt64 &  out_uPosition,		// Position in file (absolute).
			AkUInt32 &  out_uBufferSize,	// Buffer size available for transfer.
			AkUInt32 &  out_uRequestSize	// Exact transfer size.
            );

		// Update stream object after I/O.
        virtual void Update(
			void *		in_pCookie,			// Cookie for device specific usage: With blocking device, it NULL if the transfer was not prepared, non-NULL otherwise.
			const AkUInt64 in_uPosition,	// Absolute file position of transfer.
            void *      in_pBuffer,			// Address of data.
			AkUInt32    in_uActualIOSize,	// Size available for writing/reading.
			AKRESULT	in_eIOResult		// AK_Success if IO was successful, AK_Cancelled if IO was cancelled, AK_Fail otherwise.
            );

		// Data buffer access.
        // Tries to get a buffer for I/O. 
        // Returns the address of the user's buffer.
        virtual void * TryGetIOBuffer();

		// Scheduling heuristics.
        virtual AkReal32 EffectiveDeadline();   // Compute task's effective deadline for next operation, in ms.

	protected:
		// Note: m_bTransferInProgress is set to true as soon as a transfer was prepared (TransferInfo())
		// and is reset in Update().
		bool				m_bTransferInProgress;
	};
		
		
	//-----------------------------------------------------------------------------
    // Name: class CAkAutoStmBlocking
    // Desc: Automatic stream implementation.
    //-----------------------------------------------------------------------------
    class CAkAutoStmBlocking : public CAkAutoStmBase
    {
	public:
		 CAkAutoStmBlocking();
		 virtual ~CAkAutoStmBlocking();

		AKRESULT Init( 
			CAkDeviceBase *             in_pDevice,         // Owner device.
			const AkFileDesc &          in_fileDesc,        // File descriptor.
			const AkAutoStmHeuristics & in_heuristics,      // Streaming heuristics.
			AkAutoStmBufSettings *      in_pBufferSettings, // Stream buffer settings.
			AkUInt32                    in_uGranularity     // Device's I/O granularity.
			);

		// Task interface.
		// ---------------------------------

		// Get information for data transfer.
        virtual bool TransferInfo( 
            AkFileDesc *& out_pFileDesc,	// Stream's associated file descriptor.
            AkUInt64 &  out_uPosition,		// Position in file (absolute).
			AkUInt32 &  out_uBufferSize,	// Buffer size available for transfer.
			AkUInt32 &  out_uRequestSize	// Exact transfer size.
			);

		// Update stream object after I/O.
        virtual void Update(
			void *		in_pCookie,			// Cookie for device specific usage: With blocking device, it NULL if the transfer was not prepared, non-NULL otherwise.
			const AkUInt64 in_uPosition,	// Absolute file position of transfer.
            void *      in_pBuffer,			// Address of data.
			AkUInt32    in_uActualIOSize,	// Size available for writing/reading.
			AKRESULT	in_eIOResult		// AK_Success if IO was successful, AK_Cancelled if IO was cancelled, AK_Fail otherwise.
            );

	protected:
		
		// Automatic stream specific. 
        // For buffer reassignment. The scheduler calls this if there is no memory available and it considers
        // that this task could give away some of its buffers.
        virtual void * PopIOBuffer();

		// Returns the file position after the one and only valid (non cancelled) pending transfer. 
		// If there is no transfer pending, then it is the position at the end of buffering.
		virtual AkUInt64 GetVirtualFilePosition();
		
		// Cancel pending transfer.
		// Skip if transfer's position is smaller than in_uMaxKeepPosition.
		virtual void CancelPendingTransfers(
			const AkUInt64 in_uMaxKeepPosition	// Request position under which requests should not be flushed.
			);
		
		// Cancel last (most recent) pending transfer.
		// Returns true if and only if there was a transfer pending that could be cancelled.
		virtual bool CancelLastTransfer();

		// Returns the expected transfer size (that is, takes into account the end of file not being aligned with the
		// required transfer size.
		AkUInt32 PushTransferRequest(
			const AkUInt64		in_uPosition,		// Position in file (absolute).
			AkUInt32			in_uSize			// Required transfer size.
			);

#ifdef _DEBUG
		virtual void CheckVirtualBufferingConsistency();
		AkUInt32 			m_uExpectedTransferSize;
#endif

	protected:
		AkUInt64			m_uExpectedFilePosition;
		// Note: there is a slight difference between these 2 flags. Both are set to true as soon as 
		// a transfer was prepared. m_bTransferInProgress is always reset in Update() only, whereas 
		// m_bHasTransferPending may be reset if the transfer is cancelled, and is used to determine
		// whether we keep the data or not.
		AkUInt8				m_bHasTransferPending	:1;
		AkUInt8				m_bTransferInProgress	:1;
    };
}
}
#endif //_AK_DEVICE_BLOCKING_H_
