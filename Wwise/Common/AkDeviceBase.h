//////////////////////////////////////////////////////////////////////
//
// AkDeviceBase.h
//
// Device implementation that is common across all high-level IO devices.
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////
#ifndef _AK_DEVICE_BASE_H_
#define _AK_DEVICE_BASE_H_

#include "AkIOThread.h"
#include <AK/Tools/Common/AkLock.h>
#include <AK/Tools/Common/AkArray.h>
#include "AkStreamMgr.h"
#include <AK/Tools/Common/AkPlatformFuncs.h>
#include <AK/Tools/Common/AkListBare.h>
#include <AK/Tools/Common/AkListBareLight.h>

#include <AK/SoundEngine/Common/AkStreamMgrModule.h>

// ------------------------------------------------------------------------------
// Defines.
// ------------------------------------------------------------------------------

#ifdef AK_WIN
#define OS_PRINTF	swprintf
#else
#define OS_PRINTF	sprintf
#endif


/// Stream type.
enum AkStmType
{
    AK_StmTypeStandard         	= 0,    	///< Standard stream for manual IO (AK::IAkStdStream).
    AK_StmTypeAutomatic        	= 1     	///< Automatic stream (AK::IAkAutoStream): IO requests are scheduled automatically into internal Stream Manager memory.
};

namespace AK
{
namespace StreamMgr
{
    class CAkStmTask;
    
	// ------------------------------------------------------------------------------
    // Stream buffer record used for automatic streams memory management.
    // ------------------------------------------------------------------------------
    struct AkStmBuffer
    {
		AkUInt64	uPosition;
        void *      pBuffer;
        AkUInt32    uDataSize;
		AkStmBuffer * pNextBuffer;	// List bare sibling: AkBufferList/AkFreeBufferList.
    };
	struct AkListBareNextBuffer
	{
		static AkForceInline AkStmBuffer *& Get( AkStmBuffer * in_pItem ) 
		{
			return in_pItem->pNextBuffer;
		}
	};
	typedef AkListBare<AkStmBuffer,AkListBareNextBuffer>		AkBufferList;
	typedef AkListBareLight<AkStmBuffer,AkListBareNextBuffer>	AkFreeBufferList;

    //-----------------------------------------------------------------------------
    // Name: CAkDeviceBase
    // Desc: Base implementation of the high-level I/O device interface.
    //       Implements the I/O thread, provides Stream Tasks (CAkStmTask) 
    //       scheduling services for data transfer. 
    // Note: Device description and access to platform API is handled by low-level.
    //       The I/O thread calls pure virtual method PerformIO(), that has to be
    //       implemented by derived classes according to how they communicate with
    //       the Low-Level IO.
    //       Implementation of the device logic is distributed across the device
    //       and its streaming objects.
    //-----------------------------------------------------------------------------
    class CAkDeviceBase : public CAkIOThread
#ifndef AK_OPTIMIZED
						, public AK::IAkDeviceProfile
#endif
    {
    public:

        CAkDeviceBase(
			IAkLowLevelIOHook *	in_pLowLevelHook
			);
        virtual ~CAkDeviceBase( );
        
		// Methods used by Stream Manager.
		// -------------------------------

       virtual AKRESULT	Init( 
            const AkDeviceSettings &	in_settings,
            AkDeviceID					in_deviceID 
            );
        virtual void	Destroy();

        AkDeviceID		GetDeviceID();

		// Stream objects creation.
        virtual CAkStmTask *	CreateStd(
            AkFileDesc &				in_fileDesc,        // Application defined ID.
            AkOpenMode                  in_eOpenMode,       // Open mode (read, write, ...).
            IAkStdStream *&             out_pStream         // Returned interface to a standard stream.
            ) = 0;
        virtual CAkStmTask *	CreateAuto(
            AkFileDesc &				in_fileDesc,        // Application defined ID.
            const AkAutoStmHeuristics & in_heuristics,      // Streaming heuristics.
            AkAutoStmBufSettings *      in_pBufferSettings, // Stream buffer settings. Pass NULL to use defaults (recommended).
            IAkAutoStream *&            out_pStream         // Returned interface to an automatic stream.
            ) = 0;

		// Force the device to clean up dead tasks. 
		void			ForceCleanup(
			bool in_bKillLowestPriorityTask,				// True if the device should kill the task with lowest priority.
			AkPriority in_priority							// Priority of the new task if applicable. Pass AK_MAX_PRIORITY to ignore.
            );
        
        // Methods used by stream objects.
        // --------------------------------------------------------
        
        // Access for stream objects.
		inline IAkLowLevelIOHook * GetLowLevelHook()
		{
			return m_pLowLevelHook;
		}
        inline AkUInt32 GetGranularity()
        {
            return m_uGranularity;
        }
        inline AkReal32 GetTargetAutoStmBufferLength()
        {
            return m_fTargetAutoStmBufferLength;
        }
        inline AkMemPoolId GetIOPoolID()
        {
            return m_streamIOPoolId;
        }
        inline AkInt64 GetTime()
        {
            return m_time;
        }

		// Get/release cached buffer holders. Buffer holders are cached at device initialization
		// because falling out-of-memory when trying to get a buffer holder has dramatic consequences
		// on system behaviour: the high-priority IO thread will keep on trying to execute I/O.
		// IMPORTANT: The CAkIOThread must be locked while calling Get/ReleaseCachedBufferHolder.
		inline AkStmBuffer * GetCachedBufferHolder()
		{
			AkStmBuffer * pBuffer = m_listFreeBufferHolders.First();
			AKASSERT( pBuffer || !"Not enough cached buffer holders" );
			m_listFreeBufferHolders.RemoveFirst();
			return pBuffer;
		}
		inline void ReleaseCachedBufferHolder( AkStmBuffer * in_pBufferHolder )
		{
			m_listFreeBufferHolders.AddFirst( in_pBufferHolder );
		}
        
        // Device Profile Ex interface.
        // --------------------------------------------------------
#ifndef AK_OPTIMIZED
        inline AkMemPoolId GetIOPoolSize()
        {
            return m_streamIOPoolSize;  // IO memory size.
        }

	    // Monitoring status.
        virtual AKRESULT     StartMonitoring();
	    virtual void         StopMonitoring();
        inline bool          IsMonitoring() { return m_bIsMonitoring; }

        // Device profiling.
        virtual void     GetDesc( 
            AkDeviceDesc & out_deviceDesc 
            );
        virtual bool     IsNew();
        virtual void     ClearNew();
        
        // Stream profiling.
        virtual AkUInt32 GetNumStreams();
        // Note. The following functions refer to streams by index, which must honor the call to GetNumStreams().
        virtual AK::IAkStreamProfile * GetStreamProfile( 
            AkUInt32    in_uStreamIndex             // [0,numStreams[
            );
#endif

    protected:

		// Device can buffer streams above their target buffering length if and only if it uses the 
		// uIdleWaitTime feature (the one that allows the device to stream in data during its free time 
		// - usually used when there is a lot of streaming memory).
		inline bool CanOverBuffer()
		{
			return ( GetIOThreadWaitTime() != AK_INFINITE );
		}

        // Add a new task to the list.
        void AddTask( 
            CAkStmTask * in_pStmTask
            );

        // Destroys all streams.
		// Returns true if it was able to destroy all streams. Otherwise, the IO thread needs to
		// wait for pending transfers to complete.
        virtual bool ClearStreams();

		// Called once when I/O thread starts.
		virtual void OnThreadStart();

        // Scheduler algorithm.
        // Finds the next task for which an I/O request should be issued.
        // Return: If a task is found, a valid pointer to a task is returned, as well
        // as the address in/from which to perform a data transfer.
        // Otherwise, returns NULL.
        CAkStmTask *    SchedulerFindNextTask(
			void *&		out_pBuffer,	// Returned I/O buffer used for this transfer.
			AkReal32 &	out_fOpDeadline	// Returned deadline for this transfer.
            );
        // Finds next task among standard streams only (typically when there is no more memory for automatic streams).
        CAkStmTask *    ScheduleStdStmOnly(
			void *&		out_pBuffer,	// Returned I/O buffer used for this transfer.
			AkReal32 &	out_fOpDeadline	// Returned deadline for this transfer.
            );

	protected:
		// Time in milliseconds. Stamped at every scheduler pass.
        AkInt64         m_time;

		// Task list.
        // Tasks live in m_arTasks from the time they are created until they are completely destroyed (by the I/O thread).
        // It is more efficient to query the tasks every time scheduling occurs than to add/remove them from the list, 
        // every time, from other threads.
        typedef AkListBareLight<CAkStmTask> TaskList;
        TaskList		m_listTasks;            // List of tasks.
        CAkLock         m_lockTasksList;        // Protects tasks array.

		// List of free cached buffer holder structures.
		AkFreeBufferList	m_listFreeBufferHolders;
		AkStmBuffer *		m_pBufferMem;

		// Low-Level I/O hook.
		IAkLowLevelIOHook *	m_pLowLevelHook;

		// Settings.
        AkUInt32        m_uGranularity;
        AkReal32        m_fTargetAutoStmBufferLength;
        /** Needed at thread level (CAkIOThread)
        AkUInt32		m_uIdleWaitTime;
		AkUInt32        m_uMaxConcurrentIO;
		**/

        // Memory.
        AkMemPoolId	    m_streamIOPoolId;		// IO memory.

        AkDeviceID      m_deviceID;

        // Profiling specifics.
#ifndef AK_OPTIMIZED
		AkUInt32        m_streamIOPoolSize;     // IO memory size.
        bool            m_bIsMonitoring;
        bool            m_bIsNew;
	public:
		// NOTE: Although ArrayStreamProfiles is used only inside CAkDeviceBase, the definition of its memory pool policy
		// must be public in order to avoid private nested class access inside AkArray. 
		AK_DEFINE_ARRAY_POOL( ArrayPoolLocal, CAkStreamMgr::GetObjPoolID() );
	protected:
		typedef AkArray<AK::IAkStreamProfile*,AK::IAkStreamProfile*,ArrayPoolLocal,AK_STM_OBJ_POOL_BLOCK_SIZE/sizeof(CAkStmTask*)> ArrayStreamProfiles;
        ArrayStreamProfiles m_arStreamProfiles; // Tasks pointers are copied there when GetNumStreams() is called, to avoid 
                                                // locking-unlocking the real tasks list to query each stream's profiling data.
#endif
    };


    //-----------------------------------------------------------------------------
    //
    // Stream objects implementation.
    //
    //-----------------------------------------------------------------------------

    //-----------------------------------------------------------------------------
    // Name: class CAkStmTask
    // Desc: Base implementation common to streams. Defines the interface used by
    //       the device scheduler.
    //-----------------------------------------------------------------------------
	class CAkStmTask : public CAkClientThreadAware
#ifndef AK_OPTIMIZED
        , public IAkStreamProfile
#endif
    {
    public:

		// Stream Manager interface.
		// -------------------------
		inline void	SetFileOpen() { m_bIsFileOpen = true; }
		AKRESULT SetDeferredFileOpen(
			const AkOSChar*				in_pszFileName,		// File name.
			AkFileSystemFlags *			in_pFSFlags,		// File system flags (can be NULL).
			AkOpenMode					in_eOpenMode		// Open mode.
			);
		AKRESULT SetDeferredFileOpen(
			AkFileID					in_fileID,			// File ID.
			AkFileSystemFlags *			in_pFSFlags,		// File system flags (can be NULL).
			AkOpenMode					in_eOpenMode		// Open mode.
			);

		// Scheduling interface.
		// -------------------------

        // Task management.
		AKRESULT EnsureFileIsOpen();

        // Returns true when the object is ready to be destroyed. Common. 
        inline bool IsToBeDestroyed()
        {
            // Note. When profiling, we need to have the profiler's agreement to destroy the stream.
        #ifndef AK_OPTIMIZED
            return m_bIsToBeDestroyed && 
                ( !m_pDevice->IsMonitoring( ) || m_bIsProfileDestructionAllowed );
        #else
            return m_bIsToBeDestroyed;
        #endif
        }

		// Task needs to acknowledge that it can be destroyed. Device specific (virtual). Call only when IsToBeDestroyed().
		// Default implementation returns "true".
		// Note: Implementations must always lock the task's status in order to avoid race conditions between
		// client's Destroy() and the I/O thread calling InstantDestroy().
		virtual bool CanBeDestroyed();

        // Destroys the object. Must be called only if IsToBeDestroyed() and CanBeDestroyed() returned True.
        inline void InstantDestroy()
		{
			AKASSERT( IsToBeDestroyed() && CanBeDestroyed() );
			// Destroys itself.
			AkDelete( CAkStreamMgr::GetObjPoolID(), this );
		}
		// Destroys a stream even if it was not destroyed by its owner. 
		// However, it still has to wait for transfers to complete: returns false if it has to wait,
		// true if it was destroyed. If it has to wait, this function must be called again later.
		virtual bool ForceInstantDestroy() = 0;

		// Sets the object in error state.
		virtual void Kill() = 0;

		// This is called when file size is set after deferred open. Stream object implementations may
		// perform any update required after file size was set. Does nothing by default.
		virtual void OnSetFileSize() {}

        // Settings access.
        inline AkStmType StmType()      // Task stream type.
        {
            return m_eStmType;
        }
        inline bool IsWriteOp()         // Task operation type (read or write).
        {
            return m_bIsWriteOp;
        }
        inline AkPriority Priority()    // Priority.
        {
            AKASSERT( m_priority >= AK_MIN_PRIORITY &&
                    m_priority <= AK_MAX_PRIORITY );
            return m_priority;
        }

        // Get information for synchronous data transfer.
		// Returns true if the transfer needs to be done, false if it should be cancelled.
        virtual bool TransferInfo( 
            AkFileDesc *& out_pFileDesc,    // Stream's associated file descriptor.
            AkUInt64 &  out_uPosition,		// Position in file (absolute).
			AkUInt32 &  out_uBufferSize,	// Buffer size available for transfer.
			AkUInt32 &  out_uRequestSize	// Exact transfer size.
            )
		{
			AKASSERT( !"Not implemented" );
			return false;
		}
		// Asynchronous counterpart for async devices.
		// Returns NULL if the transfer needs to be cancelled.
		virtual AkAsyncIOTransferInfo * TransferInfo(
			void *				in_pBuffer,		// Buffer for transfer.
			AkFileDesc *&		out_pFileDesc	// Stream's associated file descriptor.
			)
		{
			AKASSERT( !"Not implemented" );
			return NULL;
		}
        // Update stream object after I/O.
        virtual void Update(
			void *		in_pCookie,			// Cookie for device specific usage: generally a transfer object.
			const AkUInt64 in_uPosition,	// Absolute file position of transfer.
            void *      in_pBuffer,         // Address of data.
			AkUInt32    in_uActualIOSize,	// Size available for writing/reading.
			AKRESULT	in_eIOResult		// AK_Success if IO was successful, AK_Cancelled if IO was cancelled, AK_Fail otherwise.
            ) = 0;

        // Data buffer access.
        // Tries to get a buffer for I/O. It might fail if there is no more memory (automatic streams)
        // or if the operation was cancelled (standard streams). In such a case it returns NULL.
        // Returns the address of the user's buffer.
        virtual void * TryGetIOBuffer() = 0;
        // For buffer reassignment. The scheduler calls this if there is no memory available and it considers
        // that this task could give away some of its buffers.
        // Automatic stream specific. Not implemented in standard stream objects.
        // Returns NULL if failed, and notifies that memory management should become idle.
        // Fails if the user got all buffers before the scheduler.
        virtual void * PopIOBuffer() = 0;

		// Returns True if the task should be considered by the scheduler for an I/O operation.
		// - Standard streams are ready for I/O when they are Pending.
        // - Automatic streams are ready for I/O when they are Running and !EOF.
		inline bool ReadyForIO()
		{
			return m_bIsReadyForIO;
		}

		// Returns True if the task requires scheduling (tasks requiring scheduling have priority,
		// regardless of their deadline).
        inline bool RequiresScheduling()
		{
			return m_bRequiresScheduling;
		}

        // Scheduling heuristics.
        virtual AkReal32 EffectiveDeadline() = 0;   // Compute task's effective deadline for next operation, in ms.
        AkReal32 TimeSinceLastTransfer(             // Time elapsed since last I/O transfer.
            const AkInt64 & in_liNow                // Time stamp.
            )
        {
            return AKPLATFORM::Elapsed( in_liNow, m_iIOStartTime );
        }

        // Profiling.
#ifndef AK_OPTIMIZED
        
        // IAkStreamProfile interface.
        // ---------------------------
        virtual bool IsNew()                        // Returns true if stream is tagged "New".
        {
            return m_bIsNew;
        }
        virtual void ClearNew()                     // Clears stream's "New" tag.
        {
            m_bIsNew = false;
        }
        virtual void GetStreamRecord( 
            AkStreamRecord & out_streamRecord
            );
        // ---------------------------

        inline bool IsProfileNew()                  // Returns true if stream is tagged "New".
        {
            return m_bIsNew;
        }
		inline bool IsProfileReady()
		{
			return m_bIsFileOpen;					// Ready to be profiled when Low-Level Open succeeded.
		}
        inline AK::IAkStreamProfile * GetStreamProfile()    // Returns associated stream profile interface.
        {
            return this;
        }
        inline void SetStreamID(                    // Assigns a stream ID used by profiling.
            AkUInt32 in_uStreamID 
            )
        {
            m_uStreamID = in_uStreamID;
        }
        inline bool ProfileIsToBeDestroyed()        // True when the stream has been scheduled for destruction.
        {
            return m_bIsToBeDestroyed;
        }
        virtual void ProfileAllowDestruction() = 0;	// Signals that stream can be destroyed.
        
#endif

		// List bare light sibling: device's TaskList.
		CAkStmTask * pNextLightItem;

	protected:

		// Helpers.

		inline void SetToBeDestroyed()
		{
			m_bIsToBeDestroyed = true;
			SetReadyForIO( false );
		}

		// Set "Ready for I/O" status.
        inline void SetReadyForIO( bool in_bReadyForIO )
		{
			m_bIsReadyForIO = in_bReadyForIO;
		}

		// Returns file offset in bytes.
		inline AkUInt64 GetFileOffset()
		{
			return m_fileDesc.uSector * m_uLLBlockSize;
		}

		// Returns absolute position of end of file, in bytes.
		inline AkUInt64 GetFileEndPosition()
		{
			return m_fileDesc.iFileSize + m_fileDesc.uSector * m_uLLBlockSize;
		}

		void FreeDeferredOpenData();

        // Common attributes.
        // ------------------
    protected:
        
		CAkStmTask();
		CAkStmTask(CAkStmTask&);
		~CAkStmTask();
        
		// Deferred open data.
		struct DeferredOpenData
		{
			union
			{
				AkOSChar *		pszFileName;
				AkFileID		fileID;
			};
			AkFileSystemFlags	flags;
			AkOpenMode			eOpenMode;
			AkUInt32			bByString :1;
			AkUInt32			bUseFlags;
		};
		DeferredOpenData *	m_pDeferredOpenData;// Deferred open data. NULL when no deferred opening required.

        // File info.
        AkFileDesc          m_fileDesc;         // File descriptor:
                                                // uFileSize: File size.
                                                // uSector: Position of beginning of file (relative to handle).
                                                // hFile: System handle.
                                                // Custom parameter and size (owned by Low-level).
                                                // Device ID.
        CAkLock				m_lockStatus;       // Lock for status integrity.
        AkInt64		        m_iIOStartTime;     // Time when I/O started. 
        CAkDeviceBase *     m_pDevice;          // Access to owner device.
        wchar_t *           m_pszStreamName;    // User defined stream name.   
        AkUInt32            m_uBufferSize;      // Remaining size to fill. 
        AkUInt32            m_uLLBlockSize;     // Low-level IO block size (queried once at init).

        // Profiling.
#ifndef AK_OPTIMIZED
        AkUInt32            m_uStreamID;        // Profiling stream ID.
        AkUInt32            m_uBytesTransfered; // Number of bytes transferred (replace).
#endif

        AkPriority          m_priority;         // IO priority. Keeps last operation's priority.

        AkStmType           m_eStmType      :2; // Stream type. 2 types, avoid sign bit.
        AkUInt32            m_bIsWriteOp    :1; // Operation type (automatic streams are always reading).
        AkUInt32            m_bHasReachedEof    :1; // True when file pointer reached eof.
        AkUInt32            m_bIsToBeDestroyed  :1; // True when this stream is scheduled to be destroyed.
        AkUInt32			m_bIsFileOpen	:1;	// False while Low-Level IO open is pending.
		AkUInt32            m_bRequiresScheduling	:1; // Stream's own indicator saying if it counts in the scheduler semaphore.
	private:
		AkUInt32            m_bIsReadyForIO	:1;	// True when task is in a state where it is ready for an I/O transfer (distinct from Deadline-based status).
	
	protected:
        // Profiling.
#ifndef AK_OPTIMIZED
        AkUInt32            m_bIsNew        :1; // "New" flag.
        AkUInt32            m_bIsProfileDestructionAllowed  :1; // True when profiler gave its approbation for destruction.
#endif
                                
    };

    //-----------------------------------------------------------------------------
    // Name: class CAkStmBase
    // Desc: Base implementation for standard streams.
    //-----------------------------------------------------------------------------
    class CAkStdStmBase : public CAkStmTask,
						  public AK::IAkStdStream
    {
    public:

        // Construction/destruction.
        CAkStdStmBase();
        virtual ~CAkStdStmBase();

        AKRESULT Init(
            CAkDeviceBase *     in_pDevice,         // Owner device.
            const AkFileDesc &  in_fileDesc,        // File descriptor.
            AkOpenMode          in_eOpenMode        // Open mode.
            );

        //-----------------------------------------------------------------------------
        // AK::IAkStdStream interface.
        //-----------------------------------------------------------------------------

        // Stream info access.
        virtual void      GetInfo(
            AkStreamInfo &      out_info        // Returned stream info.
            );
        // Name the stream (appears in Wwise profiler).
        virtual AKRESULT  SetStreamName(
            const wchar_t*      in_pszStreamName    // Stream name.
            );
        // Returns I/O block size.
        virtual AkUInt32  GetBlockSize();       // Returns block size for optimal/unbuffered IO.
        
        // Operations.
        // ---------------------------------------
        
        // Read/Write.
        // Ask for a multiple of the device's atomic block size, 
        // obtained through IAkStdStream::GetBlockSize().
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
        
        // Get current stream position.
        virtual AkUInt64 GetPosition( 
            bool *          out_pbEndOfStream   // Input streams only. Can pass NULL.
            );
        // Set stream position. Modifies position of next read/write.
        virtual AKRESULT SetPosition(
            AkInt64         in_iMoveOffset,     // Seek offset.
            AkMoveMethod    in_eMoveMethod,     // Seek method, from beginning, end or current file position.
            AkInt64 *       out_piRealOffset    // Actual seek offset may differ from expected value when unbuffered IO.
                                                // In that case, floors to sector boundary. Pass NULL if don't care.
            );
        
        /// Query user data and size.
        virtual void * GetData( 
            AkUInt32 &      out_uSize           // Size actually read or written.
            );
        // Status.
        virtual AkStmStatus GetStatus();        // Get operation status.

        //-----------------------------------------------------------------------------
        // CAkStmTask interface.
        //-----------------------------------------------------------------------------

		// Destroys a stream even if it was not destroyed by its owner. 
		// However, it still has to wait for transfers to complete: returns false if it has to wait,
		// true if it was destroyed. If it has to wait, this function must be called again later.
		virtual bool ForceInstantDestroy();

		// Sets the object in error state.
		virtual void Kill();

        // Automatic streams specific: does not apply.
        virtual void * PopIOBuffer();

        //-----------------------------------------------------------------------------
        // Profiling.
        //-----------------------------------------------------------------------------
#ifndef AK_OPTIMIZED
        
        // IAkStreamProfile interface.
        virtual void GetStreamData(
            AkStreamData &   out_streamData
            );

		// Signals that stream can be destroyed.
		virtual void ProfileAllowDestruction();
#endif

        //-----------------------------------------------------------------------------
        // Helpers.
        //-----------------------------------------------------------------------------
	protected:

		// Execute Operation (either Read or Write).
		AKRESULT ExecuteOp(
			bool			in_bWrite,			// Read (false) or Write (true).
			void *          in_pBuffer,         // User buffer address. 
			AkUInt32        in_uReqSize,        // Requested write size. 
			bool            in_bWait,           // Block until operation is complete.
			AkPriority      in_priority,        // Heuristic: operation priority.
			AkReal32        in_fDeadline,       // Heuristic: operation deadline (s).
			AkUInt32 &      out_uSize           // Size actually written.
			);

        // Set task status. Increment and release Std semaphore.
		// Note: Status must be locked prior to calling this function.
        void SetStatus(
            AkStmStatus in_eStatus              // New status.
            );

		// Stream type specific policies.
		// ------------------------------
		
		// Update stream's position, attach or flush IO buffer.
		// Sync: Status must be locked prior to calling this function. 
		// Returns the byte offset by which the stream position was incremented therein 
		// (usually equals in_uActualIOSize unless there was an error).
		AkUInt32 UpdatePosition( 
			const AkUInt64 & in_uPosition,		// Absolute file position of transfer.
			void *      in_pBuffer,             // Address of data.
			AkUInt32    in_uActualIOSize,       // Size available for writing/reading.
			bool		in_bStoreData			// Store data in stream object if true, free buffer otherwise.
			);

		// Update task's status after transfer.
		void UpdateTaskStatus(
			AKRESULT	in_eIOResult			// AK_Success if IO was successful, AK_Cancelled if IO was cancelled, AK_Fail otherwise.
			);

		// Notify stream when a transfer is removed.
		// Standard streams have nothing to do (they have don't keep a virtual buffering size).
		void OnTransferRemoved( 
			const AkUInt64 & in_uExpectedPosition,	// Expected file position (absolute) after request.
			const AkUInt64 & in_uActualPosition		// Actual file position after request.
			)
		{ }

    protected:

		AkUInt64            m_uFilePosition;    // File position (absolute).
		AkUInt64            m_uCurPosition;     // Stream position (relative to beginning of file).

        void *              m_pBuffer;          // Buffer for IO.
        AkUInt32            m_uActualSize;      // Actual size read/written.
        AkReal32            m_fDeadline;        // Deadline. Keeps last operation's deadline.
        
        // Stream settings.
        AkOpenMode          m_eOpenMode     :3; // Either input (read), output (write) or both. 4 values, avoid sign bit.
        
        // Operation info.
        AkStmStatus         m_eStmStatus    :4; // Stream operation status. 5 values, avoid sign bit.

		AkUInt32            m_bIsPositionDirty  :1; // Dirty flag for position (between saved ulCurPosition and file pointer).
    };

    //-----------------------------------------------------------------------------
    // Name: class CAkAutoStmBase
    // Desc: Base automatic stream implementation.
    //-----------------------------------------------------------------------------
    class CAkAutoStmBase : public CAkStmTask,
						   public AK::IAkAutoStream
    {
    public:
    
    	// Construction/destruction.
        CAkAutoStmBase();
        virtual ~CAkAutoStmBase();

        AKRESULT Init( 
            CAkDeviceBase *             in_pDevice,         // Owner device.
            const AkFileDesc &          in_pFileDesc,       // File descriptor.
            const AkAutoStmHeuristics & in_heuristics,      // Streaming heuristics.
            AkAutoStmBufSettings *      in_pBufferSettings, // Stream buffer settings. Pass NULL to use defaults (recommended).
            AkUInt32                    in_uGranularity     // Device's I/O granularity.
            );

        //-----------------------------------------------------------------------------
        // AK::IAkAutoStream interface.
        //-----------------------------------------------------------------------------

        // Closes stream. The object is destroyed and the interface becomes invalid.
        virtual void      Destroy();

        // Stream info access.
        virtual void      GetInfo(
            AkStreamInfo &      out_info        // Returned stream info.
            );
        // Stream heuristics access.
        virtual void      GetHeuristics(
            AkAutoStmHeuristics & out_heuristics// Returned stream heuristics.
            );
        // Stream heuristics run-time change.
        virtual AKRESULT  SetHeuristics(
            const AkAutoStmHeuristics & in_heuristics   // New stream heuristics.
            );
        // Name the stream (appears in Wwise profiler).
        virtual AKRESULT  SetStreamName(
            const wchar_t*      in_pszStreamName    // Stream name.
            );
        // Returns I/O block size.
        virtual AkUInt32  GetBlockSize();


        // Operations.
        // ---------------------------------------
        
        // Starts automatic scheduling.
        virtual AKRESULT Start();
        // Stops automatic scheduling.
        virtual AKRESULT Stop();

        // Get stream position.
        virtual AkUInt64 GetPosition( 
            bool *          out_pbEndOfStream   // Set to true if reached end of stream. Can pass NULL.
            );   
        // Set stream position. Modifies position in stream for next read user access.
        virtual AKRESULT SetPosition(
            AkInt64         in_iMoveOffset,     // Seek offset.
            AkMoveMethod    in_eMoveMethod,     // Seek method, from beginning, end or current file position.
            AkInt64 *       out_piRealOffset    // Actual seek offset may differ from expected value when unbuffered IO.
                                                // In that case, floors to sector boundary. Pass NULL if don't care.
            );

        // Data/status access. 
        // -----------------------------------------

        // GetBuffer.
        // Return values : 
        // AK_DataReady     : if buffer was granted.
        // AK_NoDataReady   : if buffer was not granted yet.
        // AK_NoMoreData    : if buffer was granted but reached end of file (next call will return with size 0).
        // AK_Fail          : there was an IO error.
        virtual AKRESULT GetBuffer(
            void *&         out_pBuffer,        // Address of granted data space.
            AkUInt32 &      out_uSize,          // Size of granted data space.
            bool            in_bWait            // Block until data is ready.
            );

        // Release buffer granted through GetBuffer().
        virtual AKRESULT ReleaseBuffer();

		// Get the amount of buffering that the stream has. 
		// Returns
		// - AK_DataReady	: Some data has been buffered (out_uNumBytesAvailable is greater than 0).
		// - AK_NoDataReady	: No data is available, and the end of file has not been reached.
		// - AK_NoMoreData	: No data is available, but the end of file has been reached. There will not be any more data.
		// - AK_Fail		: The stream is invalid due to an I/O error.
		virtual AKRESULT QueryBufferingStatus( 
			AkUInt32 & out_uNumBytesAvailable 
			);

		// Returns the target buffering size based on the throughput heuristic.
		virtual AkUInt32 GetNominalBuffering();

        //-----------------------------------------------------------------------------
        // CAkStmTask interface.
        //-----------------------------------------------------------------------------

		// Destroys a stream even if it was not destroyed by its owner. 
		// However, it still has to wait for transfers to complete: returns false if it has to wait,
		// true if it was destroyed. If it has to wait, this function must be called again later.
		virtual bool ForceInstantDestroy();

		// Sets the object in error state.
		virtual void Kill();

		// This is called when file size is set after deferred open. Stream object implementations may
		// perform any update required after file size was set. 
		// Automatic stream object ensure that the loop end heuristic is consistent.
		virtual void OnSetFileSize();

        // Data buffer access.
        // Tries to get a buffer for I/O. It might fail if there is no more memory (returns NULL).
        // Returns the address of the user's buffer.
        virtual void * TryGetIOBuffer();
        
        // Scheduling heuristics.
        virtual AkReal32 EffectiveDeadline();   // Compute task's effective deadline for next operation, in ms.
        
#ifndef AK_OPTIMIZED
        // IAkStreamProfile interface.
        //----------------------------
        virtual void GetStreamData(
            AkStreamData & out_streamData
            );

		// Signals that stream can be destroyed.
		virtual void ProfileAllowDestruction();
#endif

	protected:
        //-----------------------------------------------------------------------------
        // Helpers.
        //-----------------------------------------------------------------------------
		bool NeedsBuffering(
			AkUInt32 in_uVirtualBufferingSize
			);

		inline void SetRunning( bool in_bRunning )
		{
			m_bIsRunning = in_bRunning;
			SetReadyForIO( in_bRunning && !m_bHasReachedEof && !m_bIsToBeDestroyed );
		}

		inline void SetReachedEof( bool in_bHasReachedEof )
		{
			m_bHasReachedEof = in_bHasReachedEof;
			SetReadyForIO( m_bIsRunning && !in_bHasReachedEof && !m_bIsToBeDestroyed );
		}

		// Returns true if the writer thread is done feeding this stream with data.
		// NOTE: Since stream objects do not keep an explicit count of their actual buffering 
		// (that is, the amount of data which transfers have completed, which is the sum of 
		// uDataSize of all their AkStmBuffers), callers must compute it and pass it to this function. 
		// It is compared to the virtual buffering size, which represents the amount of data for 
		// transfers that have been _scheduled_ (including those that have completed).
		inline bool NeedsNoMoreTransfer( AkUInt32 in_uActualBufferingSize )
		{
			return !RequiresScheduling() && ( m_uVirtualBufferingSize <= in_uActualBufferingSize );
		}

		// Automatic streams must implement a method that returns the file position after the last
		// valid (non cancelled) pending transfer. If there is no transfer pending, then it is the position
		// at the end of buffering.
		virtual AkUInt64 GetVirtualFilePosition() = 0;

		// Cancel pending transfers.
		// Skip buffers whose position is smaller than in_uMaxKeepPosition. Flush all the rest
		// (flush prefetched loops). Specify 0 to flush all.
		virtual void CancelPendingTransfers(
			const AkUInt64 in_uMaxKeepPosition	// Request position under which requests should not be flushed.
			) = 0;
		
		// Cancel last (most recent) pending transfer.
		// Returns true if and only if there was a transfer pending that could be cancelled.
		virtual bool CancelLastTransfer() = 0;

		// Position management.
        void ForceFilePosition(
            const AkUInt64 in_uNewPosition		// New stream position (absolute).
            );

        // Scheduling status management.
        void UpdateSchedulingStatus();

        // Returns a buffer filled with data. NULL if no data is ready.
        void *   GetReadBuffer(     
            AkUInt32 &  out_uSize               // Buffer size.
            );
        // Releases the latest buffer granted to user. Returns AK_Fail if no buffer was granted.
        AKRESULT ReleaseReadBuffer();
        // Flushes all stream buffers that are not currently granted.
        void Flush();

#ifdef _DEBUG
		virtual void CheckVirtualBufferingConsistency() = 0;
#define CHECK_BUFFERING_CONSISTENCY()	CheckVirtualBufferingConsistency()
#else
#define CHECK_BUFFERING_CONSISTENCY()
#endif
        
		// Stream type specific policies.
		// ------------------------------
		
		// Update stream's position, attach or flush IO buffer.
		// Sync: Status must be locked prior to calling this function. 
		// Returns the byte offset by which the stream position was incremented therein 
		// (usually equals in_uActualIOSize unless there was an error).
		AkUInt32 UpdatePosition( 
			const AkUInt64 & in_uPosition,		// Absolute file position of transfer.
			void *      in_pBuffer,             // Address of data.
			AkUInt32    in_uActualIOSize,       // Size available for writing/reading.
			bool		in_bStoreData			// Store data in stream object if true, free buffer otherwise.
			);

		// Update task's status after transfer.
		void UpdateTaskStatus(
			AKRESULT	in_eIOResult			// AK_Success if IO was successful, AK_Cancelled if IO was cancelled, AK_Fail otherwise.
			);

		// Notify stream when a transfer is removed.
		// Correct the virtual buffering size after transfer occurred.
		void OnTransferRemoved( 
			const AkUInt64 in_uExpectedPosition,	// Expected file position (absolute) after request.
			const AkUInt64 in_uActualPosition	// Actual file position after request.
			);
        
    protected:

		AkUInt64			m_uNextExpectedUserPosition;	// Expected (absolute) position of next GetBuffer().
        
        // Stream heuristics.
        AkReal32            m_fThroughput;      // Average throughput in bytes/ms. 
        AkUInt32            m_uLoopStart;       // Set to start of loop (byte offset from beginning of stream) for streams that loop, 0 otherwise.
        AkUInt32            m_uLoopEnd;         // Set to end of loop (byte offset from beginning of stream) for streams that loop, 0 otherwise.
        AkUInt32            m_uMinNumBuffers;   // Specify a minimal number of buffers if you plan to own more than one buffer at a time, 0 or 1 otherwise.

        // Streaming buffers.
        AkUInt32            m_uVirtualBufferingSize;	// Virtual buffering size: sum of all buffered data and pending transfers, minus what is granted to client.
														// Used for fast scheduling (minimizes scheduler computation and locking).

		AkBufferList		m_listBuffers;
        AkUInt8             m_uNextToGrant;     // Index of next buffer to grant (this implementation supports a maximum of 255 concurrently granted buffers).

		// Helper: get next buffer to grant to client.
		inline AkStmBuffer * GetNextBufferToGrant()
		{
			AKASSERT( m_listBuffers.Length() > m_uNextToGrant );
			AkUInt32 uIdx = 0;
			AkBufferList::Iterator it = m_listBuffers.Begin();
			while ( uIdx < m_uNextToGrant )
			{
				++uIdx;
				++it;
			}
			return *it;
		}

        // Stream status.
        AkUInt8            	m_bIsRunning    :1; // Running or paused.
        AkUInt8           	m_bIOError      :1; // Stream encountered I/O error.
    };
}
}
#endif //_AK_DEVICE_BASE_H_
