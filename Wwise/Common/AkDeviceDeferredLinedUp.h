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
#ifndef _AK_DEVICE_DEFERRED_LINEDUP_H_
#define _AK_DEVICE_DEFERRED_LINEDUP_H_

#include "AkDeviceBase.h"
#include "AkStmDeferredLinedUpBase.h"

namespace AK
{
namespace StreamMgr
{

    //-----------------------------------------------------------------------------
    // Name: CAkDeviceDeferredLinedUp
    // Desc: Implementation of the deferred lined-up scheduler.
    //-----------------------------------------------------------------------------
    class CAkDeviceDeferredLinedUp : public CAkDeviceDeferredLinedUpBase
    {
    public:

        CAkDeviceDeferredLinedUp( 
			IAkLowLevelIOHook *	in_pLowLevelHook 
			);
        virtual ~CAkDeviceDeferredLinedUp();

		virtual AKRESULT	Init( 
            const AkDeviceSettings &	in_settings,
            AkDeviceID					in_deviceID 
            );
		virtual void		Destroy();
        
        // Stream creation interface override
        // (because we need to initialize specialized stream objects).
        // ---------------------------------------------------------------
        // Standard stream.
        virtual CAkStmTask * CreateStd(
            AkFileDesc &		in_fileDesc,        // Application defined ID.
            AkOpenMode          in_eOpenMode,       // Open mode (read, write, ...).
			IAkStdStream *&     out_pStream         // Returned interface to a standard stream.
            );
        // Automatic stream.
        virtual CAkStmTask * CreateAuto(
            AkFileDesc &				in_fileDesc,        // Application defined ID.
            const AkAutoStmHeuristics & in_heuristics,      // Streaming heuristics.
            AkAutoStmBufSettings *      in_pBufferSettings, // Stream buffer settings. Pass NULL to use defaults (recommended).
			IAkAutoStream *&            out_pStream         // Returned interface to an automatic stream.
            );
            
    protected:
        
        // This device's implementation of PerformIO().
        virtual void PerformIO( );

        // Execute task chosen by scheduler.
        void ExecuteTask( 
            CAkStmTask *	in_pTask,
			void *			in_pBuffer,
			AkReal32		in_fOpDeadline
            );

		CAkPendingTransfer * m_pXferObjMem;
    };

    //-----------------------------------------------------------------------------
    // Stream objects specificity.
    //-----------------------------------------------------------------------------

    //-----------------------------------------------------------------------------
    // Name: class CAkStdStmDeferredLinedUp
    // Desc: Overrides methods for deferred lined-up device specificities.
    //-----------------------------------------------------------------------------
    class CAkStdStmDeferredLinedUp : public CAkStmDeferredLinedUpBase<CAkStdStmBase>
    {
    public:

        CAkStdStmDeferredLinedUp();
        virtual ~CAkStdStmDeferredLinedUp();

		AKRESULT Init(
            CAkDeviceBase *     in_pDevice,         // Owner device.
            const AkFileDesc &  in_fileDesc,        // File descriptor.
            AkOpenMode          in_eOpenMode        // Open mode.
            );

		// Override Read/Write to keep track of amount transferred for one given client request.
        virtual AKRESULT Read(
            void *          in_pBuffer,         // User buffer address. 
            AkUInt32        in_uReqSize,        // Requested read size.
            bool            in_bWait,           // Block until operation is complete.
            AkPriority      in_priority,        // Heuristic: operation priority.
            AkReal32        in_fDeadline,       // Heuristic: operation deadline (s).
            AkUInt32 &      out_uSize           // Size actually read.
            );
        virtual AKRESULT Write(
            void *          in_pBuffer,         // User buffer address. 
            AkUInt32        in_uReqSize,        // Requested write size. 
            bool            in_bWait,           // Block until operation is complete.
            AkPriority      in_priority,        // Heuristic: operation priority.
            AkReal32        in_fDeadline,       // Heuristic: operation deadline (s).
            AkUInt32 &      out_uSize           // Size actually written.
            );

		// Cancel.
        virtual void Cancel();

		// Override GetStatus(): return "pending" if we are in fact Idle, but transfers are pending.
        virtual AkStmStatus GetStatus();        // Get operation status.

		// Override TryGetIOBuffer(): return base address of user buffer. Increment is handled in this class.
		virtual void * TryGetIOBuffer();

		// Closes stream. The object is destroyed and the interface becomes invalid.
        virtual void	Destroy();
		
		// Asynchronous counterpart for async devices.
		virtual AkAsyncIOTransferInfo * TransferInfo(
			void *			in_pBuffer,			// Buffer for transfer.
			AkFileDesc *&	out_pFileDesc		// Stream's associated file descriptor.
			);

		// Scheduling heuristics.
        virtual AkReal32 EffectiveDeadline();   // Compute task's effective deadline for next operation, in ms.

	protected:
		AkUInt32 m_uCumulTransferSize;
    };

    //-----------------------------------------------------------------------------
    // Name: class CAkAutoStmDeferredLinedUp
    // Desc: Base automatic stream implementation.
    //-----------------------------------------------------------------------------
    class CAkAutoStmDeferredLinedUp : public CAkStmDeferredLinedUpBase<CAkAutoStmBase>
    {
    public:

        CAkAutoStmDeferredLinedUp();
        virtual ~CAkAutoStmDeferredLinedUp();

		AKRESULT Init(
            CAkDeviceBase *             in_pDevice,         // Owner device.
            const AkFileDesc &          in_pFileDesc,       // File descriptor.
            const AkAutoStmHeuristics & in_heuristics,      // Streaming heuristics.
            AkAutoStmBufSettings *      in_pBufferSettings, // Stream buffer settings. Pass NULL to use defaults (recommended).
            AkUInt32                    in_uGranularity     // Device's I/O granularity.
            );
		
		// Asynchronous counterpart for async devices.
		virtual AkAsyncIOTransferInfo * TransferInfo(
			void *				in_pBuffer,			// Buffer for transfer.
			AkFileDesc *&		out_pFileDesc		// Stream's associated file descriptor.
			);

	protected:

		// CAkAutoStm-specific implementation.
		// -------------------------------------

		// Automatic stream specific. 
        // For buffer reassignment. The scheduler calls this if there is no memory available and it considers
        // that this task could give away some of its buffers.
        virtual void * PopIOBuffer();

		// Automatic streams must implement a method that returns the file position after the last
		// valid (non cancelled) pending transfer. If there is no transfer pending, then it is the position
		// at the end of buffering.
		virtual AkUInt64 GetVirtualFilePosition();

		// Cancel pending transfers.
		// Skip buffers whose position is smaller than in_uMaxKeepPosition. Flush all the rest
		// (flush prefetched loops). Specify 0 to flush all.
		virtual void CancelPendingTransfers(
			const AkUInt64 in_uMaxKeepPosition	// Request position under which requests should not be flushed.
			);

		// Cancel last scheduled transfer.
		// Returns true if and only if there was a transfer pending that could be cancelled.
		virtual bool CancelLastTransfer();

#ifdef _DEBUG
		virtual void CheckVirtualBufferingConsistency();
#endif
    };
}
}
#endif //_AK_DEVICE_DEFERRED_LINEDUP_H_
