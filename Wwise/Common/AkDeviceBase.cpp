//////////////////////////////////////////////////////////////////////
//
// AkDeviceBase.cpp
//
// Device implementation that is common across all IO devices.
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkDeviceBase.h"
#include "AkStreamingDefaults.h"
#include <AK/Tools/Common/AkAutoLock.h>
#include <AK/Tools/Common/AkMonitorError.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>
#include <wchar.h>
#include <stdio.h>

using namespace AK;
using namespace AK::StreamMgr;

//-------------------------------------------------------------------
// Defines.
//-------------------------------------------------------------------

#define AK_INFINITE_DEADLINE            (100000)    // 100 seconds.


//-------------------------------------------------------------------
// CAkDeviceBase implementation.
//-------------------------------------------------------------------

// Device construction/destruction.
CAkDeviceBase::CAkDeviceBase( 
	IAkLowLevelIOHook *	in_pLowLevelHook
	)
: m_pLowLevelHook( in_pLowLevelHook )
, m_streamIOPoolId( AK_INVALID_POOL_ID )
, m_pBufferMem( NULL )
#ifndef AK_OPTIMIZED
, m_streamIOPoolSize( 0 )
#endif
{
}

CAkDeviceBase::~CAkDeviceBase( )
{
}

// Init.
AKRESULT CAkDeviceBase::Init( 
    const AkDeviceSettings &	in_settings,
    AkDeviceID					in_deviceID 
    )
{
    if ( 0 == in_settings.uGranularity )
    {
        AKASSERT( !"Invalid I/O granularity: must be non-zero" );
        return AK_InvalidParameter;
    }
    if ( in_settings.uIOMemorySize && 
         in_settings.fTargetAutoStmBufferLength < 0 )
    {
        AKASSERT( !"Invalid automatic stream average buffering value" );
        return AK_InvalidParameter;
    }
	if ( in_settings.uSchedulerTypeFlags & AK_SCHEDULER_DEFERRED_LINED_UP 
		&& ( in_settings.uMaxConcurrentIO < 1 || in_settings.uMaxConcurrentIO > 1024 ) )
    {
        AKASSERT( !"Invalid maximum number of concurrent I/O" );
        return AK_InvalidParameter;
    }

	m_listTasks.Init();
	m_listFreeBufferHolders.Init();

    m_uGranularity			= in_settings.uGranularity;
    m_fTargetAutoStmBufferLength  = in_settings.fTargetAutoStmBufferLength;
    m_uIdleWaitTime			= in_settings.uIdleWaitTime;
	m_uMaxConcurrentIO		= in_settings.uMaxConcurrentIO;

	m_deviceID				= in_deviceID;
    
    // Create stream memory pool.
    if ( in_settings.uIOMemorySize > 0 )
    {		
		m_streamIOPoolId = AK::MemoryMgr::CreatePool( 
            in_settings.pIOMemory,
			in_settings.uIOMemorySize,
			in_settings.uGranularity,
			in_settings.ePoolAttributes | AkFixedSizeBlocksMode,
			in_settings.uIOMemoryAlignment );        
    }

    if( m_streamIOPoolId != AK_INVALID_POOL_ID )
	{
        // This pool must not send error notifications: not having memory is normal and notifications are costly.
        AK::MemoryMgr::SetMonitoring(
            m_streamIOPoolId,
            false );
        AK_SETPOOLNAME( m_streamIOPoolId, L"Stream I/O" );

		// Cache buffer holder structures. 
		// The maximum amount of buffers is IO pool size / granularity.
		AkUInt32 uNumBuffers = ( in_settings.uIOMemorySize + in_settings.uGranularity - 1 ) / in_settings.uGranularity;
		m_pBufferMem = (AkStmBuffer*)AkAlloc( CAkStreamMgr::GetObjPoolID(), uNumBuffers * sizeof( AkStmBuffer ) );
		if ( !m_pBufferMem )
			return AK_Fail;

		AkStmBuffer * pBuffer = m_pBufferMem;
		AkStmBuffer * pBufferEnd = pBuffer + uNumBuffers;
		do
		{
			m_listFreeBufferHolders.AddFirst( pBuffer++ );
		}
		while ( pBuffer < pBufferEnd );
	}
    else if ( in_settings.uIOMemorySize > 0 )
    {
        AKASSERT( !"Cannot create stream pool" );
		return AK_Fail;
    }
    // otherwise, device does not support automatic streams.
	
#ifndef AK_OPTIMIZED
    m_streamIOPoolSize  = in_settings.uIOMemorySize;
    m_bIsMonitoring     = false;
    m_bIsNew            = true;
#endif

	// Create I/O scheduler thread objects.
	return CAkIOThread::Init( in_settings.threadProperties );
}

// Destroy.
void CAkDeviceBase::Destroy()
{
#ifndef AK_OPTIMIZED
    m_arStreamProfiles.Term();
#endif
	CAkIOThread::Term();

	// Free cached buffer holders.
	if ( m_pBufferMem )
	{
		m_listFreeBufferHolders.RemoveAll();
		AkFree( CAkStreamMgr::GetObjPoolID(), m_pBufferMem );
	}
	m_listFreeBufferHolders.Term();

    // Destroy IO pool.
    if ( m_streamIOPoolId != AK_INVALID_POOL_ID )
        AKVERIFY( AK::MemoryMgr::DestroyPool( m_streamIOPoolId ) == AK_Success );
    m_streamIOPoolId = AK_INVALID_POOL_ID;

    AkDelete( CAkStreamMgr::GetObjPoolID(), this );
}

// Device ID. Virtual method defined in IAkDevice.
AkDeviceID CAkDeviceBase::GetDeviceID()
{
    return m_deviceID;
}

// Destroys all streams remaining to be destroyed.
bool CAkDeviceBase::ClearStreams()
{
	TaskList::IteratorEx it = m_listTasks.BeginEx();
	while ( it != m_listTasks.End() )
	{
		CAkStmTask * pTask = (*it);
		it = m_listTasks.Erase( it );
		if ( !pTask->ForceInstantDestroy() )
		{
			m_listTasks.AddFirst( pTask );
			return false;
		}
	}
	m_listTasks.Term();
	return true;
}

// Called once when I/O thread starts.
void CAkDeviceBase::OnThreadStart()
{
	// Stamp time the first time.
    AKPLATFORM::PerformanceCounter( &m_time );
}

// Helper: adds a new task to the list.
// Sync: task list lock.
void CAkDeviceBase::AddTask( 
    CAkStmTask * in_pStmTask
    )
{
    AkAutoLock<CAkLock> gate( m_lockTasksList );
    
    m_listTasks.AddFirst( in_pStmTask );

#ifndef AK_OPTIMIZED
    // Compute and assign a new unique stream ID.
    in_pStmTask->SetStreamID(
		CAkStreamMgr::GetNewStreamID() // Gen stream ID.
        );
#endif
}

// Scheduler algorithm.
// Finds the next task for which an I/O request should be issued.
// Return: If a task is found, a valid pointer to a task is returned, as well
// as the address in/from which to perform a data transfer.
// Otherwise, returns NULL.
// Sync: 
// 1. Locks task list ("scheduler lock").
// 2. If it chooses a standard stream task, the stream becomes "I/O locked" before the scheduler lock is released.
CAkStmTask * CAkDeviceBase::SchedulerFindNextTask( 
    void *&		out_pBuffer,	// Returned I/O buffer used for this transfer.
	AkReal32 &	out_fOpDeadline	// Returned deadline for this transfer.
    )
{
    // Start scheduling.
    // ------------------------------------
	
	// Lock tasks list.
    AkAutoLock<CAkLock> scheduling( m_lockTasksList );

    // Stamp time.
    AKPLATFORM::PerformanceCounter( &m_time );

    // If m_bDoWaitMemoryChange, no automatic stream operation can be scheduled because memory is full
    // and will not be reassigned until someone calls NotifyMemChange().
    // Therefore, we only look for a pending standard stream (too bad if memory is freed in the meantime).
    if ( CannotScheduleAutoStreams() )
        return ScheduleStdStmOnly( out_pBuffer, out_fOpDeadline );

    TaskList::IteratorEx it = m_listTasks.BeginEx();
    CAkStmTask * pTask = NULL;
	CAkStmTask * pMostBufferedTask = NULL;
	AkReal32 fSmallestDeadline;
	AkReal32 fGreatestDeadlineAmongAutoStms = 0;
	bool bLeastBufferedTaskRequiresScheduling = false;
    
    // Get first valid task's values for comparison.
    while ( it != m_listTasks.End() )
    {
		// Verify that we can perform I/O on this one.
        if ( (*it)->IsToBeDestroyed() )
        {
			if ( (*it)->CanBeDestroyed() )
			{
				// Clean up.
				CAkStmTask * pTaskToDestroy = (*it);
	            it = m_listTasks.Erase( it );
				pTaskToDestroy->InstantDestroy();
	        }
			else 
			{
				// Not ready to be destroyed: wait until next turn.
				++it;
			}
        }
        else if ( !(*it)->ReadyForIO() )
            ++it;   // Skip this one.
        else
        {
            // Current iterator refers to a task that is not scheduled to be destroyed, and is ready for I/O. Proceed.
			// pTask and fSmallestDeadline are the current task and associated effective deadline chosen for I/O.
            pTask = (*it);
			fSmallestDeadline = pTask->EffectiveDeadline();
			bLeastBufferedTaskRequiresScheduling = pTask->RequiresScheduling();

			// pMostBufferedTask and fGreatestDeadlineAmongAutoStms are used to determine which task is going to give a buffer away
			// if we lack memory. They apply to automatic streams only,
			if ( pTask->StmType() == AK_StmTypeAutomatic )
			{
				pMostBufferedTask = pTask;
				fGreatestDeadlineAmongAutoStms = fSmallestDeadline;
			}
			break;  
        }
    }    

    if ( !pTask )
    {
        // No task was ready for I/O. Leave.
        return NULL;
    }

    // Find task with smallest effective deadline.
    // If a task has a deadline equal to 0, this means we are starving; user throughtput is greater than
    // low-level bandwidth. In that situation, starving streams are chosen according to their priority.
    // If more than one starving stream has the same priority, the scheduler chooses the one that has been 
    // waiting for I/O for the longest time.
    // Note 1: This scheduler does not have any idea of the actual low-level throughput, nor does it try to
    // know it. It just reacts to the status of its streams at a given moment.
    // Note 2: By choosing the highest priority stream only when we encounter starvation, we take the bet
    // that the transfer will complete before the user has time consuming its data. Therefore it remains 
    // possible that high priority streams starve.
    // Note 3: Automatic streams that just started are considered starving. They are chosen according to
    // their priority first, in a round robin fashion (starving mechanism).
    // Note 4: If starving mode lasts for a long time, low-priority streams will stop being chosen for I/O.
	// Note 5: Tasks that are actually signaled (RequireScheduling) have priority over other tasks. A task 
	// that is unsignaled may still have a smaller deadline than other tasks (because tasks must be double-
	// buffered at least). However, an unsignaled task will only be chosen if there are no signaled task.

    // Start with the following task.
    AKASSERT( pTask );
    ++it;

    while ( it != m_listTasks.End() )
    {
        // Verify that we can perform I/O on this one.
        if ( (*it)->IsToBeDestroyed() )
        {
			if ( (*it)->CanBeDestroyed() )
			{
				// Clean up.
				CAkStmTask * pTaskToDestroy = (*it);
	            it = m_listTasks.Erase( it );
				pTaskToDestroy->InstantDestroy();
	        }
			else 
			{
				// Not ready to be destroyed: wait until next turn.
				++it;
			}
        }
        else if ( (*it)->ReadyForIO() )
        {
			AkReal32 fDeadline = (*it)->EffectiveDeadline();

			if ( !bLeastBufferedTaskRequiresScheduling && (*it)->RequiresScheduling() )
			{
				// This is the first task that we run into which actually requires action from 
				// scheduler: pick it. 
				pTask = (*it);
				fSmallestDeadline = fDeadline;
				bLeastBufferedTaskRequiresScheduling = true;
			}
			else if ( !bLeastBufferedTaskRequiresScheduling || (*it)->RequiresScheduling() )
			{
				if ( fDeadline == 0 )
				{
					// Deadline is zero: starvation mode.
					// Choose task with highest priority among those that are starving.
					if ( (*it)->Priority() > pTask->Priority() || fSmallestDeadline > 0 )
					{
						pTask = (*it);
						fSmallestDeadline = fDeadline;
						bLeastBufferedTaskRequiresScheduling = pTask->RequiresScheduling();
					}
					else if ( (*it)->Priority() == pTask->Priority() )
					{
						// Same priority: choose the one that has waited the most.
						if ( (*it)->TimeSinceLastTransfer( GetTime() ) > pTask->TimeSinceLastTransfer( GetTime() ) )
						{
							pTask = (*it);
							fSmallestDeadline = fDeadline;
							bLeastBufferedTaskRequiresScheduling = pTask->RequiresScheduling();
						}
					}
				}
				else if ( fDeadline < fSmallestDeadline )
				{
					// Deadline is not zero: low-level has enough bandwidth. Just take the task with smallest deadline.
					// We take the bet that this transfer will have time to occur fast enough to properly service
					// the others on next pass.
					pTask = (*it);
					fSmallestDeadline = fDeadline;
					bLeastBufferedTaskRequiresScheduling = pTask->RequiresScheduling();
				}
			}

            // Keep track of automatic stream tasks that have a great deadline, for eventual buffer reassignment.
            if ( (*it)->StmType() == AK_StmTypeAutomatic &&
                 fDeadline > fGreatestDeadlineAmongAutoStms )
            {
                fGreatestDeadlineAmongAutoStms = fDeadline;
                pMostBufferedTask = (*it);
            }

            ++it;
        }
        else
            ++it;   // Skip this task: it is not waiting for I/O.
	}
	
	out_fOpDeadline = fSmallestDeadline;

	// Bail out now if the chosen task doesn't actually needs buffering, and we are not using the uIdleWaitTime
	// feature (the one that allows the device to stream in data during its free time - usually used when there
	// is a lot of streaming memory).
	if ( !bLeastBufferedTaskRequiresScheduling && !CanOverBuffer() )
		return NULL;

    // Standard streams:
    // ------------------------------
    if ( pTask->StmType() == AK_StmTypeStandard )
    {
        // Standard streams may not return a buffer if the operation was cancelled while we were scheduling.
        // IMPORTANT: If this method succeeds (returns a buffer), the task will lock itself for I/O (AkStdStmBase::m_lockIO).
        // All operations that need to wait for I/O to complete block on that lock.
        out_pBuffer = pTask->TryGetIOBuffer();
        if ( !out_pBuffer )
            return NULL;    // Task cancelled or destroyed by user. Return NULL to cancel I/O.
        return pTask;
    }
    
    // Automatic streams:
    // ------------------------------

    // Automatic streams' TryGetIOBuffer() must be m_lockAutoSems protected, because it tries to allocate a 
    // buffer from the memory manager.
    // If it fails, and we decide not to reassign a buffer from another automatic stream, the automatic 
    // streams semaphore should be notified as "memory idle" (inhibates the semaphore). 
    // Memory allocation and semaphore inhibition must be atomic, in case someone freed memory in the meantime.
    // Since most of the time this situation does not happen, we first try getting memory without locking.

    // Try allocate a buffer.
	{
		AkAutoLock<CAkIOThread> lock( *this );
	    out_pBuffer = pTask->TryGetIOBuffer();
	}
    if ( !out_pBuffer )
    {
        // No buffer is available. Remove a buffer from the most buffered task unless one of the following 
        // conditions is met:
        // - The most buffered task is also the one that was chosen for I/O.
		// - The chosen task is signaled.
        // - Its deadline is greater than the target buffering length.

        // If we decide not to remove a buffer, inhibate automatic stream semaphore (NotifyMemIdle).

        if ( CanOverBuffer() &&
			 bLeastBufferedTaskRequiresScheduling &&
			 pMostBufferedTask &&
			 pTask != pMostBufferedTask &&
             fGreatestDeadlineAmongAutoStms > m_fTargetAutoStmBufferLength )
        {
            // Remove a buffer from the most buffered task.
            // Note 1. PopIOBuffer() does not free memory, it just removes a buffer from its table and passes it back. 
            // Note 2. Bad citizens could be harmful to this algorithm. A stream could decide
            // not to give a buffer just because its user owns too much at a time. Technically, we could seek the next
            // "most buffered" stream, but we prefer not.
            // Note 3. If PopIOBuffer() fails returning a buffer, we must call NotifyMemIdle(). However, since PopIOBuffer()
            // needs to lock its status to manipulate its array of buffers, NotifyMemIdle() is called from within to
            // avoid potential deadlocks.
            
            out_pBuffer = pMostBufferedTask->PopIOBuffer();
            if ( !out_pBuffer )
            {
                // The stream would not let go one of its buffers. Sleep (NotifyMemIdle() was called from PopIOBuffer()).
                return NULL;
            }
        }
        else
        {
            // We decided not to reassign a buffer. Sleep until a buffer is freed.
            // IMPORTANT: However, allocation and notification must be atomic, otherwise the scheduler could erroneously
            // think that there is no memory in the case a buffer was freed after our failed attempt to allocate one.
            // Therefore, we lock, try to allocate again, perform I/O if it succeeds, notify that memory is idle otherwise.
			AkAutoLock<CAkIOThread> lock( *this );
            
			out_pBuffer = pTask->TryGetIOBuffer(); 
            if ( !out_pBuffer )
			{
                NotifyMemIdle();

				// Starting now, automatic streams will not trigger I/O thread awakening until a change occurs with memory.
				// Pending standard streams will continue notifying, so the thread might come back and execute a
				// standard stream. 
				return NULL;
			}
            return pTask;
        }
    }

    return pTask;
}

// Scheduler algorithm: standard stream-only version.
// Finds next task among standard streams only (typically when there is no more memory).
// Note: standard streams that are ready for IO are always signaled.
CAkStmTask * CAkDeviceBase::ScheduleStdStmOnly(
    void *&		out_pBuffer,	// Returned I/O buffer used for this transfer.
	AkReal32 &	out_fOpDeadline	// Returned deadline for this transfer.
    )
{
    TaskList::IteratorEx it = m_listTasks.BeginEx();
    CAkStmTask * pTask = NULL;
    
    // Get first valid task's values for comparison.
    while ( it != m_listTasks.End() )
    {
        // Verify that we can perform I/O on this one.
        if ( (*it)->IsToBeDestroyed() )
        {
            if ( (*it)->CanBeDestroyed() )
			{
				// Clean up.
				CAkStmTask * pTaskToDestroy = (*it);
	            it = m_listTasks.Erase( it );
				pTaskToDestroy->InstantDestroy();
	        }
			else 
			{
				// Not ready to be destroyed: wait until next turn.
				++it;
			}
        }
        else if ( (*it)->StmType() == AK_StmTypeStandard &&
                    (*it)->ReadyForIO() )
        {
            // Current iterator refers to a standard stream task that is not scheduled to be destroyed, 
            // and that is pending. Proceed.
            pTask = (*it);
            break;
        }
        else
            ++it;
    }    

    if ( !pTask )
    {
        // No task was ready for I/O. Leave.
        return NULL;
    }

    // fSmallestDeadline is the smallest effective deadline found to date. Used to find the next task for I/O.
    AkReal32 fSmallestDeadline = pTask->EffectiveDeadline();
    
    // Find task with smallest effective deadline.
    // See note in SchedulerFindNextTask(). It is the same algorithm, except that automatic streams are excluded.
    
    // Start with the following task.
    AKASSERT( pTask );
    ++it;

    while ( it != m_listTasks.End() )
    {
        // Verify that we can perform I/O on this one.
        if ( (*it)->IsToBeDestroyed() )
        {
            if ( (*it)->CanBeDestroyed() )
			{
				// Clean up.
				CAkStmTask * pTaskToDestroy = (*it);
	            it = m_listTasks.Erase( it );
				pTaskToDestroy->InstantDestroy();
	        }
			else 
			{
				// Not ready to be destroyed: wait until next turn.
				++it;
			}
        }
        else if ( (*it)->StmType() == AK_StmTypeStandard &&
                    (*it)->ReadyForIO() )
        {
            AkReal32 fDeadline = (*it)->EffectiveDeadline(); 

            if ( fDeadline == 0 )
            {
                // Deadline is zero. Starvation mode: user throughput is greater than low-level bandwidth.
                // Choose task with highest priority among those that are starving.
                if ( (*it)->Priority() > pTask->Priority() || fSmallestDeadline > 0 )
                {
                    pTask = (*it);
                    fSmallestDeadline = fDeadline;
                }
                else if ( (*it)->Priority() == pTask->Priority() )
                {
                    // Same priority: choose the one that has waited the most.
                    if ( (*it)->TimeSinceLastTransfer( GetTime() ) > pTask->TimeSinceLastTransfer( GetTime() ) )
                    {
                        pTask = (*it);
                        fSmallestDeadline = fDeadline;
                    }
                }
            }
            else if ( fDeadline < fSmallestDeadline )
            {
                // Deadline is not zero: low-level has enough bandwidth. Just take the task with smallest deadline.
                // We take the bet that this transfer will have time to occur fast enough to properly service
                // the others.
                pTask = (*it);
                fSmallestDeadline = fDeadline;
            }
            ++it;
        }
        else
            ++it;   // Skip this task; not waiting for I/O.
    }

    // Lock task.
    AKASSERT( pTask );

	out_fOpDeadline = fSmallestDeadline;
    
    // IMPORTANT: If this method succeeds (returns a buffer), the task will lock itself for I/O (AkStdStmBase::m_lockIO).
    // All operations that need to wait for I/O to complete block on that lock.
    out_pBuffer = pTask->TryGetIOBuffer(); 
    if ( !out_pBuffer )
        return NULL;    // Task cancelled or destroyed by user. Return NULL to cancel I/O.
    return pTask;

}

// Forces the device to clean up dead tasks. 
void CAkDeviceBase::ForceCleanup(
	bool in_bKillLowestPriorityTask,				// True if the device should kill the task with lowest priority.
	AkPriority in_priority							// Priority of the new task if applicable. Pass AK_MAX_PRIORITY to ignore.
	)
{
	AkAutoLock<CAkLock> scheduling( m_lockTasksList );

	CAkStmTask * pTaskToKill = NULL;
	TaskList::IteratorEx it = m_listTasks.BeginEx();
    while ( it != m_listTasks.End() )
    {
        // Cleanup while we're at it.
        if ( (*it)->IsToBeDestroyed() )
        {
            if ( (*it)->CanBeDestroyed() )
			{
				// Clean up.
				CAkStmTask * pTaskToDestroy = (*it);
	            it = m_listTasks.Erase( it );
				pTaskToDestroy->InstantDestroy();
	        }
			else
			{
				// Not ready to be destroyed: wait until next turn.
				++it;
			}			
        }
		// Otherwise, check if it is a candidate to be killed.
		else if ( in_bKillLowestPriorityTask
				&& ( !pTaskToKill || (*it)->Priority() < pTaskToKill->Priority() )
				&& (*it)->Priority() < in_priority
				&& (*it)->ReadyForIO() )
        {
            // Current iterator refers to a standard stream task that is not scheduled to be destroyed, 
            // and that is pending. Proceed.
            pTaskToKill = (*it);
			++it;
        }
        else
            ++it;
    }

	// Kill the task if any.
	if ( pTaskToKill )
		pTaskToKill->Kill();
}

// Device Profile Ex interface.
// --------------------------------------------------------
#ifndef AK_OPTIMIZED

// Caps/desc.
void CAkDeviceBase::GetDesc( 
    AkDeviceDesc & out_deviceDesc 
    )
{
    AKVERIFY( m_pLowLevelHook->GetDeviceDesc( out_deviceDesc ) == AK_Success );
}

bool CAkDeviceBase::IsNew( )
{
    return m_bIsNew;
}

void CAkDeviceBase::ClearNew( )
{
    m_bIsNew = false;
}


AKRESULT CAkDeviceBase::StartMonitoring( )
{
    m_bIsMonitoring = true;
    return AK_Success;
}

void CAkDeviceBase::StopMonitoring( )
{
    m_bIsMonitoring = false;
}

// Stream profiling: GetNumStreams.
// Clears profiling array.
// Inspects all streams. 
// Grants permission to destroy if scheduled for destruction and AND not new.
// Copies all stream profile interfaces into its array, that will be accessed
// by IAkStreamProfile methods.
AkUInt32 CAkDeviceBase::GetNumStreams( )
{
    m_arStreamProfiles.RemoveAll( );

    AkAutoLock<CAkLock> gate( m_lockTasksList );

    TaskList::Iterator it = m_listTasks.Begin();
    while ( it != m_listTasks.End() )
    {
        // Profiler will let the device destroy this stream if it is not "new" (was already
		// read). If it is ready to be destroyed and not new, it could be because the file could not 
		// be opened, so the client will have destroyed the stream. In such a case the profiler allow  
		if ( (*it)->ProfileIsToBeDestroyed() 
			&& ( !(*it)->IsProfileNew() 
				|| !(*it)->IsProfileReady() ) )
		{
			(*it)->ProfileAllowDestruction();
		}
        else if ( (*it)->IsProfileReady() )
        {
            // Copy into profiler list.
            m_arStreamProfiles.AddLast( (*it)->GetStreamProfile() );
        }

        ++it;
    }
    return m_arStreamProfiles.Length();
}

// Note. The following functions refer to streams by index, which must honor the call to GetNumStreams().
AK::IAkStreamProfile * CAkDeviceBase::GetStreamProfile( 
    AkUInt32    in_uStreamIndex             // [0,numStreams[
    )
{
    // Get stream profile and return.
    return m_arStreamProfiles[in_uStreamIndex];
}
#endif


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
CAkStmTask::CAkStmTask()
: m_pDeferredOpenData( NULL )
, m_pszStreamName( NULL )
#ifndef AK_OPTIMIZED
//Not set m_ulStreamID;
, m_bIsNew( true )
, m_bIsProfileDestructionAllowed( false )
, m_uBytesTransfered( 0 )
#endif
, m_bHasReachedEof( false )
, m_bIsToBeDestroyed( false )
, m_bIsFileOpen( false )
, m_bRequiresScheduling( false )
, m_bIsReadyForIO( false )
{
}
CAkStmTask::~CAkStmTask()
{
	// Cleanup in Low-Level IO.
    AKASSERT( m_pDevice != NULL );
	if ( m_bIsFileOpen )
	    AKVERIFY( m_pDevice->GetLowLevelHook()->Close( m_fileDesc ) == AK_Success );

	FreeDeferredOpenData();

	if ( m_pszStreamName )
        AkFree( CAkStreamMgr::GetObjPoolID(), m_pszStreamName );
}

AKRESULT CAkStmTask::SetDeferredFileOpen(
	const AkOSChar*				in_pszFileName,		// File name.
	AkFileSystemFlags *			in_pFSFlags,		// File system flags (can be NULL).
	AkOpenMode					in_eOpenMode		// Open mode.
	)
{
	AKASSERT( !m_pDeferredOpenData );
	m_bIsFileOpen = false;	// We try to set a deferred open command: this mean this file was not opened yet.

	m_pDeferredOpenData = (DeferredOpenData*)AkAlloc( CAkStreamMgr::GetObjPoolID(), sizeof( DeferredOpenData ) );
	if ( m_pDeferredOpenData )
	{
		// Important: Need to set this first for proper freeing in case of error.
		m_pDeferredOpenData->bByString = true;

		m_pDeferredOpenData->eOpenMode = in_eOpenMode;
		m_pDeferredOpenData->pszFileName = NULL;
		
		if ( in_pFSFlags )
		{
			m_pDeferredOpenData->bUseFlags = true;
			m_pDeferredOpenData->flags = *in_pFSFlags;
		}
		else
			m_pDeferredOpenData->bUseFlags = false;

		// Allocate string buffer for user defined stream name.
		size_t uStrLen = AKPLATFORM::OsStrLen( in_pszFileName ) + 1;
        m_pDeferredOpenData->pszFileName = (AkOSChar*)AkAlloc( CAkStreamMgr::GetObjPoolID(), (AkUInt32)sizeof(AkOSChar)*( uStrLen ) );
        if ( !m_pDeferredOpenData->pszFileName )
            return AK_Fail;

        // Copy.
		AKPLATFORM::SafeStrCpy( m_pDeferredOpenData->pszFileName, in_pszFileName, uStrLen );
		return AK_Success;
	}
			
	return AK_Fail;
}

AKRESULT CAkStmTask::SetDeferredFileOpen(
	AkFileID					in_fileID,			// File ID.
	AkFileSystemFlags *			in_pFSFlags,		// File system flags (can be NULL).
	AkOpenMode					in_eOpenMode		// Open mode.
	)
{
	AKASSERT( !m_pDeferredOpenData );
	m_bIsFileOpen = false;	// We try to set a deferred open command: this mean this file was not opened yet.

	m_pDeferredOpenData = (DeferredOpenData*)AkAlloc( CAkStreamMgr::GetObjPoolID(), sizeof( DeferredOpenData ) );
	if ( m_pDeferredOpenData )
	{
		// Important: Need to set this first for proper freeing in case of error.
		m_pDeferredOpenData->bByString = false;

		if ( in_pFSFlags )
		{
			m_pDeferredOpenData->bUseFlags = true;
			m_pDeferredOpenData->flags = *in_pFSFlags;
		}
		else
			m_pDeferredOpenData->bUseFlags = false;

		m_pDeferredOpenData->fileID = in_fileID;
		m_pDeferredOpenData->eOpenMode = in_eOpenMode;
		return AK_Success;
	}
			
	return AK_Fail;
}

void CAkStmTask::FreeDeferredOpenData()
{
	if ( m_pDeferredOpenData )
	{
		if ( m_pDeferredOpenData->bByString 
			&& m_pDeferredOpenData->pszFileName )
		{
			AkFree( CAkStreamMgr::GetObjPoolID(), m_pDeferredOpenData->pszFileName );
		}

		AkFree( CAkStreamMgr::GetObjPoolID(), m_pDeferredOpenData );
		m_pDeferredOpenData = NULL;
	}
}

AKRESULT CAkStmTask::EnsureFileIsOpen()
{
	// Resolve pending file open command if it exists.
	if ( m_pDeferredOpenData && !m_bIsToBeDestroyed )
	{
		AKRESULT eResult;
		bool bSyncOpen = true;	// Force Low-Level IO to open synchronously.
		AkFileSystemFlags * pFlags = ( m_pDeferredOpenData->bUseFlags ) ? &m_pDeferredOpenData->flags : NULL;
		if ( m_pDeferredOpenData->bByString )
		{
			eResult = CAkStreamMgr::GetFileLocationResolver()->Open(
				m_pDeferredOpenData->pszFileName, 
				m_pDeferredOpenData->eOpenMode,
				pFlags,
				bSyncOpen,
				m_fileDesc );
		}
		else
		{
			eResult = CAkStreamMgr::GetFileLocationResolver()->Open(
				m_pDeferredOpenData->fileID, 
				m_pDeferredOpenData->eOpenMode,
				pFlags,
				bSyncOpen,
				m_fileDesc );
		}

		if ( AK_Success == eResult )
		{
			SetFileOpen();
			// Debug check sync flag.
			AKASSERT( bSyncOpen || !"Cannot defer open when asked for synchronous" );
			OnSetFileSize();
		}
		else
		{
#ifndef AK_OPTIMIZED
			// Monitor error.
			if ( m_pDeferredOpenData->bByString )
			{
				size_t uLen = sizeof( AkOSChar ) * ( AKPLATFORM::OsStrLen( m_pDeferredOpenData->pszFileName ) + 64 );
				AkOSChar * pMsg = (AkOSChar *) AkAlloca( uLen );
				OS_PRINTF( pMsg, ( AK_FileNotFound == eResult ) ? AKTEXT("File not found: %s") : AKTEXT("Cannot open file: %s"),  m_pDeferredOpenData->pszFileName );
				AK::Monitor::PostString( pMsg, AK::Monitor::ErrorLevel_Error );
			}
			else
			{
				size_t uLen = 64;
				AkOSChar * pMsg = (AkOSChar *) AkAlloca( uLen );
				OS_PRINTF( pMsg, ( AK_FileNotFound == eResult ) ? AKTEXT("File not found: %u") : AKTEXT("Cannot open file: %u"),  m_pDeferredOpenData->fileID );
				AK::Monitor::PostString( pMsg, AK::Monitor::ErrorLevel_Error );
			}
#endif
		}

		// Free m_pDeferredOpenData. No need to lock: this is always called from the I/O thread,
		// as is InstantDestroy().
		FreeDeferredOpenData();
		
		return eResult;
	}

	// Already open.
	return AK_Success;
}

// Task needs to acknowledge that it can be destroyed. Device specific (virtual). Call only when IsToBeDestroyed().
// Default implementation returns "true".
// Note: Implementations must always lock the task's status in order to avoid race conditions between
// client's Destroy() and the I/O thread calling InstantDestroy().
bool CAkStmTask::CanBeDestroyed()
{
	AkAutoLock<CAkLock> stmLock( m_lockStatus );
	return true;
}

#ifndef AK_OPTIMIZED
// Profiling: Get stream information. This information should be queried only once, since it is unlikely to change.
void CAkStmTask::GetStreamRecord( 
    AkStreamRecord & out_streamRecord
    )
{
    out_streamRecord.bIsAutoStream = true;
    out_streamRecord.deviceID = m_pDevice->GetDeviceID( );
    if ( m_pszStreamName != NULL )
    {
        AK_WCHAR_TO_UTF16( out_streamRecord.szStreamName, m_pszStreamName, AK_MONITOR_STREAMNAME_MAXLENGTH );    
		out_streamRecord.uStringSize = (AkUInt32)AKPLATFORM::AkUtf16StrLen( out_streamRecord.szStreamName ) + 1;
        out_streamRecord.szStreamName[AK_MONITOR_STREAMNAME_MAXLENGTH-1] = 0;
    }
    else
    {
        out_streamRecord.uStringSize = 0;    
        out_streamRecord.szStreamName[0] = NULL;
    }
    out_streamRecord.uFileSize = m_fileDesc.iFileSize;
    out_streamRecord.uStreamID = m_uStreamID;
}
#endif

//-----------------------------------------------------------------------------
// Name: class CAkStdStmBase
// Desc: Standard stream base implementation.
//-----------------------------------------------------------------------------

CAkStdStmBase::CAkStdStmBase()
: m_uFilePosition( 0 )
, m_uCurPosition( 0 )
, m_eStmStatus( AK_StmStatusIdle )
, m_pBuffer( NULL )
, m_uActualSize( 0 )
{
	m_eStmType = AK_StmTypeStandard;
	m_bIsWriteOp = false;
	m_uBufferSize = 0;
}

CAkStdStmBase::~CAkStdStmBase( )
{
	// If the stream kept asking to be scheduled, now it is time to stop.
    if ( m_bRequiresScheduling )
        m_pDevice->StdSemDecr();
}

// Init.
// Sync: None.
AKRESULT CAkStdStmBase::Init(
    CAkDeviceBase *     in_pDevice,         // Owner device.
    const AkFileDesc &  in_fileDesc,        // File descriptor.
    AkOpenMode          in_eOpenMode        // Open mode.
    )
{
    AKASSERT( in_pDevice != NULL );

    m_pDevice           = in_pDevice;

    // Copy file descriptor.
    m_fileDesc          = in_fileDesc;
    if ( m_fileDesc.iFileSize < 0 )
    {
        AKASSERT( !"Invalid file size" );
        return AK_InvalidParameter;
    }
    
	// Notify low-level IO if file descriptor is sector based and offset is not null.
    if ( m_fileDesc.uSector > 0 )
        m_bIsPositionDirty       = true;
    else
        m_bIsPositionDirty       = false;
    
    m_eOpenMode         = in_eOpenMode;
    AKASSERT( m_pszStreamName == NULL );

    m_uLLBlockSize      = m_pDevice->GetLowLevelHook()->GetBlockSize( m_fileDesc );
    if ( !m_uLLBlockSize )
    {
        AKASSERT( !"Invalid Low-Level I/O block size. Must be >= 1" );
        return AK_Fail;
    }

	return AK_Success;
}

//-----------------------------------------------------------------------------
// IAkStdStream interface.
//-----------------------------------------------------------------------------

// Stream info access.
// Sync: None.
void CAkStdStmBase::GetInfo(
    AkStreamInfo & out_info       // Returned stream info.
    )
{
    AKASSERT( m_pDevice != NULL );
    out_info.deviceID	= m_pDevice->GetDeviceID();
    out_info.pszName	= m_pszStreamName;
    out_info.uSize		= m_fileDesc.iFileSize;
	out_info.bIsOpen	= m_bIsFileOpen;
}

// Name the stream (appears in Wwise profiler).
// Sync: None
AKRESULT CAkStdStmBase::SetStreamName(
    const wchar_t * in_pszStreamName    // Stream name.
    )
{
    if ( m_pszStreamName != NULL )
        AkFree( CAkStreamMgr::GetObjPoolID(), m_pszStreamName );

    if ( in_pszStreamName != NULL )
    {
        // Allocate string buffer for user defined stream name.
		size_t uStrLen = wcslen( in_pszStreamName ) + 1;
        m_pszStreamName = (wchar_t*)AkAlloc( CAkStreamMgr::GetObjPoolID(), (AkUInt32)sizeof(wchar_t)*( uStrLen ) );
        if ( m_pszStreamName == NULL )
            return AK_InsufficientMemory;

        // Copy.
		AKPLATFORM::SafeStrCpy( m_pszStreamName, in_pszStreamName, uStrLen );
    }
    return AK_Success;
}

// Get low-level block size for this stream.
// Returns block size for optimal/unbuffered IO.
AkUInt32 CAkStdStmBase::GetBlockSize()
{
    return m_uLLBlockSize;
}

// Get stream position.
// Sync: None. 
// Users should not call this when pending.
AkUInt64 CAkStdStmBase::GetPosition( 
    bool * out_pbEndOfStream   // Input streams only. Can be NULL.
    )   
{
    AKASSERT( m_eStmStatus != AK_StmStatusPending ||
              !"Inaccurate stream position when operation is pending" );
    if ( out_pbEndOfStream != NULL )
    {
        *out_pbEndOfStream = m_bHasReachedEof && !m_bIsPositionDirty;
    }
    return m_uCurPosition;
}

// Operations.
// ------------------------------------------

// Set stream position. Modifies position of next read/write.
// Sync: 
// Fails if an operation is pending.
AKRESULT CAkStdStmBase::SetPosition(
    AkInt64         in_iMoveOffset,     // Seek offset.
    AkMoveMethod    in_eMoveMethod,     // Seek method, from beginning, end or current file position.
    AkInt64 *       out_piRealOffset    // Actual seek offset may differ from expected value when unbuffered IO. 
                                        // In that case, floors to sector boundary. Pass NULL if don't care.
    )
{
    if ( out_piRealOffset != NULL )
    {
        *out_piRealOffset = 0;
    }

    // Safe status.
    if ( m_eStmStatus == AK_StmStatusPending )
    {
        AKASSERT( !"Trying to change stream position while standard IO is pending" );
        return AK_Fail;
    }

    // Compute absolute position.
    AkInt64 iPosition;
    if ( in_eMoveMethod == AK_MoveBegin )
    {
        iPosition = in_iMoveOffset;
    }
    else if ( in_eMoveMethod == AK_MoveCurrent )
    {
        iPosition = m_uCurPosition + in_iMoveOffset;
    }
    else if ( in_eMoveMethod == AK_MoveEnd )
    {
        iPosition = m_fileDesc.iFileSize + in_iMoveOffset;
    }
    else
    {
        AKASSERT( !"Invalid move method" );
        return AK_InvalidParameter;
    }

    if ( iPosition < 0 )
    {
        AKASSERT( !"Trying to move the file pointer before the beginning of the file" );
        return AK_InvalidParameter;
    }

    // Round offset to block size.
    if ( iPosition % m_uLLBlockSize != 0 )
    {
        // Snap to lower boundary.
        iPosition -= ( iPosition % m_uLLBlockSize );
        AKASSERT( iPosition >= 0 );
    }

    // Set real offset if argument specified.
    if ( out_piRealOffset != NULL )
    {
        switch ( in_eMoveMethod )
        {
        case AK_MoveBegin:
            *out_piRealOffset = iPosition;
            break;
        case AK_MoveCurrent:
            *out_piRealOffset = iPosition - m_uCurPosition;
            break;
        case AK_MoveEnd:
            *out_piRealOffset = iPosition - m_fileDesc.iFileSize;
            break;
        default:
            AKASSERT( !"Invalid move method" );
            return AK_Fail;
        }
    }

    // Update position if it changed.
    // Set new file position.
    m_uCurPosition = iPosition;
    // Set position dirty flag.
    m_bIsPositionDirty = true;
    
    return AK_Success;
}

// Read.
// Sync: Returns if task pending. Status change.
AKRESULT CAkStdStmBase::Read(
    void *          in_pBuffer,         // User buffer address. 
    AkUInt32        in_uReqSize,        // Requested read size.
    bool            in_bWait,           // Block until operation is complete.
    AkPriority      in_priority,        // Heuristic: operation priority.
    AkReal32        in_fDeadline,       // Heuristic: operation deadline (s).
    AkUInt32 &      out_uSize           // Size actually read.
    )
{
    return ExecuteOp( false,// (Read)
		in_pBuffer,         // User buffer address. 
		in_uReqSize,        // Requested write size. 
		in_bWait,           // Block until operation is complete.
		in_priority,        // Heuristic: operation priority.
		in_fDeadline,       // Heuristic: operation deadline (s).
		out_uSize );        // Size actually written.
}

// Write.
// Sync: Returns if task pending. Changes status.
AKRESULT CAkStdStmBase::Write(
    void *          in_pBuffer,         // User buffer address. 
    AkUInt32        in_uReqSize,        // Requested write size. 
    bool            in_bWait,           // Block until operation is complete.
    AkPriority      in_priority,        // Heuristic: operation priority.
    AkReal32        in_fDeadline,       // Heuristic: operation deadline (s).
    AkUInt32 &      out_uSize           // Size actually written.
    )
{
    return ExecuteOp( true,	// (Write)
		in_pBuffer,         // User buffer address. 
		in_uReqSize,        // Requested write size. 
		in_bWait,           // Block until operation is complete.
		in_priority,        // Heuristic: operation priority.
		in_fDeadline,       // Heuristic: operation deadline (s).
		out_uSize );        // Size actually written.
}

// Execute Operation (either Read or Write).
AKRESULT CAkStdStmBase::ExecuteOp(
	bool			in_bWrite,			// Read (false) or Write (true).
	void *          in_pBuffer,         // User buffer address. 
    AkUInt32        in_uReqSize,        // Requested write size. 
    bool            in_bWait,           // Block until operation is complete.
    AkPriority      in_priority,        // Heuristic: operation priority.
    AkReal32        in_fDeadline,       // Heuristic: operation deadline (s).
    AkUInt32 &      out_uSize           // Size actually written.
	)
{
    out_uSize		= 0;
	m_uActualSize	= 0;
	m_bIsWriteOp	= in_bWrite;
    m_pBuffer		= in_pBuffer;
    m_priority		= in_priority;
    m_fDeadline		= in_fDeadline;

    // Check requested size.
    if ( in_pBuffer == NULL )
    {
        AKASSERT( !"Invalid buffer" );
        return AK_InvalidParameter;
    }

    // Check heuristics.
    if ( in_priority < AK_MIN_PRIORITY ||
         in_priority > AK_MAX_PRIORITY ||
         in_fDeadline < 0 )
    {
        AKASSERT( !"Invalid heuristics" );
        return AK_InvalidParameter;
    }

    // Check status.
    if ( m_eStmStatus == AK_StmStatusPending )
    {
        AKASSERT( !"Operation already pending or stream is in error mode" );
        return AK_Fail;
    }

    // Verify with block size.
    if ( in_uReqSize % m_uLLBlockSize != 0 )
    {
        AKASSERT( !"Requested size incompatible with Low-Level block size" );
        return AK_Fail;
    }

    // Prepare IO.
    if ( in_uReqSize <= m_fileDesc.iFileSize - m_uCurPosition || !m_bIsFileOpen || in_bWrite )
		m_uBufferSize = in_uReqSize;
	else
	{
		// Set to size left, and round up to Low-Level IO. 
		m_uBufferSize = (AkUInt32)( m_fileDesc.iFileSize - m_uCurPosition );
		m_uBufferSize = m_uLLBlockSize * ( ( m_uBufferSize + m_uLLBlockSize - 1 ) / m_uLLBlockSize );
	}

	// Leave right away if requested size turns out to be 0 (only for Read).
	if ( 0 == m_uBufferSize )
	{
		out_uSize = 0;
		return AK_Success;
	}
		
    // Reset time.
	AKPLATFORM::PerformanceCounter( &m_iIOStartTime );
    
    // If blocking, register this thread as blocked.
	AKRESULT eResult;
    if ( in_bWait )
    {
		m_lockStatus.Lock();
		SetBlockedStatus();

	    // Set Status. Notify device sheduler.
		SetStatus( AK_StmStatusPending );
		m_lockStatus.Unlock();
		
	    // Wait for the blocking event.
		m_pDevice->WaitForIOCompletion( this );

		eResult = ( AK_StmStatusCompleted == m_eStmStatus ) ? AK_Success : AK_Fail;
    }
    else
    {
    	// Set Status. Notify device sheduler.
    	AkAutoLock<CAkLock> status( m_lockStatus );
		SetStatus( AK_StmStatusPending );
		eResult = AK_Success;
    }
    	
    out_uSize = m_uActualSize;

    return eResult;
}

// Get data and size.
// Returns address of data. No check for pending I/O.
// Sync: None. Always accurate when I/O not pending.
void * CAkStdStmBase::GetData( 
    AkUInt32 & out_uActualSize   // Size actually read or written.
    )
{
    out_uActualSize = m_uActualSize;
    return m_pBuffer;
}
// Info access.
// Sync: None. Status query.
AkStmStatus CAkStdStmBase::GetStatus()
{
    return m_eStmStatus;
}

//-----------------------------------------------------------------------------
// CAkStmTask virtual methods implementation.
//-----------------------------------------------------------------------------

// Update task after data transfer.
// Sync: Status must be locked prior to calling this function. 
// Returns the byte offset by which the stream position was incremented therein 
// (usually equals in_uActualIOSize unless there was an error).
AkUInt32 CAkStdStmBase::UpdatePosition( 
	const AkUInt64 & in_uPosition,		// Absolute file position of transfer.
    void *      in_pBuffer,             // Address of data.
    AkUInt32    in_uActualIOSize,       // Size available for writing/reading.
    bool		in_bStoreData			// Store data in stream object if true, free buffer otherwise.
    )
{
    if ( in_bStoreData
		&& !m_bIsToBeDestroyed 
		&& m_eStmStatus != AK_StmStatusError )
    {
	    // Buffer returned must be the same than buffer provided by TryGetIOBuffer.
		AKASSERT( /**m_eStmStatus == AK_StmStatusPending &&**/
              in_pBuffer == (AkUInt8*)m_pBuffer + m_uActualSize );
    
		// Check EOF.
		// Truncate data size if the stream is going to pass the end of file.
		if ( ( in_uPosition + in_uActualIOSize ) >= GetFileEndPosition()
			&& !m_bIsWriteOp )
		{
			in_uActualIOSize = (AkUInt32)( GetFileEndPosition() - in_uPosition );
			m_bHasReachedEof = true;
		}
		else 
			m_bHasReachedEof = false;
	}
	else
	{
		in_uActualIOSize = 0;
	}

	// Update position.
    AKASSERT( m_uActualSize + in_uActualIOSize <= m_uBufferSize );
    m_uActualSize += in_uActualIOSize;
    AKASSERT( !m_bIsPositionDirty || 0 == in_uActualIOSize ); // else, the client called SetPosition while IO was in progress (illegal with std streams).
    m_uFilePosition += in_uActualIOSize;

    // Profiling.
#ifndef AK_OPTIMIZED
    m_uBytesTransfered += in_uActualIOSize;
#endif
    
	return in_uActualIOSize;
}

// Update task's status after transfer.
// Sync: Status must be locked prior to calling this function.
void CAkStdStmBase::UpdateTaskStatus(
	AKRESULT	in_eIOResult			// AK_Success if IO was successful, AK_Cancelled if IO was cancelled, AK_Fail otherwise.
	)
{
    // Compute status.
    if ( in_eIOResult == AK_Fail )
    {
		// Change Status and update semaphore count.
		SetStatus( AK_StmStatusError );
    }
    else
    {
        if ( m_bHasReachedEof ||
			 m_uActualSize == m_uBufferSize )
        {
			// Update client position.
			m_uCurPosition = m_uFilePosition - GetFileOffset();

			// Change Status and update semaphore count.
			SetStatus( AK_StmStatusCompleted );
        }
        // else Still pending: do not change status.
    }

    // Release the client thread if blocking I/O.
    if ( IsBlocked() 
		&& m_eStmStatus != AK_StmStatusPending 
		&& m_eStmStatus != AK_StmStatusIdle )
    {
		m_pDevice->SignalIOCompleted( this );
    }
}

void CAkStdStmBase::Kill()
{
	AkAutoLock<CAkLock> updateStatus( m_lockStatus );
	UpdateTaskStatus( AK_Fail );
}

// PopIOBuffer.
// Does not apply: automatic stream specific.
void * CAkStdStmBase::PopIOBuffer()
{
    AKASSERT( !"Cannot pop buffer from standard stream" );
    return NULL;
}

bool CAkStdStmBase::ForceInstantDestroy()
{
	if ( !IsToBeDestroyed() )
		Destroy();
	if ( CanBeDestroyed() )
	{
		// Destroys itself.
		InstantDestroy();
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Profiling.
//-----------------------------------------------------------------------------
#ifndef AK_OPTIMIZED
// Get stream data.
void CAkStdStmBase::GetStreamData(
    AkStreamData & out_streamData
    )
{
    out_streamData.uStreamID = m_uStreamID;
    // Note. Priority appearing in profiler will be that which was used in last operation. 
    out_streamData.uPriority = m_priority;
    out_streamData.uBufferSize = 0;
    out_streamData.uAvailableData = 0;
    out_streamData.uFilePosition = m_uCurPosition;
    out_streamData.uNumBytesTransfered = m_uBytesTransfered;
    m_uBytesTransfered = 0;    // Reset.
}

// Signals that stream can be destroyed.
void CAkStdStmBase::ProfileAllowDestruction()
{
    AKASSERT( m_bIsToBeDestroyed );
    m_bIsProfileDestructionAllowed = true;
	AkAutoLock<CAkLock> statusChange( m_lockStatus );
	SetStatus( AK_StmStatusCancelled );
 }
#endif

//-----------------------------------------------------------------------------
// Helpers.
//-----------------------------------------------------------------------------
// Set task's status.
// Sync: Status must be locked.
void CAkStdStmBase::SetStatus( 
    AkStmStatus in_eStatus 
    )
{
	m_eStmStatus = in_eStatus;

	// Update semaphore.
	if ( IsToBeDestroyed() && CanBeDestroyed() )
	{
		// Requires clean up.
		if ( !m_bRequiresScheduling )
        {
            // Signal IO thread for clean up.
			m_bRequiresScheduling = true;
            m_pDevice->StdSemIncr();
        }
	}
    else
    {
		if ( AK_StmStatusPending == in_eStatus )
		{
			// Requires IO transfer.
			AKASSERT( !IsToBeDestroyed() || !"Cannot call SetStatus(Pending) after stream was scheduled for termination" );
			SetReadyForIO( true );
			if ( !m_bRequiresScheduling )
			{
				m_bRequiresScheduling = true;				
				m_pDevice->StdSemIncr();
			}
		}
		else
		{
			// Does not require IO transfer.
			SetReadyForIO( false );
			if ( m_bRequiresScheduling )
			{
				m_bRequiresScheduling = false;
				m_pDevice->StdSemDecr();
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Name: class CAkAutoStmBase
// Desc: Automatic stream base implementation.
//-----------------------------------------------------------------------------

CAkAutoStmBase::CAkAutoStmBase()
: m_uVirtualBufferingSize( 0 )
, m_uNextToGrant( 0 )
, m_bIsRunning( false )
, m_bIOError( false )
{
	m_eStmType = AK_StmTypeAutomatic;
	m_bIsWriteOp = false;
}

CAkAutoStmBase::~CAkAutoStmBase( )
{
	// If the stream kept asking to be scheduled, now it is time to stop.
    if ( m_bRequiresScheduling )
        m_pDevice->AutoSemDecr();
}

// Init.
AKRESULT CAkAutoStmBase::Init( 
    CAkDeviceBase *             in_pDevice,         // Owner device.
    const AkFileDesc &          in_fileDesc,        // File descriptor.
    const AkAutoStmHeuristics & in_heuristics,      // Streaming heuristics.
    AkAutoStmBufSettings *      in_pBufferSettings, // Stream buffer settings.
    AkUInt32                    in_uGranularity     // Device's I/O granularity.
    )
{
    AKASSERT( in_pDevice != NULL );
    AKASSERT( in_heuristics.fThroughput >= 0 &&
              in_heuristics.priority >= AK_MIN_PRIORITY &&
              in_heuristics.priority <= AK_MAX_PRIORITY );

    m_pDevice           = in_pDevice;

    if ( in_fileDesc.iFileSize < 0 )
    {
        AKASSERT( !"Invalid file size" );
        return AK_InvalidParameter;
    }

    m_fileDesc          = in_fileDesc;

    AKASSERT( m_pszStreamName == NULL );
    
    m_uLLBlockSize = m_pDevice->GetLowLevelHook()->GetBlockSize( m_fileDesc );
    if ( !m_uLLBlockSize )
    {
        AKASSERT( !"Invalid Low-Level I/O block size. Must be >= 1" );
        return AK_Fail;
    }

	m_listBuffers.Init();

	// Heuristics.
    m_fThroughput   = in_heuristics.fThroughput;
    m_uLoopStart    = in_heuristics.uLoopStart - ( in_heuristics.uLoopStart % m_uLLBlockSize );;
	if ( in_heuristics.uLoopEnd <= m_fileDesc.iFileSize )
		m_uLoopEnd      = in_heuristics.uLoopEnd;
	else
		m_uLoopEnd		= (AkUInt32)m_fileDesc.iFileSize;
    
	// Minimum number of buffers concurrently owned by client is 1 when not specified.
	m_uMinNumBuffers = ( in_heuristics.uMinNumBuffers > 0 ) ? in_heuristics.uMinNumBuffers : 1;

    m_priority      = in_heuristics.priority;
    
    m_uNextExpectedUserPosition	= GetFileOffset();

    // Set up buffer size according to streaming memory constraints.
    if ( !in_pBufferSettings )
        m_uBufferSize = in_uGranularity;
    else
    {
        // Buffer size constraints.
        if ( in_pBufferSettings->uBufferSize != 0 )
        {
            // User constrained buffer size. Ensure that it is valid with device granularity and low-level IO block size.
            if ( ( in_pBufferSettings->uBufferSize - in_uGranularity ) % m_uLLBlockSize != 0 )
            {
                AKASSERT( !"Specified buffer size is invalid with device's granularity and low-level block size" );
                return AK_InvalidParameter;
            }
            m_uBufferSize = in_pBufferSettings->uBufferSize;
        }
        else
        {
            m_uBufferSize = AkMax( in_pBufferSettings->uMinBufferSize, in_uGranularity );     // User constrained MIN buffer size.

            if ( in_pBufferSettings->uBlockSize > 0 )
            {
                // Block size specified. Buffer size must be a multiple of this size.
                AkUInt32 uRemaining = m_uBufferSize % in_pBufferSettings->uBlockSize;
                if ( uRemaining != 0 )
                {
                    // Snap to ceil.
                    m_uBufferSize += ( in_pBufferSettings->uBlockSize - uRemaining );
                }
            }
        }
    }

    // Correct buffer size to block size.
    if ( ( m_uBufferSize % m_uLLBlockSize ) != 0 )
    {
        // Warn with an assert.
        AKASSERT( !"Warning, unbuffered IO invalidated due to specified stream buffer block size" );
        // Snap to ceil.
        m_uBufferSize += ( m_uLLBlockSize - ( m_uBufferSize % m_uLLBlockSize ) );
    }

    if ( m_fileDesc.iFileSize == 0 )
		SetReachedEof( true );
    
    return AK_Success;
}

//-----------------------------------------------------------------------------
// IAkAutoStream interface.
//-----------------------------------------------------------------------------

// Destruction.
// Close stream. The object is destroyed and the interface becomes invalid.
// Buffers are flushed here.
// Sync: 
// 1. Locks scheduler to set its m_bIsToBeDestroued flag. Better lock the
// scheduler once in a while than having the scheduler lock all the streams
// every time it schedules a task. Also, it spends most of its time executing I/O,
// so risks of interlock are minimized.
// 2. Lock status.
void CAkAutoStmBase::Destroy()
{
    m_lockStatus.Lock();

	SetToBeDestroyed();

	CHECK_BUFFERING_CONSISTENCY();

	// Client can have buffers still granted to him. Clear them before flushing.
	// More efficient just to clear m_uNextToGrant while adjusting the virtual buffering value, then flush all.
	AkBufferList::Iterator it = m_listBuffers.Begin();
	while ( m_uNextToGrant > 0 )
	{
		AKASSERT( it != m_listBuffers.End() );
		m_uVirtualBufferingSize += (*it)->uDataSize;
		--m_uNextToGrant;
		++it;
	}

	Flush();	// Scheduling status is updated in Flush().
    
	m_listBuffers.Term();

	m_lockStatus.Unlock();
}

// Stream info access.
// Sync: None.
void CAkAutoStmBase::GetInfo(
    AkStreamInfo & out_info       // Returned stream info.
    )
{
    AKASSERT( m_pDevice != NULL );
    out_info.deviceID	= m_pDevice->GetDeviceID( );
    out_info.pszName	= m_pszStreamName;
    out_info.uSize		= m_fileDesc.iFileSize;
	out_info.bIsOpen	= m_bIsFileOpen;
}

// Stream heuristics access.
// Sync: None.
void CAkAutoStmBase::GetHeuristics(
    AkAutoStmHeuristics & out_heuristics    // Returned stream heuristics.
    )
{
    out_heuristics.fThroughput  = m_fThroughput;
    out_heuristics.uLoopStart   = m_uLoopStart;
    out_heuristics.uLoopEnd     = m_uLoopEnd;
    out_heuristics.uMinNumBuffers   = m_uMinNumBuffers;
    out_heuristics.priority     = m_priority;
}

// Stream heuristics run-time change.
// Sync: None.
AKRESULT CAkAutoStmBase::SetHeuristics(
    const AkAutoStmHeuristics & in_heuristics   // New stream heuristics.
    )
{
    if ( in_heuristics.fThroughput < 0 ||
         in_heuristics.priority < AK_MIN_PRIORITY ||
         in_heuristics.priority > AK_MAX_PRIORITY )
    {
        AKASSERT( !"Invalid stream heuristics" );
        return AK_InvalidParameter;
    }

	// Update heuristics that have no effect on scheduling.
    m_priority      = in_heuristics.priority;

	//
	// Update heuristics that have an effect on scheduling.
    //
	// Looping.
	//
	AkUInt32 uLoopEnd;
	if ( in_heuristics.uLoopEnd <= m_fileDesc.iFileSize 
		|| !m_bIsFileOpen )
		uLoopEnd = in_heuristics.uLoopEnd;
	else
		uLoopEnd = (AkUInt32)m_fileDesc.iFileSize;
    if ( m_uLoopEnd != uLoopEnd ||
         m_uLoopStart != in_heuristics.uLoopStart )
    {
        // Lock status.
        AkAutoLock<CAkLock> stmBufferGate( m_lockStatus );
        
        
        // Update other heuristics.
		m_fThroughput   = in_heuristics.fThroughput;
		// Note: Minimum number of buffers concurrently owned by client is 1 when not specified.
		m_uMinNumBuffers = ( in_heuristics.uMinNumBuffers > 0 ) ? in_heuristics.uMinNumBuffers : 1;


        // If data was read in the looping region, flush it.

		// Find max buffering position. It is the minimum between loop end values 
		// (except if one of them is zero, which means "no looping").
		AkUInt64 uMaxPosition;
		if ( 0 == uLoopEnd && m_uLoopEnd > 0 )
			uMaxPosition = m_uLoopEnd;
		else if ( 0 == m_uLoopEnd && uLoopEnd > 0 )
			uMaxPosition = uLoopEnd;
		else
			uMaxPosition = AkMin( m_uLoopEnd, uLoopEnd );

		// Make position absolute.
		uMaxPosition += GetFileOffset();

		// Find index from which buffers should be removed.
		AkBufferList::IteratorEx it = m_listBuffers.BeginEx();

		AkUInt32 uLastBufferIdxToKeep = 0;
		while ( uLastBufferIdxToKeep < m_uNextToGrant ) // skip buffers that are already granted
        {        
			++it;
			++uLastBufferIdxToKeep;
		}

		bool bReachedLastBufferToKeep = false;

		while ( it != m_listBuffers.End() // skip buffers according to their position in file
				&& (*it)->uPosition < uMaxPosition )
		{
			AkUInt64 uBufferEndPosition = (*it)->uPosition + (*it)->uDataSize;

			// Keep this buffer.
			++it;

			if ( uBufferEndPosition >= uMaxPosition )
			{
				// This was the last buffer to be kept. 
				bReachedLastBufferToKeep = true;
				break;
			}
		}
		
		// (it) now points to the most recent buffer that should be flushed. 
		
		// Remove buffers (inside scheduler lock: memory change).
		{
			AkAutoLock<CAkIOThread> lock( *m_pDevice );
			while ( it != m_listBuffers.End() )
			{
				AKASSERT( m_uVirtualBufferingSize >= (*it)->uDataSize );
				m_uVirtualBufferingSize -= (*it)->uDataSize;

				AkStmBuffer * pBufferEntry = (*it);
				it = m_listBuffers.Erase( it );
		            
				AK::MemoryMgr::ReleaseBlock( m_pDevice->GetIOPoolID(), pBufferEntry->pBuffer );
				m_pDevice->ReleaseCachedBufferHolder( pBufferEntry );
			}

			// Cancel pending transfers. Skip transfers below uMaxPosition if there was not enough
			// buffers to reach the loop end constraint.
			CancelPendingTransfers( ( !bReachedLastBufferToKeep ) ? uMaxPosition : 0 );

			m_pDevice->NotifyMemChange( );
		}
	
		// Snap loop start to block size.
	    m_uLoopStart    = in_heuristics.uLoopStart - ( in_heuristics.uLoopStart % m_uLLBlockSize );
	    m_uLoopEnd      = uLoopEnd;

		UpdateSchedulingStatus();
    }
	else	// looping changed
	{
		// Update other heuristics that have an effect on scheduling only if their value changed.

		// Note: Minimum number of buffers concurrently owned by client is 1 when not specified.
		AkUInt32 uNewMinNumBuffers = ( in_heuristics.uMinNumBuffers > 0 ) ? in_heuristics.uMinNumBuffers : 1;

		if ( m_fThroughput != in_heuristics.fThroughput
			|| m_uMinNumBuffers != uNewMinNumBuffers )
		{
			AkAutoLock<CAkLock> stmBufferGate( m_lockStatus );

			m_uMinNumBuffers	= uNewMinNumBuffers;
			m_fThroughput		= in_heuristics.fThroughput;

			UpdateSchedulingStatus();
		}		
	}
    return AK_Success;
}

// Name the stream (appears in Wwise profiler).
// Sync: None.
AKRESULT CAkAutoStmBase::SetStreamName(
    const wchar_t * in_pszStreamName    // Stream name.
    )
{
    if ( m_pszStreamName != NULL )
        AkFree( CAkStreamMgr::GetObjPoolID(), m_pszStreamName );

    if ( in_pszStreamName != NULL )
    {
        // Allocate string buffer for user defined stream name.
		size_t uStrLen = wcslen( in_pszStreamName ) + 1;
        m_pszStreamName = (wchar_t*)AkAlloc( CAkStreamMgr::GetObjPoolID(), (AkUInt32)sizeof(wchar_t)*( uStrLen ) );
        if ( m_pszStreamName == NULL )
            return AK_InsufficientMemory;

        // Copy.
		AKPLATFORM::SafeStrCpy( m_pszStreamName, in_pszStreamName, uStrLen );
    }
    return AK_Success;
}

// Returns low-level IO block size for this stream's file descriptor.
AkUInt32 CAkAutoStmBase::GetBlockSize()
{
    return m_uLLBlockSize;
}

// Operations.
// ---------------------------------------

// Starts automatic scheduling.
// Sync: Status update if not already running. 
// Notifies memory change.
AKRESULT CAkAutoStmBase::Start()
{
    if ( !m_bIsRunning )
    {
		{
			// UpdateSchedulingStatus() will notify scheduler if required.
			AkAutoLock<CAkLock> status( m_lockStatus );
			SetRunning( true );
			UpdateSchedulingStatus();
		}

        // The scheduler should reevaluate memory usage. Notify it.
		{
			AkAutoLock<CAkIOThread> lock( *m_pDevice );
	        m_pDevice->NotifyMemChange();
		}

        // Reset time. Time count since last transfer starts now.
        m_iIOStartTime = m_pDevice->GetTime();
    }
    return m_bIOError ? AK_Fail : AK_Success;
}

// Stops automatic scheduling.
// Sync: Status update.
AKRESULT CAkAutoStmBase::Stop()
{
	// Lock status.
    AkAutoLock<CAkLock> status( m_lockStatus );

    SetRunning( false );
	Flush();
    
    return AK_Success;
}

// Get stream position; position as seen by the user.
AkUInt64 CAkAutoStmBase::GetPosition( 
    bool * out_pbEndOfStream    // Can be NULL.
    )
{
	AkAutoLock<CAkLock> stmBufferGate( m_lockStatus );

	AkUInt64 uCurPosition;
	if ( !m_listBuffers.IsEmpty() )
		uCurPosition = m_listBuffers.First()->uPosition;
	else
		uCurPosition = m_uNextExpectedUserPosition;

	// Convert to user value.
	AKASSERT( uCurPosition >= GetFileOffset() );
	uCurPosition -= GetFileOffset();

    if ( out_pbEndOfStream != NULL )
        *out_pbEndOfStream = ( uCurPosition >= (AkUInt64)m_fileDesc.iFileSize );

    return uCurPosition;
}

// Set stream position. Modifies position of next read/write.
// Sync: Updates status. 
AKRESULT CAkAutoStmBase::SetPosition(
    AkInt64         in_iMoveOffset,     // Seek offset.
    AkMoveMethod    in_eMoveMethod,     // Seek method, from beginning, end or current file position.
    AkInt64 *       out_piRealOffset    // Actual seek offset may differ from expected value when unbuffered IO.
                                        // In that case, floors to sector boundary. Pass NULL if don't care.
    )
{
    if ( out_piRealOffset != NULL )
    {
        *out_piRealOffset = 0;
    }

    // Compute absolute position.
    AkInt64 iPosition;
    if ( in_eMoveMethod == AK_MoveBegin )
    {
        iPosition = in_iMoveOffset;
    }
    else if ( in_eMoveMethod == AK_MoveCurrent )
    {
		iPosition = GetPosition( NULL ) + in_iMoveOffset;
    }
    else if ( in_eMoveMethod == AK_MoveEnd )
    {
        iPosition = m_fileDesc.iFileSize + in_iMoveOffset;
    }
    else
    {
        AKASSERT( !"Invalid move method" );
        return AK_InvalidParameter;
    }

    if ( iPosition < 0 )
    {
        AKASSERT( !"Trying to move the file pointer before the beginning of the file" );
        return AK_InvalidParameter;
    }

    // Change offset if Low-Level block size is greater than 1.
    if ( iPosition % m_uLLBlockSize != 0 )
    {
        // Round down to block size.
        iPosition -= ( iPosition % m_uLLBlockSize );
        AKASSERT( iPosition >= 0 );
    }

    // Set real offset if argument specified.
    if ( out_piRealOffset != NULL )
    {
        switch ( in_eMoveMethod )
        {
        case AK_MoveBegin:
            *out_piRealOffset = iPosition;
            break;
        case AK_MoveCurrent:
            *out_piRealOffset = iPosition - GetPosition( NULL );
            break;
        case AK_MoveEnd:
            *out_piRealOffset = iPosition - m_fileDesc.iFileSize;
            break;
        default:
            AKASSERT( !"Invalid move method" );
        }
    }

    // Set new position and update status. 
	iPosition += GetFileOffset();
    ForceFilePosition( iPosition );

    return AK_Success;
}

// Data/status access. 
// -----------------------------------------

// GetBuffer.
// Return values : 
// AK_DataReady     : if buffer is granted.
// AK_NoDataReady   : if buffer is not granted yet.
// AK_NoMoreData    : if buffer is granted but reached end of file (next will return with size 0).
// AK_Fail          : there was an IO error. 

// Sync: Updates status.
AKRESULT CAkAutoStmBase::GetBuffer(
    void *&         out_pBuffer,        // Address of granted data space.
    AkUInt32 &      out_uSize,          // Size of granted data space.
    bool            in_bWait            // Block until data is ready.
    )
{
    out_pBuffer    = NULL;
    out_uSize       = 0;

    // Data ready?
	{
		AkAutoLock<CAkLock> bufferList( m_lockStatus );
	    out_pBuffer = GetReadBuffer( out_uSize );
	}
    
    // Handle blocking GetBuffer. No data is ready, but there is more data to come
    // (otherwise out_pBuffer would not be NULL).
    if ( in_bWait 
		&& !out_pBuffer 
		&& !m_bIOError )
	{
		if ( !m_bIsRunning )
        {
			AKASSERT( !"Trying to block on GetBuffer() on a stopped stream" );
            return AK_Fail;
        }

		// Get status lock. Try get buffer again. If it returns nothing, then wait for I/O to complete.
		m_lockStatus.Lock();
		out_pBuffer = GetReadBuffer( out_uSize );

		while ( !out_pBuffer 
				&& !m_bIOError
				&& !NeedsNoMoreTransfer( 0 ) )	// Pass 0 "Actual buffering size": We know we don't have any.
		{
			// Wait for I/O to complete if there is no error.
			// Set as "blocked, waiting for I/O".
			SetBlockedStatus();

			// Release lock and let the scheduler perform IO.
			m_lockStatus.Unlock();

			m_pDevice->WaitForIOCompletion( this );
			
			// Get status lock. Try get buffer again. If it returns nothing, then wait for I/O to complete.
			m_lockStatus.Lock();
			out_pBuffer = GetReadBuffer( out_uSize );
		}

		m_lockStatus.Unlock();
    }
    
    AKRESULT eRetCode;
    if ( m_bIOError )
    {
        eRetCode = AK_Fail;
    }
    else if ( out_pBuffer == NULL )
    {
        // Buffer is empty, either because no data is ready, or because scheduling has completed 
        // and there is no more data.
		if ( m_bHasReachedEof 
			&& m_uNextExpectedUserPosition >= GetFileEndPosition() )
            eRetCode = AK_NoMoreData;
        else
		{
			AKASSERT( !in_bWait || !"Blocking GetBuffer() cannot return AK_NoDataReady" );
            eRetCode = AK_NoDataReady;
		}
    }
    else
    {
		// Return AK_NoMoreData if buffering reached EOF and this is going to be the last buffer granted.
		if ( m_bHasReachedEof 
			&& m_uNextExpectedUserPosition >= GetFileEndPosition() )
            eRetCode = AK_NoMoreData;
        else
            eRetCode = AK_DataReady;
    }
    return eRetCode;
}

// Release buffer granted to user. Returns AK_Fail if there are no buffer granted to client.
// Sync: Status lock.
AKRESULT CAkAutoStmBase::ReleaseBuffer()
{
    // Lock status.
    AkAutoLock<CAkLock> stmBufferGate( m_lockStatus );

    // Release first buffer granted to client. 
	if ( m_uNextToGrant > 0 )
    {
		AkStmBuffer * pFirst = m_listBuffers.First();
		AKASSERT( pFirst );
      
		// Note: I/O pool access must be enclosed in scheduler lock.
		{
			AkAutoLock<CAkIOThread> lock( *m_pDevice );
			AK::MemoryMgr::ReleaseBlock( m_pDevice->GetIOPoolID(), pFirst->pBuffer );
			// Memory was released. Signal it.
			m_pDevice->NotifyMemChange();

			AKVERIFY( m_listBuffers.RemoveFirst() == AK_Success );
			m_pDevice->ReleaseCachedBufferHolder( pFirst );
		}
        
        // Update "next to grant" index.
        m_uNextToGrant--;

		UpdateSchedulingStatus();

		return AK_Success;
    }

	// Failure: Buffer was not found or not granted.
	return AK_Fail;
}

// Get the amount of buffering that the stream has. 
// Returns
// - AK_DataReady: Some data has been buffered (out_uNumBytesAvailable is greater than 0).
// - AK_NoDataReady: No data is available, and the end of file has not been reached.
// - AK_NoMoreData: Some or no data is available, but the end of file has been reached. The stream will not buffer any more data.
// - AK_Fail: The stream is invalid due to an I/O error.
AKRESULT CAkAutoStmBase::QueryBufferingStatus( AkUInt32 & out_uNumBytesAvailable )
{
	if ( AK_EXPECT_FALSE( m_bIOError ) )
		return AK_Fail;

	// Lock status to query buffering.
	AkAutoLock<CAkLock> stmBufferGate( m_lockStatus );

	AKRESULT eRetCode;
	out_uNumBytesAvailable = 0;

	AKASSERT( m_listBuffers.Length() >= m_uNextToGrant );
	if ( m_uNextToGrant < m_listBuffers.Length() )
	{
		eRetCode = AK_DataReady;

		// Compute the amount of data that is available.
		AkUInt32 uIdx = 0;
		AkBufferList::Iterator it = m_listBuffers.Begin();
		while ( uIdx < m_uNextToGrant )
	    {
			++uIdx;
			++it;
		}
		while ( it != m_listBuffers.End() )
		{
			out_uNumBytesAvailable += (*it)->uDataSize;
			++it;
		}
	}
	else
	{
		// No data is available.
		eRetCode = AK_NoDataReady;
	}

	// Deal with end of stream: return AK_NoMoreData if we are not going to stream in any more data,
	// or if the device currently cannot stream in anymore data. Clients must be aware that the device
	// is idle to avoid hangs.
	if ( NeedsNoMoreTransfer( out_uNumBytesAvailable ) 
		|| m_pDevice->CannotScheduleAutoStreams() )
		eRetCode = AK_NoMoreData;

	return eRetCode;
}

// Returns the target buffering size based on the throughput heuristic.
AkUInt32 CAkAutoStmBase::GetNominalBuffering()
{
	return (AkUInt32)( m_pDevice->GetTargetAutoStmBufferLength() * m_fThroughput );
}



//-----------------------------------------------------------------------------
// CAkStmTask implementation.
//-----------------------------------------------------------------------------

// This is called when file size is set after deferred open. Stream object implementations may
// perform any update required after file size was set. 
// Automatic stream object ensure that the loop end heuristic is consistent.
void CAkAutoStmBase::OnSetFileSize()
{
	AkAutoStmHeuristics heuristics;
	GetHeuristics( heuristics );
	if ( heuristics.uLoopEnd > m_fileDesc.iFileSize )
	{
		heuristics.uLoopEnd = (AkUInt32)m_fileDesc.iFileSize;
		AKVERIFY( SetHeuristics( heuristics ) == AK_Success );
	}
}

// Data buffer access.
// Allocate a buffer from the Memory Mgr.
// Sync: None. 
// IMPORTANT: However, if device's MemIdle() needs to be signaled, the call to this method must be atomic
// with CAkDeviceBase::NotifyMemIdle(). Then they both need to be enclosed in CAkDeviceBase::LockMem().
void * CAkAutoStmBase::TryGetIOBuffer()
{
    // Allocate a buffer.
    AKASSERT( m_uBufferSize > 0 );
	return AK::MemoryMgr::GetBlock( m_pDevice->GetIOPoolID() );
}



// Update task after data transfer.
// Sync: Status must be locked prior to calling this function. 
// Returns the byte offset by which the stream position was incremented therein 
// (usually equals in_uActualIOSize unless there was an error).
AkUInt32 CAkAutoStmBase::UpdatePosition( 
	const AkUInt64 & in_uPosition,		// Absolute file position of transfer.
    void *      in_pBuffer,             // Address of data.
    AkUInt32    in_uActualIOSize,       // Size available for writing/reading.
    bool		in_bStoreData			// Store data in stream object if true, free buffer otherwise.
    )
{
	AkUInt32 uPositionOffset = 0;

    if ( in_bStoreData 
		&& !m_bIsToBeDestroyed 
		&& !m_bIOError )
    {
		AKASSERT( in_uActualIOSize > 0 );

        // Add buffer to list.
		AkStmBuffer * pNewBuffer;
		{
			AkAutoLock<CAkIOThread> lock( *m_pDevice );
	        pNewBuffer = m_pDevice->GetCachedBufferHolder();
		}

		pNewBuffer->uPosition = in_uPosition;
		pNewBuffer->pBuffer = in_pBuffer;
        
		// Truncate data size if the stream is going to pass the end of file.
		if ( ( in_uPosition + in_uActualIOSize ) > (AkUInt64)( m_fileDesc.iFileSize + m_fileDesc.uSector*m_uLLBlockSize ) )
			uPositionOffset = (AkUInt32)( m_fileDesc.iFileSize + m_fileDesc.uSector*m_uLLBlockSize - in_uPosition );
		else
			uPositionOffset = in_uActualIOSize;

		pNewBuffer->uDataSize = uPositionOffset;

		m_listBuffers.AddLast( pNewBuffer );
    }
    else
    {
        // Stream was either scheduled to be destroyed while I/O was occurring, stopped, or 
        // its position was set dirty while I/O was occuring. Flush that data.
        // Note: I/O pool access must be enclosed in scheduler lock.
        AkAutoLock<CAkIOThread> lock( *m_pDevice );
        AK::MemoryMgr::ReleaseBlock( m_pDevice->GetIOPoolID(), in_pBuffer );
        m_pDevice->NotifyMemChange();
    }
    
    // Profiling. 
#ifndef AK_OPTIMIZED
    m_uBytesTransfered += uPositionOffset;
#endif
    
	return uPositionOffset;
}

// Update task's status after transfer.
void CAkAutoStmBase::UpdateTaskStatus(
	AKRESULT	in_eIOResult			// AK_Success if IO was successful, AK_Cancelled if IO was cancelled, AK_Fail otherwise.
	)
{    
    // Status.
    if ( AK_Fail == in_eIOResult )
    {
        // Set to Error.
        m_bIOError = true;

        // Stop auto stream.
        Stop();
    }

	// Update scheduling status.
    UpdateSchedulingStatus();

    // Release the client thread if an operation was pending and call was blocking on it.
    if ( IsBlocked() )
    {
		m_pDevice->SignalIOCompleted( this );
    }

}

void CAkAutoStmBase::Kill()
{
	AkAutoLock<CAkLock> updateStatus( m_lockStatus );
	UpdateTaskStatus( AK_Fail );
}

// Compute task's deadline for next operation.
// Sync: None, but copy throughput heuristic on the stack.
AkReal32 CAkAutoStmBase::EffectiveDeadline()
{
    // Copy throughput value on the stack, because it can be modified (to zero) by user.
    AkReal32 fThroughput = m_fThroughput;
    if ( fThroughput == 0 )
        return AK_INFINITE_DEADLINE;
    else
    {
        // Note: Sync. These values might be changed by another thread. 
        // In the worst case, the scheduler will take a sub-optimal decision.
		return m_uVirtualBufferingSize / fThroughput;
    }
}

bool CAkAutoStmBase::ForceInstantDestroy()
{
	if ( !IsToBeDestroyed() )
		Destroy();
	if ( CanBeDestroyed() )
	{
		// Destroys itself.
		InstantDestroy();
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Profiling.
//-----------------------------------------------------------------------------
#ifndef AK_OPTIMIZED
// Profiling: get data.
void CAkAutoStmBase::GetStreamData(
    AkStreamData & out_streamData
    )
{
    out_streamData.uStreamID = m_uStreamID;
    out_streamData.uPriority = m_priority;
	out_streamData.uFilePosition = m_uNextExpectedUserPosition - GetFileOffset();
    
    AkUInt32 uBufferSize = static_cast<AkUInt32>( m_pDevice->GetTargetAutoStmBufferLength() * m_fThroughput );
	// Buffering size is the max between target buffering and double buffering.
	// Important: Keep in sync with computation in NeedsBuffering().
	out_streamData.uBufferSize = AkMax( uBufferSize, m_pDevice->GetGranularity() );
	
	out_streamData.uAvailableData = m_uVirtualBufferingSize;

	if ( out_streamData.uAvailableData > out_streamData.uBufferSize )
		out_streamData.uAvailableData = out_streamData.uBufferSize;	// Clamp amount of available data to target buffer length.
    
	out_streamData.uNumBytesTransfered = m_uBytesTransfered;
    m_uBytesTransfered = 0;    // Reset.
}

// Signals that stream can be destroyed.
void CAkAutoStmBase::ProfileAllowDestruction()	
{
    AKASSERT( m_bIsToBeDestroyed );
    m_bIsProfileDestructionAllowed = true;
	UpdateSchedulingStatus();
}
#endif

//-----------------------------------------------------------------------------
// Helpers.
//-----------------------------------------------------------------------------
// Update task status.
// Sync: Status lock.
void CAkAutoStmBase::ForceFilePosition(
    const AkUInt64 in_uNewPosition     // New stream position (absolute).
    )
{
    // Lock status.
    AkAutoLock<CAkLock> statusGate( m_lockStatus );

	// Update position.
	m_uNextExpectedUserPosition = in_uNewPosition;

    // Check whether next buffer position corresponds to desired position (if SetPosition() was consistent
	// with looping heuristic, it will be). If it isn't, then we need to flush everything we have.

	if ( m_uNextToGrant < m_listBuffers.Length() )
	{
		if ( GetNextBufferToGrant()->uPosition != in_uNewPosition )
    	{
			// Flush everything we have, that was not already granted to user.
			// Note: Flush() also flushes pending transfers.
			Flush();
			AKASSERT( m_listBuffers.Length() == m_uNextToGrant );
    	}
		else
			UpdateSchedulingStatus();
    }
	else 
	{
		// Nothing buffered. Yet, there might be pending transfers that are inconsistent with in_uNewPosition.
		// Cancel all pending transfers if applicable.
		CancelPendingTransfers( 0 );
		UpdateSchedulingStatus();
	}
}

// Update task scheduling status; whether or not it is waiting for I/O and counts in scheduler semaphore.
// Sync: Status MUST be locked from outside.
void CAkAutoStmBase::UpdateSchedulingStatus()
{
	CHECK_BUFFERING_CONSISTENCY();

	// Set EOF flag.
	if ( !m_uLoopEnd 
		&& ( GetVirtualFilePosition() >= GetFileEndPosition() )
		&& m_bIsFileOpen )
    {
        SetReachedEof( true );
    }
    else
		SetReachedEof( false );

    // Update scheduler control.
	if ( ( ReadyForIO() && NeedsBuffering( m_uVirtualBufferingSize ) )	// requires a transfer.
		|| ( IsToBeDestroyed() && CanBeDestroyed() ) )	// requires clean up.
    {
        if ( !m_bRequiresScheduling )
        {
			m_bRequiresScheduling = true;
            m_pDevice->AutoSemIncr();
        }
    }
    else
    {
        if ( m_bRequiresScheduling )
        {
			m_bRequiresScheduling = false;
            m_pDevice->AutoSemDecr();
        }
    }
}

// Compares in_uVirtualBufferingSize to target buffer size.
// Important: Keep computation of target buffer size in sync with GetStreamData().
bool CAkAutoStmBase::NeedsBuffering(
	AkUInt32 in_uVirtualBufferingSize
	)
{
	// Needs buffering if below target buffer length, or if below granularity
	// (we need to ensure that the device reads a buffer, apart from what has
	// already been granted to the client).
	return ( in_uVirtualBufferingSize < m_pDevice->GetGranularity()
			|| in_uVirtualBufferingSize < m_pDevice->GetTargetAutoStmBufferLength() * m_fThroughput );
}

// Returns a buffer filled with data. NULL if no data is ready.
// Sync: Accessing list. Must be locked from outside.
void * CAkAutoStmBase::GetReadBuffer(     
    AkUInt32 & out_uSize                // Buffer size.
    )
{
    if ( m_uNextToGrant < m_listBuffers.Length() )
    {
		// Get first buffer not granted.
		AkStmBuffer * pStmBuffer = GetNextBufferToGrant();

		if ( pStmBuffer->uPosition != m_uNextExpectedUserPosition )
        {
            // User attempts to read a buffer passed the end loop point heuristic, but did not set the
            // stream position accordingly!
			// This should never occur if the client is consistent with the heuristics it sets.
            // Flush data reset looping heuristics for user to avoid repeating this mistake, return AK_NoDataReady.
            m_uLoopEnd = 0;
			Flush();
            out_uSize = 0;
            return NULL;
        }
        
        // Update "next to grant" index.
        m_uNextToGrant++;

		// Update m_iNextExpectedUserPosition.
		m_uNextExpectedUserPosition = pStmBuffer->uPosition + pStmBuffer->uDataSize;

		// Update amount of buffered data (data granted to user does not count as buffered data).
		m_uVirtualBufferingSize -= pStmBuffer->uDataSize;

		UpdateSchedulingStatus();

        out_uSize = pStmBuffer->uDataSize;
        return pStmBuffer->pBuffer;
    }
    
    // No data ready.
    out_uSize = 0;
    return NULL;
}

// Flushes all stream buffers that are not currently granted.
// Sync: None. Always called from within status-protected code.
void CAkAutoStmBase::Flush()
{
	CHECK_BUFFERING_CONSISTENCY();

	CancelPendingTransfers( 0 );

    if ( m_listBuffers.Length() > m_uNextToGrant )
    {
		AkUInt32 uIdx = 0;
		AkBufferList::IteratorEx it = m_listBuffers.BeginEx();
		while ( uIdx < m_uNextToGrant )
	    {
			++uIdx;
			++it;
		}
        
		// Lock scheduler for memory change.
		AkAutoLock<CAkIOThread> lock( *m_pDevice );
        while ( it != m_listBuffers.End() )
        {
			AkStmBuffer * pBufferFlush = *it;
            AKASSERT( m_uVirtualBufferingSize >= pBufferFlush->uDataSize );
            m_uVirtualBufferingSize -= pBufferFlush->uDataSize;

            it = m_listBuffers.Erase( it );

			AK::MemoryMgr::ReleaseBlock( m_pDevice->GetIOPoolID(), pBufferFlush->pBuffer );
			m_pDevice->ReleaseCachedBufferHolder( pBufferFlush );
        }
        m_pDevice->NotifyMemChange();
    }

	UpdateSchedulingStatus();
}

void CAkAutoStmBase::OnTransferRemoved( 
	const AkUInt64 in_uExpectedPosition,	// Expected file position (absolute) after request.
	const AkUInt64 in_uActualPosition	// Actual file position after request.
	)
{
	AKASSERT( in_uExpectedPosition >= in_uActualPosition );
	AkUInt32 in_uOvershootSize = (AkUInt32)( in_uExpectedPosition - in_uActualPosition );

	AKASSERT( m_uVirtualBufferingSize >= in_uOvershootSize );
	m_uVirtualBufferingSize -= in_uOvershootSize;
}
