//////////////////////////////////////////////////////////////////////
//
// AkStreamMgr.h
//
// Stream manager Windows-specific implementation:
// Device factory.
// Platform-specific scheduling strategy.
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////
#ifndef _AK_STREAM_MGR_H_
#define _AK_STREAM_MGR_H_

#include <AK/SoundEngine/Common/IAkStreamMgr.h>
#include <AK/Tools/Common/AkObject.h>
#include <AK/Tools/Common/AkArray.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>

#include <AK/SoundEngine/Common/AkStreamMgrModule.h>

#define AK_STM_OBJ_POOL_BLOCK_SIZE      (32)

namespace AK
{
namespace StreamMgr
{
	class CAkDeviceBase;

    //-----------------------------------------------------------------------------
    // Name: class CAkStreamMgr
    // Desc: Implementation of the stream manager.
    //-----------------------------------------------------------------------------
    class CAkStreamMgr : public CAkObject
						,public IAkStreamMgr
#ifndef AK_OPTIMIZED
                        ,public IAkStreamMgrProfile
#endif
    {
        // Public factory.
        friend IAkStreamMgr * Create( 
            const AkStreamMgrSettings &	in_settings		// Stream manager initialization settings.
            );

		// Default settings.
		void GetDefaultSettings(
			AkStreamMgrSettings &		out_settings
			);
		void GetDefaultDeviceSettings(
			AkDeviceSettings &			out_settings
			);

		// Public file location handler getter/setter.
		friend IAkFileLocationResolver * GetFileLocationResolver();
		
		friend void SetFileLocationResolver(
			IAkFileLocationResolver *	in_pFileLocationResolver	// File location resolver. Needed for Open().
			);

        // Device management.
        // Warning: This function is not thread safe.
        friend AkDeviceID CreateDevice(
            const AkDeviceSettings &    in_settings,		// Device settings.
			IAkLowLevelIOHook *			in_pLowLevelHook	// Device specific low-level I/O hook.
            );
        // Warning: This function is not thread safe. No stream should exist for that device when it is destroyed.
        friend AKRESULT   DestroyDevice(
            AkDeviceID                  in_deviceID         // Device ID.
            );

    public:

        // Stream manager destruction.
        virtual void     Destroy();

        // Globals access (for device instantiation, and profiling).
        inline static AkMemPoolId GetObjPoolID()
        {
            return m_streamMgrPoolId;   // Stream manager instance, devices, objects.
        }

		inline static IAkFileLocationResolver * GetFileLocationResolver()
		{
			return m_pFileLocationResolver;
		}

		// Global pool cleanup: dead streams.
		// Since the StreamMgr's global pool is shared across all devices, they all need to perform
		// dead handle clean up. The device that calls this method will also be asked to kill one of
		// its tasks.
		static void ForceCleanup( 
			CAkDeviceBase * in_pCallingDevice,		// Calling device: if specified, the task with the lowest priority for this device will be killed.
			AkPriority		in_priority				// Priority of the new task if applicable. Pass AK_MAX_PRIORITY to ignore.
			);

        // Stream creation interface.
        // ------------------------------------------------------
        
        // Standard stream create methods.
        // -----------------------------

        // String overload.
        virtual AKRESULT CreateStd(
            const AkOSChar*     in_pszFileName,     // Application defined string (title only, or full path, or code...).
            AkFileSystemFlags * in_pFSFlags,        // Special file system flags. Can pass NULL.
            AkOpenMode          in_eOpenMode,       // Open mode (read, write, ...).
            IAkStdStream *&     out_pStream,		// Returned interface to a standard stream.
			bool				in_bSyncOpen		// If true, force the Stream Manager to open file synchronously. Otherwise, it is left to its discretion.
            );
        // ID overload.
        virtual AKRESULT CreateStd(
            AkFileID            in_fileID,          // Application defined ID.
            AkFileSystemFlags * in_pFSFlags,        // Special file system flags. Can pass NULL.
            AkOpenMode          in_eOpenMode,       // Open mode (read, write, ...).
            IAkStdStream *&     out_pStream,		// Returned interface to a standard stream.
			bool				in_bSyncOpen		// If true, force the Stream Manager to open file synchronously. Otherwise, it is left to its discretion.
            );

        
        // Automatic stream create methods.
        // ------------------------------

        // Note: Open does not start automatic streams. 
        // They need to be started explicitly with IAkAutoStream::Start().

        // String overload.
        virtual AKRESULT CreateAuto(
            const AkOSChar*             in_pszFileName,     // Application defined string (title only, or full path, or code...).
            AkFileSystemFlags *         in_pFSFlags,        // Special file system flags. Can pass NULL.
            const AkAutoStmHeuristics & in_heuristics,      // Streaming heuristics.
            AkAutoStmBufSettings *      in_pBufferSettings, // Stream buffer settings. Pass NULL to use defaults (recommended).
            IAkAutoStream *&            out_pStream,		// Returned interface to an automatic stream.
			bool						in_bSyncOpen		// If true, force the Stream Manager to open file synchronously. Otherwise, it is left to its discretion.
            );
        // ID overload.
        virtual AKRESULT CreateAuto(
            AkFileID                    in_fileID,          // Application defined ID.
            AkFileSystemFlags *         in_pFSFlags,        // Special file system flags. Can pass NULL.
            const AkAutoStmHeuristics & in_heuristics,      // Streaming heuristics.
            AkAutoStmBufSettings *      in_pBufferSettings, // Stream buffer settings. Pass NULL to use defaults (recommended).
            IAkAutoStream *&            out_pStream,		// Returned interface to an automatic stream.
			bool						in_bSyncOpen		// If true, force the Stream Manager to open file synchronously. Otherwise, it is left to its discretion.
            );

        // -----------------------------------------------

        // Profiling interface.
        // -----------------------------------------------

         // Profiling access. Returns NULL in AK_OPTIMIZED.
        virtual IAkStreamMgrProfile * GetStreamMgrProfile();

#ifndef AK_OPTIMIZED
        // Public profiling interface.
        // ---------------------------
        virtual AKRESULT StartMonitoring();
	    virtual void     StopMonitoring();

        // Devices enumeration.
        virtual AkUInt32 GetNumDevices();         // Returns number of devices.
        virtual IAkDeviceProfile * GetDeviceProfile( 
            AkUInt32 in_uDeviceIndex              // [0,numDevices[
            );

		inline static AkUInt32 GetNewStreamID()
		{
			return AKPLATFORM::AkInterlockedIncrement( &m_iNextStreamID );
		}

		static AkInt32  m_iNextStreamID;
    #endif

    private:

        // Device management.
        // -----------------------------------------------

        // Warning: This function is not thread safe.
        AkDeviceID CreateDevice(
            const AkDeviceSettings &    in_settings,		// Device settings.
			IAkLowLevelIOHook *			in_pLowLevelHook	// Device specific low-level I/O hook.
            );
        // Warning: This function is not thread safe. No stream should exist for that device when it is destroyed.
        AKRESULT   DestroyDevice(
            AkDeviceID                  in_deviceID         // Device ID.
            );

        // Get device by ID.
        inline CAkDeviceBase * GetDevice( 
            AkDeviceID  in_deviceID 
            )
        {
	        if ( (AkUInt32)in_deviceID >= m_arDevices.Length( ) )
	        {
	            AKASSERT( !"Invalid device ID" );
	            return NULL;
	        }
	        return m_arDevices[in_deviceID];
	    }

        // Singleton.
        CAkStreamMgr();
		CAkStreamMgr( CAkStreamMgr& );
        CAkStreamMgr & operator=( CAkStreamMgr& );
        virtual ~CAkStreamMgr();

        // Initialise/Terminate.
        AKRESULT Init( 
            const AkStreamMgrSettings &	in_settings
            );
        void     Term();

        // Globals: pools and low-level IO interface.
	    static AkMemPoolId				m_streamMgrPoolId;      // Stream manager instance, devices, objects.
        static IAkFileLocationResolver *m_pFileLocationResolver;// Low-level IO location handler.

        // Array of devices.
	public:
        // NOTE: Although ArrayStreamProfiles is used only inside CAkDeviceBase, the definition of its memory pool policy
		// must be public in order to avoid private nested class access inside AkArray. 
		AK_DEFINE_ARRAY_POOL( ArrayPoolLocal, CAkStreamMgr::m_streamMgrPoolId );
	private:
        typedef AkArray<CAkDeviceBase*,CAkDeviceBase*, ArrayPoolLocal, 1> AkDeviceArray;
        static AkDeviceArray m_arDevices;
    };
}
}
#endif //_AK_STREAM_MGR_H_
