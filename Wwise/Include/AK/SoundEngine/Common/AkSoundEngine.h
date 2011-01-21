//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkSoundEngine.h

/// \file 
/// The main sound engine interface.


#ifndef _AK_SOUNDENGINE_H_
#define _AK_SOUNDENGINE_H_

#include <AK/SoundEngine/Common/AkSoundEngineExport.h>
#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/IAkPlugin.h>
#include <AK/SoundEngine/Common/AkCallback.h>

/// Function called on assert handling, optional
/// \sa 
/// - AkInitSettings
#ifndef AK_ASSERT_HOOK
typedef void (*AkAssertHook)( 
						const char * in_pszExpression,	///< Expression
                        const char * in_pszFileName,	///< File Name
                        int in_lineNumber				///< Line Number
						);
#define AK_ASSERT_HOOK
#endif

/// Platform-independent initialization settings of the sound engine
/// \sa 
/// - AK::SoundEngine::Init()
/// - AK::SoundEngine::GetDefaultInitSettings()
/// - \ref soundengine_integration_init_advanced
/// - \ref soundengine_initialization_advanced_soundengine_using_memory_threshold
struct AkInitSettings
{
    AkAssertHook        pfnAssertHook;				///< External assertion handling function (optional)

    AkUInt32            uMaxNumPaths;				///< Maximum number of paths for positioning
    AkUInt32            uMaxNumTransitions;			///< Maximum number of transitions
    AkUInt32            uDefaultPoolSize;			///< Size of the default memory pool, in bytes
	AkReal32            fDefaultPoolRatioThreshold;	///< 0.0f to 1.0f value: The percentage of occupied memory where the sound engine should enter in Low memory Mode. \ref soundengine_initialization_advanced_soundengine_using_memory_threshold
	AkUInt32            uCommandQueueSize;			///< Size of the command queue, in bytes
	AkMemPoolId			uPrepareEventMemoryPoolID;	///< Memory pool where data allocated by AK::SoundEngine::PrepareEvent() and AK::SoundEngine::PrepareGameSyncs() will be done. 
	bool				bEnableGameSyncPreparation;	///< Set to true to enable AK::SoundEngine::PrepareGameSync usage.

    AkUInt32            uMonitorPoolSize;			///< Size of the monitoring pool, in bytes. This parameter is not used in Release build.
    AkUInt32            uMonitorQueuePoolSize;		///< Size of the monitoring queue pool, in bytes. This parameter is not used in Release build.
};


//----------------------------------------------------------------------------------------------------
// AK_WIN
//----------------------------------------------------------------------------------------------------
#ifdef AK_WIN
#include <AK/SoundEngine/Platforms/Windows/AkWinSoundEngine.h>

//----------------------------------------------------------------------------------------------------
// AK_MAC
//----------------------------------------------------------------------------------------------------
#elif defined (AK_MAC)
#include <AK/SoundEngine/Platforms/Mac/AkMacSoundEngine.h>

//----------------------------------------------------------------------------------------------------
// XBOX360
//----------------------------------------------------------------------------------------------------
#elif defined (XBOX360)
#include <AK/SoundEngine/Platforms/XBox360/AkXBox360SoundEngine.h>

//----------------------------------------------------------------------------------------------------
// PS3
//----------------------------------------------------------------------------------------------------
#elif defined (__PPU__) || defined (__SPU__)
#include <AK/SoundEngine/Platforms/PS3/AkPs3SoundEngine.h>

//----------------------------------------------------------------------------------------------------
// Wii
//----------------------------------------------------------------------------------------------------
#elif defined (RVL_OS)
#include <AK/SoundEngine/Platforms/Wii/AkWiiSoundEngine.h>

#else
#error AkSoundEngine.h: Undefined platform
#endif

/// Audiokinetic namespace
namespace AK
{
	/// Audiokinetic sound engine namespace
	/// \remarks The functions in this namespace are thread-safe, unless stated otherwise.
	namespace SoundEngine
	{
        ///////////////////////////////////////////////////////////////////////
		/// @name Initialization
		//@{

		/// Query whether or not the sound engine has been successfully initialized.
		/// \warning This function is not thread-safe. It should not be called at the same time as SoundEngine::Init() or SoundEngine::Term().
		/// \return True if the sound engine has been initialized, False otherwise
		/// \sa
		/// - \ref soundengine_integration_init_advanced
		/// - AK::SoundEngine::Init()
		/// - AK::SoundEngine::Term()
		extern AKSOUNDENGINE_API bool IsInitialized();

		/// Initialize the sound engine.
		/// \warning This function is not thread-safe.
		/// \remark The initial settings should be initialized using AK::SoundEngine::GetDefaultInitSettings()
		///			and AK::SoundEngine::GetDefaultPlatformInitSettings() to fill the structures with their 
		///			default settings. This is not mandatory, but it helps avoid backward compatibility problems.
		/// \return 
		/// - AK_Success if the initialization was successful
		/// - AK_MemManagerNotInitialized if the memory manager is not available or not properly initialized
		/// - AK_StreamMgrNotInitialized if the stream manager is not available or not properly initialized
		/// - AK_SSEInstructionsNotSupported if the machine does not support SSE instruction (only on the PC)
		/// - AK_InsufficientMemory or AK_Fail if there is not enough memory available to initialize the sound engine properly
		/// - AK_InvalidParameter if some parameters are invalid
		/// - AK_Fail if the sound engine is already initialized, or if the provided settings result in insufficient 
		/// ressources for the initialization.
		/// \sa
		/// - \ref soundengine_integration_init_advanced
		/// - AK::SoundEngine::Term()
		/// - AK::SoundEngine::GetDefaultInitSettings()
		/// - AK::SoundEngine::GetDefaultPlatformInitSettings()
        extern AKSOUNDENGINE_API AKRESULT Init(
            AkInitSettings *			in_pSettings,   		///< Initialization settings (can be NULL, to use the default values)
            AkPlatformInitSettings *	in_pPlatformSettings  	///< Platform-specific settings (can be NULL, to use the default values)
		    );

		/// Get the default values of the platform-independent initialization settings.
		/// \warning This function is not thread-safe.
		/// \sa
		/// - \ref soundengine_integration_init_advanced
		/// - AK::SoundEngine::Init()
		/// - AK::SoundEngine::GetDefaultPlatformInitSettings()
		extern AKSOUNDENGINE_API void GetDefaultInitSettings(
            AkInitSettings &			out_settings   			///< Returned default platform-independent sound engine settings
		    );

		/// Get the default values of the platform-specific initialization settings.
		///
		/// Windows Specific:
		///		When initializing for Windows platform, the HWND value returned in the 
		///		AkPlatformInitSettings structure is the foreground HWND at the moment of the 
		///		initialization of the sound engine and may not be the correct one for your need.
		///		Each game must specify the HWND that will be passed to DirectSound initialization.
		///		It is required that each game provides the correct HWND to be used or it could cause
		///		one of the following problem:
		///				- Random Sound engine initialization failure.
		///				- Audio focus to be located on the wrong window.
		///
		/// \warning This function is not thread-safe.
		/// \sa 
		/// - \ref soundengine_integration_init_advanced
		/// - AK::SoundEngine::Init()
		/// - AK::SoundEngine::GetDefaultInitSettings()
		extern AKSOUNDENGINE_API void GetDefaultPlatformInitSettings(
            AkPlatformInitSettings &	out_platformSettings  	///< Returned default platform-specific sound engine settings
		    );

        /// Terminate the sound engine.
		/// If some sounds are still playing or events are still being processed when this function is 
		///	called, they will be stopped.
		/// \warning This function is not thread-safe.
		/// \warning Before calling Term, you must ensure that no other thread is accessing the sound engine.
		/// \sa 
		/// - \ref soundengine_integration_init_advanced
		/// - AK::SoundEngine::Init()
        extern AKSOUNDENGINE_API void Term();

		/// Get the output speaker configuration.
		/// Call this function to get the speaker configuration of the sound engine output (which may not correspond
		/// to the physical output format of the platform, in the case of downmixing provided by the platform itself). 
		/// This speaker configuration is automatically determined by the sound engine, based on the platform and
		/// platform settings (i.e. system menu or control panel option).
		/// 
		/// \warning Call this function only after the sound engine has been properly initialized.
		/// \return One of the supported configuration: 
		/// - AK_SPEAKER_SETUP_STEREO
		/// - AK_SPEAKER_SETUP_5POINT1
		/// - AK_SPEAKER_SETUP_SURROUND (Wii only)
		/// - AK_SPEAKER_SETUP_DPL2	(Wii only)
		/// \sa 
		/// - AkSpeakerConfig.h
		extern AKSOUNDENGINE_API AkChannelMask GetSpeakerConfiguration();

		/// Allows the game to set the volume threshold to be used by the sound engine to determine if a voice must go virtual.
		/// This may be changed anytime once the sound engine was initialized.
		/// If this function is not called, the used value will be the value specified in the platform specific project settings.
		/// \return 
		/// - AK_InvalidParameter if the threshold was not between 0 and -96.3 dB.
		/// - AK_Success if successful
		extern AKSOUNDENGINE_API AKRESULT SetVolumeThreshold( 
			AkReal32 in_fVolumeThresholdDB ///< Volume Threshold, must be a value between 0 and -96.3 dB
			);
		
        //@}

		////////////////////////////////////////////////////////////////////////
		/// @name Rendering Audio
		//@{

		/// Process all events in the sound engine's queue.
		/// This method has to be called periodically (usually once per game frame).
		/// \sa 
		/// - \ref concept_events
		/// - \ref soundengine_events
		/// - AK::SoundEngine::PostEvent()
		/// \return Always returns AK_Success
        extern AKSOUNDENGINE_API AKRESULT RenderAudio();

		//@}

		////////////////////////////////////////////////////////////////////////
		/// @name Component Registration
		//@{
		
		/// Register a plug-in with the sound engine and set the callback functions to create the 
		/// plug-in and its parameter node.
		/// \sa
		/// - \ref register_effects
		/// - \ref plugin_xml
		/// \return AK_Success if successful, AK_InvalidParameter if invalid parameters were provided or Ak_Fail otherwise. Possible reasons for an AK_Fail result are:
		/// - Insufficient memory to register the plug-in
		/// - Plug-in ID already registered
		/// \remarks
		/// Codecs and plug-ins must be registered before loading banks that use them.\n
		/// Loading a bank referencing an unregistered plug-in or codec will result in a load bank success,
		/// but the plug-ins will not be used. More specifically, playing a sound that uses an unregistered effect plug-in 
		/// will result in audio playback without applying the said effect. If an unregistered source plug-in is used by an event's audio objects, 
		/// posting the event will fail.
        extern AKSOUNDENGINE_API AKRESULT RegisterPlugin( 
			AkPluginType in_eType,						///< Plug-in type (for example, source or effect)
			AkUInt32 in_ulCompanyID,					///< Company identifier (as declared in the plug-in description XML file)
			AkUInt32 in_ulPluginID,						///< Plug-in identifier (as declared in the plug-in description XML file)
			AkCreatePluginCallback in_pCreateFunc,		///< Pointer to the plug-in's creation function
            AkCreateParamCallback in_pCreateParamFunc	///< Pointer to the plug-in's parameter node creation function
            );
		
		/// Register a codec type with the sound engine and set the callback functions to create the 
		/// codec's file source and bank source nodes.
		/// \sa 
		/// - \ref register_effects
		/// \return AK_Success if successful, AK_InvalidParameter if invalid parameters were provided, or Ak_Fail otherwise. Possible reasons for an AK_Fail result are:
		/// - Insufficient memory to register the codec
		/// - Codec ID already registered
		/// \remarks
		/// Codecs and plug-ins must be registered before loading banks that use them.\n
		/// Loading a bank referencing an unregistered plug-in or codec will result in a load bank success,
		/// but the plug-ins will not be used. More specifically, playing a sound that uses an unregistered effect plug-in 
		/// will result in audio playback without applying the said effect. If an unregistered source plug-in is used by an event's audio objects, 
		/// posting the event will fail.
        extern AKSOUNDENGINE_API AKRESULT RegisterCodec( 
			AkUInt32 in_ulCompanyID,						///< Company identifier (as declared in the plug-in description XML file)
			AkUInt32 in_ulCodecID,							///< Codec identifier (as declared in the plug-in description XML file)
			AkCreateFileSourceCallback in_pFileCreateFunc,	///< Pointer to the codec's file source node creation function
            AkCreateBankSourceCallback in_pBankCreateFunc	///< Pointer to the codec's bank source node creation function
            );

		//@}

		////////////////////////////////////////////////////////////////////////
		/// @name Getting ID from strings
		//@{

		/// Universal converter from unicode string to ID for the sound engine.
		/// This function will hash the name based on a algorithm ( provided at : /AK/Tools/Common/AkFNVHash.h )
		/// Note:
		///		This function does return a AkUInt32, which is totally compatible with:
		///		AkUniqueID, AkStateGroupID, AkStateID, AkSwitchGroupID, AkSwitchStateID, AkRtpcID, AkEnvID, and so on...
		/// \sa
		/// - AK::SoundEngine::PostEvent
		/// - AK::SoundEngine::SetRTPCValue
		/// - AK::SoundEngine::SetSwitch
		/// - AK::SoundEngine::SetState
		/// - AK::SoundEngine::PostTrigger
		/// - AK::SoundEngine::SetEnvironmentVolume
		/// - AK::SoundEngine::SetEnvironmentVolumes
		/// - AK::SoundEngine::BypassEnvironment
		/// - AK::SoundEngine::SetGameObjectEnvironmentsValues
		/// - AK::SoundEngine::LoadBank
		/// - AK::SoundEngine::UnloadBank
		/// - AK::SoundEngine::PrepareEvent
		/// - AK::SoundEngine::PrepareGameSyncs
		extern AKSOUNDENGINE_API AkUInt32 GetIDFromString( const wchar_t* in_pszString );

		/// Universal converter from ansi string to ID for the sound engine.
		/// This function will hash the name based on a algorithm ( provided at : /AK/Tools/Common/AkFNVHash.h )
		/// Note:
		///		This function does return a AkUInt32, which is totally compatible with:
		///		AkUniqueID, AkStateGroupID, AkStateID, AkSwitchGroupID, AkSwitchStateID, AkRtpcID, AkEnvID, and so on...
		/// \sa
		/// - AK::SoundEngine::PostEvent
		/// - AK::SoundEngine::SetRTPCValue
		/// - AK::SoundEngine::SetSwitch
		/// - AK::SoundEngine::SetState
		/// - AK::SoundEngine::PostTrigger
		/// - AK::SoundEngine::SetEnvironmentVolume
		/// - AK::SoundEngine::SetEnvironmentVolumes
		/// - AK::SoundEngine::BypassEnvironment
		/// - AK::SoundEngine::SetGameObjectEnvironmentsValues
		/// - AK::SoundEngine::LoadBank
		/// - AK::SoundEngine::UnloadBank
		/// - AK::SoundEngine::PrepareEvent
		/// - AK::SoundEngine::PrepareGameSyncs
		extern AKSOUNDENGINE_API AkUInt32 GetIDFromString( const char* in_pszString );

		//@}

		////////////////////////////////////////////////////////////////////////
		/// @name Event Management
		//@{

		/// Asynchronously post an event to the sound engine (by event ID).
		/// The callback function can be used to be noticed when markers are reached or when the event is finished.
		/// An array of wave file sources can be provided to resolve External Sources triggered by the event. 
        /// \return The playing ID of the event launched, or AK_INVALID_PLAYING_ID if posting the event failed
		/// \remarks
		/// If used, the array of external sources should contain the information for each external source triggered by the
		/// event.  When triggering an event with multiple external sources, you need to differentiate each source 
		/// by using the cookie property in the External Source in the Wwise project and in AkExternalSourceInfo.
		/// \aknote If an event triggers the playback of more than one external source, they must be named uniquely in the project 
		/// (therefore have a unique cookie) in order to tell them appart when filling the AkExternalSourceInfo structures.
		/// \sa 
		/// - \ref concept_events
		/// - \ref integrating_external_sources
		/// - AK::SoundEngine::RenderAudio()
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::GetSourcePlayPosition()
        extern AKSOUNDENGINE_API AkPlayingID PostEvent(
	        AkUniqueID in_eventID,							///< Unique ID of the event
	        AkGameObjectID in_gameObjectID,					///< Associated game object ID
			AkUInt32 in_uFlags = 0,							///< Bitmask: see \ref AkCallbackType
			AkCallbackFunc in_pfnCallback = NULL,			///< Callback function
			void * in_pCookie = NULL,						///< Callback cookie that will be sent to the callback function along with additional information
			AkUInt32 in_cExternals = 0,					///< Optional count of external source structures
			AkExternalSourceInfo *in_pExternalSources = NULL///< Optional array of external source resolution information
	        );

		/// Post an event to the sound engine (by event name), using callbacks.
		/// The callback function can be used to be noticed when markers are reached or when the event is finished.
		/// An array of wave file sources can be provided to resolve External Sources triggered by the event. 
        /// \return The playing ID of the event launched, or AK_INVALID_PLAYING_ID if posting the event failed
		/// \remarks
		/// If used, the array of external sources should contain the information for each external source triggered by the
		/// event.  When triggering an event with multiple external sources, you need to differentiate each source 
		/// by using the cookie property in the External Source in the Wwise project and in AkExternalSourceInfo.
		/// \aknote If an event triggers the playback of more than one external source, they must be named uniquely in the project 
		/// (therefore have a unique cookie) in order to tell them appart when filling the AkExternalSourceInfo structures.
		/// \sa 
		/// - \ref concept_events
		/// - \ref integrating_external_sources
		/// - AK::SoundEngine::RenderAudio()
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::GetSourcePlayPosition()
        extern AKSOUNDENGINE_API AkPlayingID PostEvent(
	        const wchar_t* in_pszEventName,					///< Name of the event (Unicode string)
	        AkGameObjectID in_gameObjectID,					///< Associated game object ID
			AkUInt32 in_uFlags = 0,							///< Bitmask: see \ref AkCallbackType
			AkCallbackFunc in_pfnCallback = NULL,			///< Callback function
			void * in_pCookie = NULL,						///< Callback cookie that will be sent to the callback function along with additional information.
			AkUInt32 in_cExternals = 0,						///< Optional count of external source structures
			AkExternalSourceInfo *in_pExternalSources = NULL///< Optional array of external source resolution information
	        );

		/// Post an event to the sound engine (by event name), using callbacks.
		/// The callback function can be used to be noticed when markers are reached or when the event is finished.
		/// An array of wave file sources can be provided to resolve External Sources triggered by the event.  P
        /// \return The playing ID of the event launched, or AK_INVALID_PLAYING_ID if posting the event failed
		/// \remarks
		/// If used, the array of external sources should contain the information for each external source triggered by the
		/// event.  When triggering an event with multiple external sources, you need to differentiate each source 
		/// by using the cookie property in the External Source in the Wwise project and in AkExternalSourceInfo.
		/// \aknote If an event triggers the playback of more than one external source, they must be named uniquely in the project 
		/// (therefore have a unique cookie) in order to tell them appart when filling the AkExternalSourceInfo structures.
		/// \sa 
		/// - \ref concept_events
		/// - \ref integrating_external_sources
		/// - AK::SoundEngine::RenderAudio()
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::GetSourcePlayPosition()
        extern AKSOUNDENGINE_API AkPlayingID PostEvent(
	        const char* in_pszEventName,					///< Name of the event (Ansi string)
	        AkGameObjectID in_gameObjectID,					///< Associated game object ID
			AkUInt32 in_uFlags = 0,							///< Bitmask: see \ref AkCallbackType
			AkCallbackFunc in_pfnCallback = NULL,			///< Callback function
			void * in_pCookie = NULL,						///< Callback cookie that will be sent to the callback function along with additional information.
			AkUInt32 in_cExternals = 0,						///< Optional count of external source structures
			AkExternalSourceInfo *in_pExternalSources = NULL///< Optional array of external source resolution information
	        );

		
		/// AkActionOnEventType
		/// \sa
		/// - AK::SoundEngine::ExecuteActionOnEvent()
		enum AkActionOnEventType
		{
			AkActionOnEventType_Stop	= 0,///< Stop
			AkActionOnEventType_Pause	= 1,///< Pause
			AkActionOnEventType_Resume	= 2	///< Resume
		};

		/// Executes an action on all nodes that are referenced in the specified event in an action of type play.
		/// \sa
		/// - AK::SoundEngine::AkActionOnEventType
		extern AKSOUNDENGINE_API AKRESULT ExecuteActionOnEvent(
			AkUniqueID in_eventID,												///< Unique ID of the event
			AkActionOnEventType in_ActionType,									///< Action to execute on all the elements that were played using the specified event.
	        AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT,			///< Associated game object ID
			AkTimeMs in_uTransitionDuration = 0,								///< Fade duration
			AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear	///< Curve type to be used for the transition
			);

		/// Executes an action on all nodes that are referenced in the specified event in an action of type play.
		/// \sa
		/// - AK::SoundEngine::AkActionOnEventType
		extern AKSOUNDENGINE_API AKRESULT ExecuteActionOnEvent(
			const wchar_t* in_pszEventName,										///< Name of the event (Unicode string)
			AkActionOnEventType in_ActionType,									///< Action to execute on all the elements that were played using the specified event.
	        AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT,			///< Associated game object ID
			AkTimeMs in_uTransitionDuration = 0,								///< Fade duration
			AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear	///< Curve type to be used for the transition
			);

		/// Executes an action on all nodes that are referenced in the specified event in an action of type play.
		/// \sa
		/// - AK::SoundEngine::AkActionOnEventType
		extern AKSOUNDENGINE_API AKRESULT ExecuteActionOnEvent(
			const char* in_pszEventName,										///< Name of the event (Ansi string)
			AkActionOnEventType in_ActionType,									///< Action to execute on all the elements that were played using the specified event.
	        AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT,			///< Associated game object ID
			AkTimeMs in_uTransitionDuration = 0,								///< Fade duration
			AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear	///< Curve type to be used for the transition
			);


		/// Seek inside all playing objects that are referenced in play actions of the specified event.
		///
		/// Notes:
		///		- This works with all objects of the actor-mixer hierarchy, and also with music segments. 
		///		- There is a restriction with sounds that play within a continuous sequence. Seeking is ignored 
		///			if one of their ancestors is a continuous (random or sequence) container with crossfade or 
		///			trigger rate transitions. Seeking is also ignored with sample-accurate transitions, unless
		///			the sound that is currently playing is the first sound of the sequence.
		///		- Seeking is also ignored with voices that can go virtual with "From Beginning" behavior. 
		///		- Sounds/segments are stopped if in_iPosition is greater than their duration.
		///		- in_iPosition is clamped internally to the beginning of the sound/segment.
		///		- If the option "Seek to nearest marker" is used, the seeking position snaps to the nearest marker.
		///			With objects of the actor-mixer hierarchy, markers are embedded in wave files by an external wave editor.
		///			Note that looping regions ("sampler loop") are not considered as markers. Also, the "add file name marker" of the 
		///			conversion settings dialog adds a marker at the beginning of the file, which is considered when seeking
		///			to nearest marker. In the case of objects of the interactive music hierarchy, user (wave) markers are ignored:
		///			seeking occurs to the nearest segment cue (authored in the segment editor), including the Entry Cue, but excluding the Exit Cue.
		///		- This method posts a command in the sound engine queue, thus seeking will not occur before 
		///			the audio thread consumes it (after a call to RenderAudio()). 
		///
		///	Notes specific to music segments:
		///		- With music segments, in_iPosition is relative to the Entry Cue, in milliseconds. Use a negative
		///			value to seek within the Pre-Entry.
		///		- Music segments cannot be looped. You may want to listen to the AK_EndOfEvent notification 
		///			in order to restart them if required.
		///		- In order to restart at the correct location, with all their tracks synchronized, music segments 
		///			take the "look-ahead time" property of their streamed tracks into account for seeking. 
		///			Consequently, the resulting position after a call to SeekOnEvent() might be earlier than the 
		///			value that was passed to the method. Use AK::MusicEngine::GetPlayingSegmentInfo() to query 
		///			the exact position of a segment. Also, the segment will be silent during that time
		///			(so that it restarts precisely at the position that you specified). AK::MusicEngine::GetPlayingSegmentInfo() 
		///			also informs you about the remaining look-ahead time. 
		///
		/// \sa
		/// - AK::SoundEngine::RenderAudio()
		/// - AK::SoundEngine::PostEvent()
		/// - AK::SoundEngine::GetSourcePlayPosition()
		/// - AK::MusicEngine::GetPlayingSegmentInfo()
		extern AKSOUNDENGINE_API AKRESULT SeekOnEvent( 
			AkUniqueID in_eventID,										///< Unique ID of the event
			AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT,	///< Associated game object ID; use AK_INVALID_GAME_OBJECT to affect all game objects
			AkTimeMs in_iPosition = 0,									///< Desired position where playback should restart, in milliseconds
			bool in_bSeekToNearestMarker = false						///< If true, the final seeking position will be made equal to the nearest marker (see note above)
			);

		/// Seek inside all playing objects that are referenced in play actions of the specified event.
		///
		/// Notes:
		///		- This works with all objects of the actor-mixer hierarchy, and also with music segments. 
		///		- There is a restriction with sounds that play within a continuous sequence. Seeking is ignored 
		///			if one of their ancestors is a continuous (random or sequence) container with crossfade or 
		///			trigger rate transitions. Seeking is also ignored with sample-accurate transitions, unless
		///			the sound that is currently playing is the first sound of the sequence.
		///		- Seeking is also ignored with voices that can go virtual with "From Beginning" behavior. 
		///		- With music segments, in_iPosition is relative to the Entry Cue, in milliseconds. Use a negative
		///			value to seek within the Pre-Entry.
		///		- Sounds/segments are stopped if in_iPosition is greater than their duration.
		///		- in_iPosition is clamped internally to the beginning of the sound/segment.
		///		- If the option "Seek to nearest marker" is used, the seeking position snaps to the nearest marker.
		///			With objects of the actor-mixer hierarchy, markers are embedded in wave files by an external wave editor.
		///			Note that looping regions ("sampler loop") are not considered as markers. Also, the "add file name marker" of the 
		///			conversion settings dialog adds a marker at the beginning of the file, which is considered when seeking
		///			to nearest marker. In the case of objects of the interactive music hierarchy, user (wave) markers are ignored:
		///			seeking occurs to the nearest segment cue (authored in the segment editor), including the Entry Cue, but excluding the Exit Cue.
		///		- This method posts a command in the sound engine queue, thus seeking will not occur before 
		///			the audio thread consumes it (after a call to RenderAudio()). 
		///
		///	Notes specific to music segments:
		///		- With music segments, in_iPosition is relative to the Entry Cue, in milliseconds. Use a negative
		///			value to seek within the Pre-Entry.
		///		- Music segments cannot be looped. You may want to listen to the AK_EndOfEvent notification 
		///			in order to restart them if required.
		///		- In order to restart at the correct location, with all their tracks synchronized, music segments 
		///			take the "look-ahead time" property of their streamed tracks into account for seeking. 
		///			Consequently, the resulting position after a call to SeekOnEvent() might be earlier than the 
		///			value that was passed to the method. Use AK::MusicEngine::GetPlayingSegmentInfo() to query 
		///			the exact position of a segment. Also, the segment will be silent during that time
		///			(so that it restarts precisely at the position that you specified). AK::MusicEngine::GetPlayingSegmentInfo() 
		///			also informs you about the remaining look-ahead time. 
		///
		/// \sa
		/// - AK::SoundEngine::RenderAudio()
		/// - AK::SoundEngine::PostEvent()
		/// - AK::SoundEngine::GetSourcePlayPosition()
		/// - AK::MusicEngine::GetPlayingSegmentInfo()
		extern AKSOUNDENGINE_API AKRESULT SeekOnEvent( 
			const wchar_t* in_pszEventName,								///< Name of the event (Unicode string)
			AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT,	///< Associated game object ID; use AK_INVALID_GAME_OBJECT to affect all game objects
			AkTimeMs in_iPosition = 0,									///< Desired position where playback should restart, in milliseconds
			bool in_bSeekToNearestMarker = false						///< If true, the final seeking position will be made equal to the nearest marker (see note above)
			);

		/// Seek inside all playing objects that are referenced in play actions of the specified event.
		///
		/// Notes:
		///		- This works with all objects of the actor-mixer hierarchy, and also with music segments. 
		///		- There is a restriction with sounds that play within a continuous sequence. Seeking is ignored 
		///			if one of their ancestors is a continuous (random or sequence) container with crossfade or 
		///			trigger rate transitions. Seeking is also ignored with sample-accurate transitions, unless
		///			the sound that is currently playing is the first sound of the sequence.
		///		- Seeking is also ignored with voices that can go virtual with "From Beginning" behavior. 
		///		- With music segments, in_iPosition is relative to the Entry Cue, in milliseconds. Use a negative
		///			value to seek within the Pre-Entry.
		///		- Sounds/segments are stopped if in_iPosition is greater than their duration.
		///		- in_iPosition is clamped internally to the beginning of the sound/segment.
		///		- If the option "Seek to nearest marker" is used, the seeking position snaps to the nearest marker.
		///			With objects of the actor-mixer hierarchy, markers are embedded in wave files by an external wave editor.
		///			Note that looping regions ("sampler loop") are not considered as markers. Also, the "add file name marker" of the 
		///			conversion settings dialog adds a marker at the beginning of the file, which is considered when seeking
		///			to nearest marker. In the case of objects of the interactive music hierarchy, user (wave) markers are ignored:
		///			seeking occurs to the nearest segment cue (authored in the segment editor), including the Entry Cue, but excluding the Exit Cue.
		///		- This method posts a command in the sound engine queue, thus seeking will not occur before 
		///			the audio thread consumes it (after a call to RenderAudio()). 
		///
		///	Notes specific to music segments:
		///		- With music segments, in_iPosition is relative to the Entry Cue, in milliseconds. Use a negative
		///			value to seek within the Pre-Entry.
		///		- Music segments cannot be looped. You may want to listen to the AK_EndOfEvent notification 
		///			in order to restart them if required.
		///		- In order to restart at the correct location, with all their tracks synchronized, music segments 
		///			take the "look-ahead time" property of their streamed tracks into account for seeking. 
		///			Consequently, the resulting position after a call to SeekOnEvent() might be earlier than the 
		///			value that was passed to the method. Use AK::MusicEngine::GetPlayingSegmentInfo() to query 
		///			the exact position of a segment. Also, the segment will be silent during that time
		///			(so that it restarts precisely at the position that you specified). AK::MusicEngine::GetPlayingSegmentInfo() 
		///			also informs you about the remaining look-ahead time. 
		///
		/// \sa
		/// - AK::SoundEngine::RenderAudio()
		/// - AK::SoundEngine::PostEvent()
		/// - AK::SoundEngine::GetSourcePlayPosition()
		/// - AK::MusicEngine::GetPlayingSegmentInfo()
		extern AKSOUNDENGINE_API AKRESULT SeekOnEvent( 
			const char* in_pszEventName,								///< Name of the event (Ansi string)
			AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT,	///< Associated game object ID; use AK_INVALID_GAME_OBJECT to affect all game objects
			AkTimeMs in_iPosition = 0,									///< Desired position where playback should restart, in milliseconds
			bool in_bSeekToNearestMarker = false						///< If true, the final seeking position will be made equal to the nearest marker (see note above)
			);

		/// Seek inside all playing objects that are referenced in play actions of the specified event.
		/// Seek position is specified as a percentage of the sound's total duration, and takes looping into account. 
		///
		/// Notes:
		///		- This works with all objects of the actor-mixer hierarchy, and also with music segments. 
		///		- There is a restriction with sounds that play within a continuous sequence. Seeking is ignored 
		///			if one of their ancestors is a continuous (random or sequence) container with crossfade or 
		///			trigger rate transitions. Seeking is also ignored with sample-accurate transitions, unless
		///			the sound that is currently playing is the first sound of the sequence.
		///		- Seeking is also ignored with voices that can go virtual with "From Beginning" behavior. 
		///		- in_iPosition is clamped internally to the beginning of the sound/segment.
		///		- If the option "Seek to nearest marker" is used, the seeking position snaps to the nearest marker.
		///			With objects of the actor-mixer hierarchy, markers are embedded in wave files by an external wave editor.
		///			Note that looping regions ("sampler loop") are not considered as markers. Also, the "add file name marker" of the 
		///			conversion settings dialog adds a marker at the beginning of the file, which is considered when seeking
		///			to nearest marker. In the case of objects of the interactive music hierarchy, user (wave) markers are ignored:
		///			seeking occurs to the nearest segment cue (authored in the segment editor), including the Entry Cue, but excluding the Exit Cue.
		///		- This method posts a command in the sound engine queue, thus seeking will not occur before 
		///			the audio thread consumes it (after a call to RenderAudio()). 
		///
		///	Notes specific to music segments:
		///		- With music segments, in_fPercent is relative to the Entry Cue, and the segment's duration is the 
		///			duration between its entry and exit cues. Consequently, you cannot seek within the pre-entry or
		///			post-exit of a segment using this method. Use absolute values instead.
		///		- Music segments cannot be looped. You may want to listen to the AK_EndOfEvent notification 
		///			in order to restart them if required.
		///		- In order to restart at the correct location, with all their tracks synchronized, music segments 
		///			take the "look-ahead time" property of their streamed tracks into account for seeking. 
		///			Consequently, the resulting position after a call to SeekOnEvent() might be earlier than the 
		///			value that was passed to the method. Use AK::MusicEngine::GetPlayingSegmentInfo() to query 
		///			the exact position of a segment. Also, the segment will be silent during the time that period
		///			(so that it restarts precisely at the position that you specified). AK::MusicEngine::GetPlayingSegmentInfo() 
		///			also informs you about the remaining look-ahead time. 
		///
		/// \sa
		/// - AK::SoundEngine::RenderAudio()
		/// - AK::SoundEngine::PostEvent()
		/// - AK::SoundEngine::GetSourcePlayPosition()
		/// - AK::MusicEngine::GetPlayingSegmentInfo()
		extern AKSOUNDENGINE_API AKRESULT SeekOnEvent( 
			AkUniqueID in_eventID,										///< Unique ID of the event
			AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT,	///< Associated game object ID; use AK_INVALID_GAME_OBJECT to affect all game objects
			AkReal32 in_fPercent = 0.f,									///< Desired position where playback should restart, expressed in a percentage of the file's total duration, between 0 and 1.f (see note above about infinite looping sounds)
			bool in_bSeekToNearestMarker = false						///< If true, the final seeking position will be made equal to the nearest marker (see note above)
			);

		/// Seek inside all playing objects that are referenced in play actions of the specified event.
		/// Seek position is specified as a percentage of the sound's total duration, and takes looping into account. 
		///
		/// Notes:
		///		- This works with all objects of the actor-mixer hierarchy, and also with music segments. 
		///		- There is a restriction with sounds that play within a continuous sequence. Seeking is ignored 
		///			if one of their ancestors is a continuous (random or sequence) container with crossfade or 
		///			trigger rate transitions. Seeking is also ignored with sample-accurate transitions, unless
		///			the sound that is currently playing is the first sound of the sequence.
		///		- Seeking is also ignored with voices that can go virtual with "From Beginning" behavior. 
		///		- If the option "Seek to nearest marker" is used, the seeking position snaps to the nearest marker.
		///			With objects of the actor-mixer hierarchy, markers are embedded in wave files by an external wave editor.
		///			Note that looping regions ("sampler loop") are not considered as markers. Also, the "add file name marker" of the 
		///			conversion settings dialog adds a marker at the beginning of the file, which is considered when seeking
		///			to nearest marker. In the case of objects of the interactive music hierarchy, user (wave) markers are ignored:
		///			seeking occurs to the nearest segment cue (authored in the segment editor), including the Entry Cue, but excluding the Exit Cue.
		///		- This method posts a command in the sound engine queue, thus seeking will not occur before 
		///			the audio thread consumes it (after a call to RenderAudio()). 
		///
		///	Notes specific to music segments:
		///		- With music segments, in_fPercent is relative to the Entry Cue, and the segment's duration is the 
		///			duration between its entry and exit cues. Consequently, you cannot seek within the pre-entry or
		///			post-exit of a segment using this method. Use absolute values instead.
		///		- Music segments cannot be looped. You may want to listen to the AK_EndOfEvent notification 
		///			in order to restart them if required.
		///		- In order to restart at the correct location, with all their tracks synchronized, music segments 
		///			take the "look-ahead time" property of their streamed tracks into account for seeking. 
		///			Consequently, the resulting position after a call to SeekOnEvent() might be earlier than the 
		///			value that was passed to the method. Use AK::MusicEngine::GetPlayingSegmentInfo() to query 
		///			the exact position of a segment. Also, the segment will be silent during the time that period
		///			(so that it restarts precisely at the position that you specified). AK::MusicEngine::GetPlayingSegmentInfo() 
		///			also informs you about the remaining look-ahead time. 
		///
		/// \sa
		/// - AK::SoundEngine::RenderAudio()
		/// - AK::SoundEngine::PostEvent()
		/// - AK::SoundEngine::GetSourcePlayPosition()
		/// - AK::MusicEngine::GetPlayingSegmentInfo()
		extern AKSOUNDENGINE_API AKRESULT SeekOnEvent( 
			const wchar_t* in_pszEventName,								///< Name of the event (Unicode string)
			AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT,	///< Associated game object ID; use AK_INVALID_GAME_OBJECT to affect all game objects
			AkReal32 in_fPercent = 0.f,									///< Desired position where playback should restart, expressed in a percentage of the file's total duration, between 0 and 1.f (see note above about infinite looping sounds)
			bool in_bSeekToNearestMarker = false						///< If true, the final seeking position will be made equal to the nearest marker (see note above)
			);

		/// Seek inside all playing objects that are referenced in play actions of the specified event.
		/// Seek position is specified as a percentage of the sound's total duration, and takes looping into account. 
		///
		/// Notes:
		///		- This works with all objects of the actor-mixer hierarchy, and also with music segments. 
		///		- There is a restriction with sounds that play within a continuous sequence. Seeking is ignored 
		///			if one of their ancestors is a continuous (random or sequence) container with crossfade or 
		///			trigger rate transitions. Seeking is also ignored with sample-accurate transitions, unless
		///			the sound that is currently playing is the first sound of the sequence.
		///		- Seeking is also ignored with voices that can go virtual with "From Beginning" behavior. 
		///		- If the option "Seek to nearest marker" is used, the seeking position snaps to the nearest marker.
		///			With objects of the actor-mixer hierarchy, markers are embedded in wave files by an external wave editor.
		///			Note that looping regions ("sampler loop") are not considered as markers. Also, the "add file name marker" of the 
		///			conversion settings dialog adds a marker at the beginning of the file, which is considered when seeking
		///			to nearest marker. In the case of objects of the interactive music hierarchy, user (wave) markers are ignored:
		///			seeking occurs to the nearest segment cue (authored in the segment editor), including the Entry Cue, but excluding the Exit Cue.
		///		- This method posts a command in the sound engine queue, thus seeking will not occur before 
		///			the audio thread consumes it (after a call to RenderAudio()). 
		///
		///	Notes specific to music segments:
		///		- With music segments, in_fPercent is relative to the Entry Cue, and the segment's duration is the 
		///			duration between its entry and exit cues. Consequently, you cannot seek within the pre-entry or
		///			post-exit of a segment using this method. Use absolute values instead.
		///		- Music segments cannot be looped. You may want to listen to the AK_EndOfEvent notification 
		///			in order to restart them if required.
		///		- In order to restart at the correct location, with all their tracks synchronized, music segments 
		///			take the "look-ahead time" property of their streamed tracks into account for seeking. 
		///			Consequently, the resulting position after a call to SeekOnEvent() might be earlier than the 
		///			value that was passed to the method. Use AK::MusicEngine::GetPlayingSegmentInfo() to query 
		///			the exact position of a segment. Also, the segment will be silent during the time that period
		///			(so that it restarts precisely at the position that you specified). AK::MusicEngine::GetPlayingSegmentInfo() 
		///			also informs you about the remaining look-ahead time. 
		///
		/// \sa
		/// - AK::SoundEngine::RenderAudio()
		/// - AK::SoundEngine::PostEvent()
		/// - AK::SoundEngine::GetSourcePlayPosition()
		/// - AK::MusicEngine::GetPlayingSegmentInfo()
		extern AKSOUNDENGINE_API AKRESULT SeekOnEvent( 
			const char* in_pszEventName,								///< Name of the event (Ansi string)
			AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT,	///< Associated game object ID; use AK_INVALID_GAME_OBJECT to affect all game objects
			AkReal32 in_fPercent = 0.f,									///< Desired position where playback should restart, expressed in a percentage of the file's total duration, between 0 and 1.f (see note above about infinite looping sounds)
			bool in_bSeekToNearestMarker = false						///< If true, the final seeking position will be made equal to the nearest marker (see notes above).
			);

		/// Cancel all event callbacks associated with a specific callback cookie.\n
		/// \sa 
		/// - AK::SoundEngine::PostEvent()
		extern AKSOUNDENGINE_API void CancelEventCallbackCookie( 
			void * in_pCookie 							///< Callback cookie to be cancelled
			);

		/// Cancel all event callbacks for a specific playing ID.
		/// \sa 
		/// - AK::SoundEngine::PostEvent()
		extern AKSOUNDENGINE_API void CancelEventCallback( 
			AkPlayingID in_playingID 					///< Playing ID of the event that must not use callbacks
			);

		/// Get the current position of the source associated with this playing ID, obtained from PostEvent().
		/// Notes:
		/// - You need to pass AK_EnableGetSourcePlayPosition to PostEvent() in order to use this function, otherwise
		/// 	it returns AK_Fail, even if the playing ID is valid.
		/// - The source's position is updated at every audio frame, and the time at which this occurs is stored. 
		///		When you call this function from your thread, you therefore query the position that was updated in the previous audio frame.
		///		If in_bExtrapolate is true (default), the returned position is extrapolated using the elapsed time since last 
		///		sound engine update and the source's playback rate.
		/// \return AK_Success if successful.
		///			It returns AK_InvalidParameter if the provided pointer is not valid.
		///			It returns AK_Fail if an error occured.
		/// \sa 
		/// - \ref soundengine_query_pos
		/// - \ref concept_events
		extern AKSOUNDENGINE_API AKRESULT GetSourcePlayPosition(
			AkPlayingID		in_PlayingID,				///< Playing ID returned by AK::SoundEngine::PostEvent()
			AkTimeMs*		out_puPosition,				///< Position of the source (in ms) associated with that playing ID
			bool			in_bExtrapolate = true		///< Position is extrapolated based on time elapsed since last sound engine update.
			);

		/// Stop the current content playing associated to the specified game object ID.
		/// If no game object is specified, all sounds will be stopped.
		extern AKSOUNDENGINE_API void StopAll( 
			AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT ///< (Optional)Specify a game object to stop only playback associated to the provided game object ID.
			);

		/// Stop the current content playing associated to the specified playing ID.
		extern AKSOUNDENGINE_API void StopPlayingID( 
			AkPlayingID in_playingID,											///< Playing ID to be stopped.
			AkTimeMs in_uTransitionDuration = 0,								///< Fade duration
			AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear	///< Curve type to be used for the transition
			);

		//@}

		////////////////////////////////////////////////////////////////////////
		/// @name Game Objects
		//@{
		
        /// Register a game object.
		/// \return
		/// - AK_Success if successful
		///	- AK_Fail if the specified AkGameObjectID is invalid (0 and -1 are invalid)
		/// \remark Registering a game object twice does nothing. Unregistering it once unregisters it no 
		///			matter how many times it has been registered.
		/// \sa 
		/// - AK::SoundEngine::UnregisterGameObj()
		/// - AK::SoundEngine::UnregisterAllGameObj()
		/// - \ref concept_gameobjects
        extern AKSOUNDENGINE_API AKRESULT RegisterGameObj(
	        AkGameObjectID in_gameObjectID				///< ID of the game object to be registered
	        );

		/// Register a game object.
		/// \return
		/// - AK_Success if successful
		///	- AK_Fail if the specified AkGameObjectID is invalid (0 and -1 are invalid)
		/// \remark Registering a game object twice does nothing. Unregistering it once unregisters it no 
		///			matter how many times it has been registered.
		/// \sa 
		/// - AK::SoundEngine::UnregisterGameObj()
		/// - AK::SoundEngine::UnregisterAllGameObj()
		/// - \ref concept_gameobjects
        extern AKSOUNDENGINE_API AKRESULT RegisterGameObj(
	        AkGameObjectID in_gameObjectID,				///< ID of the game object to be registered
			const char * in_pszObjName					///< Name of the game object (for monitoring purpose)
	        );

        /// Unregister a game object.
		/// \return 
		/// - AK_Success if successful
		///	- AK_Fail if the specified AkGameObjectID is invalid (0 is an invalid ID)
		/// \remark Registering a game object twice does nothing. Unregistering it once unregisters it no 
		///			matter how many times it has been registered. Unregistering a game object while it is 
		///			in use is allowed, but the control over the parameters of this game object is lost.
		///			For example, say a sound associated with this game object is a 3D moving sound. This sound will 
		///			stop moving when the game object is unregistered, and there will be no way to regain control over the game object.
		/// \sa 
		/// - AK::SoundEngine::RegisterGameObj()
		/// - AK::SoundEngine::UnregisterAllGameObj()
		/// - \ref concept_gameobjects
        extern AKSOUNDENGINE_API AKRESULT UnregisterGameObj(
	        AkGameObjectID in_gameObjectID				///< ID of the game object to be unregistered. Use 
	        											/// AK_INVALID_GAME_OBJECT to unregister all game objects.
	        );

        /// Unregister all game objects.
		/// \return Always returns AK_Success
		/// \remark Registering a game object twice does nothing. Unregistering it once unregisters it no 
		///			matter how many times it has been registered. Unregistering a game object while it is 
		///			in use is allowed, but the control over the parameters of this game object is lost.
		///			For example, if a sound associated with this game object is a 3D moving sound, it will 
		///			stop moving once the game object is unregistered, and there will be no way to recover 
		///			the control over this game object.
		/// \sa 
		/// - AK::SoundEngine::RegisterGameObj()
		/// - AK::SoundEngine::UnregisterGameObj()
		/// - \ref concept_gameobjects
        extern AKSOUNDENGINE_API AKRESULT UnregisterAllGameObj();

       	/// Set the position of a game object.
		/// \return Always returns AK_Success
		/// \sa 
		/// - \ref soundengine_3dpositions
        extern AKSOUNDENGINE_API AKRESULT SetPosition( 
			AkGameObjectID in_GameObjectID,				///< Game object identifier
			const AkSoundPosition & in_Position,		///< Position to set
			AkUInt32 in_ulListenerIndex = AK_INVALID_LISTENER_INDEX	///< If the listener index is valid, the listener position is used instead of the game object position.
		    );

		/// MultiPositionType.
		/// \sa
		/// - AK::SoundEngine::SetMultiplePosition()
		/// - \ref soundengine_3dpositions_multiplepos
		enum MultiPositionType
		{
			MultiPositionType_SingleSource,		///< Used for normal sounds, not expected to pass to AK::SoundEngine::SetMultiplePosition() (if done, only the first position will be used).
			MultiPositionType_MultiSources,		///< Simulate multiple sources in one sound playing, adding volumes. For instance, all the torches on your level emitting using only one sound.
			MultiPositionType_MultiDirections,	///< Simulate one sound coming from multiple directions. Useful for repositionning sounds based on wall openings or to simulate areas like forest or rivers ( in combination with spreading in the attenuation of the sounds ).
		};

		/// Set multiple positions to a single game object.
		/// Setting multiple position on a single game object is a way to simulate multiple emission sources while using the ressources of only one voice.
		/// This can be used to simulate wall openings, area sounds, or multiple objects emiting the same sound in the same area.
		/// \aknote Calling AK::SoundEngine::SetMultiplePositions() with only one position is the same than calling AK::SoundEngine::SetPosition() \endaknote
		/// \return 
		/// - returns AK_Success when successful
		/// - return AK_InvalidParameter if parameters are not valid.
		/// \sa 
		/// - \ref soundengine_3dpositions
		/// - \ref soundengine_3dpositions_multiplepos
		/// - \ref AK::SoundEngine::MultiPositionType
        extern AKSOUNDENGINE_API AKRESULT SetMultiplePositions( 
			AkGameObjectID in_GameObjectID,						///< Game object identifier.
			const AkSoundPosition * in_pPositions,				///< Array of positions to apply.
			AkUInt16 in_NumPositions,							///< Number of positions specified in the provided array.
			MultiPositionType in_eMultiPositionType = MultiPositionType_MultiDirections ///< \ref AK::SoundEngine::MultiPositionType
		    );

        /// Set the scaling factor of a game object.
		/// Modify the attenuation computations on this game object to simulate sounds with a a larger or smaller area of effect.
		/// \return 
		/// - AK_Success when successful
		/// - AK_InvalidParameter if the scaling factor specified was 0 or negative.
		extern AKSOUNDENGINE_API AKRESULT SetAttenuationScalingFactor(
			AkGameObjectID in_GameObjectID,				///< Game object identifier
			AkReal32 in_fAttenuationScalingFactor		///< Scaling Factor, 1 means 100%, 0.5 means 50%, 2 means 200%, and so on.
			);

        /// Set the scaling factor for a listener.
		/// A larger factor means that the listener will hear sounds with less attenuation based on distance.
		/// \return 
		/// - AK_Success when successful
		/// - AK_InvalidParameter if the scaling factor specified was 0 or negative.
		extern AKSOUNDENGINE_API AKRESULT SetListenerScalingFactor(
			AkUInt32 in_uListenerIndex,				///< ListenerIndex
			AkReal32 in_fListenerScalingFactor		///< Scaling Factor, 1 means 100%, 0.5 means 50%, 2 means 200%, and so on.
			);
        //@}

		////////////////////////////////////////////////////////////////////////
		/// @name Bank Management
		//@{

		/// Unload all currently loaded banks.
		/// It also internally calls ClearPreparedEvents() since at least one bank must have been loaded to allow preparing events.
		/// \return 
		/// - AK_Success if successful
		///	- AK_Fail if the sound engine was not correctly initialized or if there is not enough memory to handle the command
		/// \sa 
		/// - AK::SoundEngine::UnloadBank()
		/// - AK::SoundEngine::LoadBank()
		/// - \ref soundengine_banks
		extern AKSOUNDENGINE_API AKRESULT ClearBanks();

		/// Set the I/O settings of the bank load and prepare event processes.
        /// The sound engine uses default values unless explicitly set by calling this method.
		/// \warning This function must be called before loading banks.
		/// \return 
		/// - AK_Success if successful
		/// - AK_Fail if the sound engine was not correctly initialized
		/// - AK_InvalidParameter if some parameters are invalid
		/// \sa 
		/// - \ref soundengine_banks
        /// - \ref streamingdevicemanager
        extern AKSOUNDENGINE_API AKRESULT SetBankLoadIOSettings(
            AkReal32            in_fThroughput,         ///< Average throughput of bank data streaming (bytes/ms) (the default value is AK_DEFAULT_BANK_THROUGHPUT)
            AkPriority          in_priority             ///< Priority of bank streaming (the default value is AK_DEFAULT_PRIORITY)
            );

		/// Load a bank synchronously (by unicode string).\n
		/// The bank name is passed to the Stream Manager.
		/// Refer to \ref soundengine_banks_general for a discussion on using strings and IDs.
		/// You can specify a custom pool for storage of media, the engine will create a new pool if AK_DEFAULT_POOL_ID is passed.
		/// A bank load request will be posted, and consumed by the Bank Manager thread.
		/// The function returns when the request has been completely processed.
		/// \return 
		/// The bank ID, which is obtained by hashing the bank name (see GetIDFromString()). 
		/// You may use this ID with UnloadBank().
		///	- AK_Success: Load or unload successful.
		/// - AK_InsufficientMemory: Insufficient memory to store bank data.
		/// - AK_BankReadError: I/O error.
		/// - AK_WrongBankVersion: Invalid bank version: make sure the version of Wwise that 
		/// you used to generate the SoundBanks matches that of the SDK you are currently using.
		/// - AK_InvalidFile: File specified could not be opened.
		/// - AK_InvalidParameter: Invalid parameter, invalid memory alignment.		
		/// - AK_Fail: Load or unload failed for any other reason.
		/// \remarks
		/// - The initialization bank must be loaded first.
		/// - All SoundBanks subsequently loaded must come from the same Wwise project as the
		///   initialization bank. If you need to load SoundBanks from a different project, you
		///   must first unload ALL banks, including the initialization bank, then load the
		///   initialization bank from the other project, and finally load banks from that project.
		/// - Codecs and plug-ins must be registered before loading banks that use them.
		/// - Loading a bank referencing an unregistered plug-in or codec will result in a load bank success,
		/// but the plug-ins will not be used. More specifically, playing a sound that uses an unregistered effect plug-in 
		/// will result in audio playback without applying the said effect. If an unregistered source plug-in is used by an event's audio objects, 
		/// posting the event will fail.
		/// - The sound engine internally calls GetIDFromString(in_pszString) to return the correct bank ID.
		/// Therefore, in_pszString should be the real name of the SoundBank (with or without the "bnk" extension - it is trimmed internally),
		/// not the name of the file (if you changed it), nor the full path of the file. The path should be resolved in 
		/// your implementation of the Stream Manager, or in the Low-Level I/O module if you use the default Stream Manager's implementation.
		/// - Requesting to load a bank in a different memory pool than where the bank was previously loaded must be done only
		/// after receiving confirmation by the callback that the bank was completely unloaded or by usung synchrone versions
		/// of the UnloadBank function.
		/// \sa 
		/// - AK::SoundEngine::UnloadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - AK::SoundEngine::GetIDFromString
		/// - AK::MemoryMgr::CreatePool()
		/// - \ref soundengine_banks
		/// - \ref integrating_elements_plugins
		/// - \ref streamingdevicemanager
		/// - \ref streamingmanager_lowlevel
		/// - \ref sdk_bank_training
		extern AKSOUNDENGINE_API AKRESULT LoadBank(
	        const wchar_t*      in_pszString,		    ///< Name of the bank to load (Unicode string)
            AkMemPoolId         in_memPoolId,			///< Memory pool ID (the pool is created if AK_DEFAULT_POOL_ID is passed)
            AkBankID &          out_bankID				///< Returned bank ID
	        );

		/// Load a bank synchronously (by ansi string).\n
		/// The bank name is passed to the Stream Manager.
		/// Refer to \ref soundengine_banks_general for a discussion on using strings and IDs.
		/// You can specify a custom pool for storage of media, the engine will create a new pool if AK_DEFAULT_POOL_ID is passed.
		/// A bank load request will be posted, and consumed by the Bank Manager thread.
		/// The function returns when the request has been completely processed.
		/// \return 
		/// The bank ID, which is obtained by hashing the bank name (see GetIDFromString()). 
		/// You may use this ID with UnloadBank().
		///	- AK_Success: Load or unload successful.
		/// - AK_InsufficientMemory: Insufficient memory to store bank data.
		/// - AK_BankReadError: I/O error.
		/// - AK_WrongBankVersion: Invalid bank version: make sure the version of Wwise that 
		/// you used to generate the SoundBanks matches that of the SDK you are currently using.
		/// - AK_InvalidFile: File specified could not be opened.
		/// - AK_InvalidParameter: Invalid parameter, invalid memory alignment.		
		/// - AK_Fail: Load or unload failed for any other reason.
		/// \remarks
		/// - The initialization bank must be loaded first.
		/// - All SoundBanks subsequently loaded must come from the same Wwise project as the
		///   initialization bank. If you need to load SoundBanks from a different project, you
		///   must first unload ALL banks, including the initialization bank, then load the
		///   initialization bank from the other project, and finally load banks from that project.
		/// - Codecs and plug-ins must be registered before loading banks that use them.
		/// - Loading a bank referencing an unregistered plug-in or codec will result in a load bank success,
		/// but the plug-ins will not be used. More specifically, playing a sound that uses an unregistered effect plug-in 
		/// will result in audio playback without applying the said effect. If an unregistered source plug-in is used by an event's audio objects, 
		/// posting the event will fail.
		/// - The sound engine internally calls GetIDFromString(in_pszString) to return the correct bank ID.
		/// Therefore, in_pszString should be the real name of the SoundBank (with or without the "bnk" extension - it is trimmed internally),
		/// not the name of the file (if you changed it), nor the full path of the file. The path should be resolved in 
		/// your implementation of the Stream Manager, or in the Low-Level I/O module if you use the default Stream Manager's implementation.
		/// - Requesting to load a bank in a different memory pool than where the bank was previously loaded must be done only
		/// after receiving confirmation by the callback that the bank was completely unloaded or by usung synchrone versions
		/// of the UnloadBank function.
		/// \sa 
		/// - AK::SoundEngine::UnloadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - AK::SoundEngine::GetIDFromString
		/// - AK::MemoryMgr::CreatePool()
		/// - \ref soundengine_banks
		/// - \ref integrating_elements_plugins
		/// - \ref streamingdevicemanager
		/// - \ref streamingmanager_lowlevel
		/// - \ref sdk_bank_training
		extern AKSOUNDENGINE_API AKRESULT LoadBank(
	        const char*         in_pszString,		    ///< Name of the bank to load (Ansi string)
            AkMemPoolId         in_memPoolId,			///< Memory pool ID (the pool is created if AK_DEFAULT_POOL_ID is passed)
            AkBankID &          out_bankID				///< Returned bank ID
	        );

        /// Load a bank synchronously (by ID).\n
		/// The bank ID is passed to the Stream Manager.
		/// Refer to \ref soundengine_banks_general for a discussion on using strings and IDs.
		/// You can specify a custom pool for storage of media, the engine will create a new pool if AK_DEFAULT_POOL_ID is passed.
		/// A bank load request will be posted, and consumed by the Bank Manager thread.
		/// The function returns when the request has been completely processed.
		/// \return 
		///	- AK_Success: Load or unload successful.
		/// - AK_InsufficientMemory: Insufficient memory to store bank data.
		/// - AK_BankReadError: I/O error.
		/// - AK_WrongBankVersion: Invalid bank version: make sure the version of Wwise that 
		/// you used to generate the SoundBanks matches that of the SDK you are currently using.
		/// - AK_InvalidFile: File specified could not be opened.
		/// - AK_InvalidParameter: Invalid parameter, invalid memory alignment.		
		/// - AK_Fail: Load or unload failed for any other reason.
		/// \remarks
		/// - The initialization bank must be loaded first.
		/// - All SoundBanks subsequently loaded must come from the same Wwise project as the
		///   initialization bank. If you need to load SoundBanks from a different project, you
		///   must first unload ALL banks, including the initialization bank, then load the
		///   initialization bank from the other project, and finally load banks from that project.
		/// - Codecs and plug-ins must be registered before loading banks that use them.
		/// - Loading a bank referencing an unregistered plug-in or codec will result in a load bank success,
		/// but the plug-ins will not be used. More specifically, playing a sound that uses an unregistered effect plug-in 
		/// will result in audio playback without applying the said effect. If an unregistered source plug-in is used by an event's audio objects, 
		/// posting the event will fail.
		/// - Requesting to load a bank in a different memory pool than where the bank was previously loaded must be done only
		/// after receiving confirmation by the callback that the bank was completely unloaded or by usung synchrone versions
		/// of the UnloadBank function.
		/// \sa 
		/// - AK::SoundEngine::UnloadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - AK::MemoryMgr::CreatePool()
		/// - \ref soundengine_banks
		/// - \ref integrating_elements_plugins
		/// - \ref sdk_bank_training
		extern AKSOUNDENGINE_API AKRESULT LoadBank(
	        AkBankID			in_bankID,              ///< Bank ID of the bank to load
            AkMemPoolId         in_memPoolId			///< Memory pool ID (the pool is created if AK_DEFAULT_POOL_ID is passed)
            );

		/// Load a bank synchronously (by in-memory data).\n
		/// Use this overload when you want to manage I/O on your side. Load the bank file
		/// in a buffer and pass its address to the sound engine.
		/// In-memory loading is in-place: the memory must be valid until the bank is unloaded.
		/// A bank load request will be posted, and consumed by the Bank Manager thread.
		/// The function returns when the request has been completely processed.
		/// \return 
		/// The bank ID, which is stored in the first few bytes of the bank file. You may use this 
		/// ID with UnloadBank().
		///	- AK_Success: Load or unload successful.
		/// - AK_InsufficientMemory: Insufficient memory to store bank data.
		/// - AK_BankReadError: I/O error.
		/// - AK_WrongBankVersion: Invalid bank version: make sure the version of Wwise that 
		/// you used to generate the SoundBanks matches that of the SDK you are currently using.
		/// - AK_InvalidFile: File specified could not be opened.
		/// - AK_InvalidParameter: Invalid parameter, invalid memory alignment.		
		/// - AK_Fail: Load or unload failed for any other reason.
		/// \remarks
		/// - The initialization bank must be loaded first.
		/// - All SoundBanks subsequently loaded must come from the same Wwise project as the
		///   initialization bank. If you need to load SoundBanks from a different project, you
		///   must first unload ALL banks, including the initialization bank, then load the
		///   initialization bank from the other project, and finally load banks from that project.
		/// - Codecs and plug-ins must be registered before loading banks that use them.
		/// - Loading a bank referencing an unregistered plug-in or codec will result in a load bank success,
		/// but the plug-ins will not be used. More specifically, playing a sound that uses an unregistered effect plug-in 
		/// will result in audio playback without applying the said effect. If an unregistered source plug-in is used by an event's audio objects, 
		/// posting the event will fail.
		/// - The memory must be aligned on platform-specific AK_BANK_PLATFORM_DATA_ALIGNMENT bytes (see AkTypes.h).
		/// - Requesting to load a bank in a different memory pool than where the bank was previously loaded must be done only
		/// after receiving confirmation by the callback that the bank was completely unloaded or by using synchrone versions
		/// of the UnloadBank function.
		/// - Avoid using this function for banks containing a lot of events or structure data.  
		///	  This data will be loaded in the Default Pool anyway thus duplicating memory (one copy in the Default Pool 
		///   and one in the block you provided).  For event/structure-only banks, prefer the other versions of LoadBank().
		/// \sa 
		/// - AK::SoundEngine::UnloadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - \ref soundengine_banks
		/// - \ref integrating_elements_plugins
		/// - \ref sdk_bank_training
		extern AKSOUNDENGINE_API AKRESULT LoadBank(
	        const void *		in_pInMemoryBankPtr,	///< Pointer to the in-memory bank to load
			AkUInt32			in_uInMemoryBankSize,	///< Size of the in-memory bank to load
            AkBankID &          out_bankID				///< Returned bank ID
	        );

        /// Load a bank asynchronously (by unicode string).\n
		/// The bank name is passed to the Stream Manager.
		/// Refer to \ref soundengine_banks_general for a discussion on using strings and IDs.
		/// You can specify a custom pool for storage of media, the engine will create a new pool if AK_DEFAULT_POOL_ID is passed.
		/// A bank load request will be posted to the Bank Manager consumer thread.
		/// The function returns immediately.
		/// \return 
		/// AK_Success if the scheduling was successful, AK_Fail otherwise.
		/// Use a callback to be notified when completed, and get the status of the request.
		/// The bank ID, which is obtained by hashing the bank name (see GetIDFromString()). 
		/// You may use this ID with UnloadBank().
		/// \remarks
		/// - The initialization bank must be loaded first.
		/// - All SoundBanks subsequently loaded must come from the same Wwise project as the
		///   initialization bank. If you need to load SoundBanks from a different project, you
		///   must first unload ALL banks, including the initialization bank, then load the
		///   initialization bank from the other project, and finally load banks from that project.
		/// - Codecs and plug-ins must be registered before loading banks that use them.
		/// - Loading a bank referencing an unregistered plug-in or codec will result in a load bank success,
		/// but the plug-ins will not be used. More specifically, playing a sound that uses an unregistered effect plug-in 
		/// will result in audio playback without applying the said effect. If an unregistered source plug-in is used by an event's audio objects, 
		/// posting the event will fail.
		/// - The sound engine internally calls GetIDFromString(in_pszString) to return the correct bank ID.
		/// Therefore, in_pszString should be the real name of the SoundBank (with or without the "bnk" extension - it is trimmed internally),
		/// not the name of the file (if you changed it), nor the full path of the file. The path should be resolved in 
		/// your implementation of the Stream Manager, or in the Low-Level I/O module if you use the default Stream Manager's implementation.
		/// - Requesting to load a bank in a different memory pool than where the bank was previously loaded must be done only
		/// after receiving confirmation by the callback that the bank was completely unloaded or by usung synchrone versions
		/// of the UnloadBank function.
		/// \sa 
		/// - AK::SoundEngine::UnloadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - AK::MemoryMgr::CreatePool()
		/// - AkBankCallbackFunc
		/// - \ref soundengine_banks
		/// - \ref integrating_elements_plugins
		/// - \ref streamingdevicemanager
		/// - \ref streamingmanager_lowlevel
		/// - \ref sdk_bank_training
		extern AKSOUNDENGINE_API AKRESULT LoadBank(
	        const wchar_t*      in_pszString,           ///< Name/path of the bank to load (Unicode string)
			AkBankCallbackFunc  in_pfnBankCallback,	    ///< Callback function
			void *              in_pCookie,				///< Callback cookie (reserved to user, passed to the callback function)
            AkMemPoolId         in_memPoolId,			///< Memory pool ID (the pool is created if AK_DEFAULT_POOL_ID is passed)
			AkBankID &          out_bankID				///< Returned bank ID
	        );

        /// Load a bank asynchronously (by ansi string).\n
		/// The bank name is passed to the Stream Manager.
		/// Refer to \ref soundengine_banks_general for a discussion on using strings and IDs.
		/// You can specify a custom pool for storage of media, the engine will create a new pool if AK_DEFAULT_POOL_ID is passed.
		/// A bank load request will be posted to the Bank Manager consumer thread.
		/// The function returns immediately.
		/// \return 
		/// AK_Success if the scheduling was successful, AK_Fail otherwise.
		/// Use a callback to be notified when completed, and get the status of the request.
		/// The bank ID, which is obtained by hashing the bank name (see GetIDFromString()). 
		/// You may use this ID with UnloadBank().
		/// \remarks
		/// - The initialization bank must be loaded first.
		/// - All SoundBanks subsequently loaded must come from the same Wwise project as the
		///   initialization bank. If you need to load SoundBanks from a different project, you
		///   must first unload ALL banks, including the initialization bank, then load the
		///   initialization bank from the other project, and finally load banks from that project.
		/// - Codecs and plug-ins must be registered before loading banks that use them.
		/// - Loading a bank referencing an unregistered plug-in or codec will result in a load bank success,
		/// but the plug-ins will not be used. More specifically, playing a sound that uses an unregistered effect plug-in 
		/// will result in audio playback without applying the said effect. If an unregistered source plug-in is used by an event's audio objects, 
		/// posting the event will fail.
		/// - The sound engine internally calls GetIDFromString(in_pszString) to return the correct bank ID.
		/// Therefore, in_pszString should be the real name of the SoundBank (with or without the "bnk" extension - it is trimmed internally),
		/// not the name of the file (if you changed it), nor the full path of the file. The path should be resolved in 
		/// your implementation of the Stream Manager, or in the Low-Level I/O module if you use the default Stream Manager's implementation.
		/// - Requesting to load a bank in a different memory pool than where the bank was previously loaded must be done only
		/// after receiving confirmation by the callback that the bank was completely unloaded or by usung synchrone versions
		/// of the UnloadBank function.
		/// \sa 
		/// - AK::SoundEngine::UnloadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - AK::MemoryMgr::CreatePool()
		/// - AkBankCallbackFunc
		/// - \ref soundengine_banks
		/// - \ref integrating_elements_plugins
		/// - \ref streamingdevicemanager
		/// - \ref streamingmanager_lowlevel
		/// - \ref sdk_bank_training
		extern AKSOUNDENGINE_API AKRESULT LoadBank(
	        const char*         in_pszString,            ///< Name/path of the bank to load (Ansi string)
			AkBankCallbackFunc  in_pfnBankCallback,	    ///< Callback function
			void *              in_pCookie,				///< Callback cookie (reserved to user, passed to the callback function)
            AkMemPoolId         in_memPoolId,			///< Memory pool ID (the pool is created if AK_DEFAULT_POOL_ID is passed)
			AkBankID &          out_bankID				///< Returned bank ID
	        );

        /// Load a bank asynchronously (by ID).\n
		/// The bank ID is passed to the Stream Manager.
		/// Refer to \ref soundengine_banks_general for a discussion on using strings and IDs.
		/// You can specify a custom pool for storage of media, the engine will create a new pool if AK_DEFAULT_POOL_ID is passed.
		/// A bank load request will be posted to the Bank Manager consumer thread.
		/// The function returns immediately.
		/// \return 
		/// AK_Success if the scheduling was successful, AK_Fail otherwise.
		/// Use a callback to be notified when completed, and get the status of the request.
		/// The bank ID, which is obtained by hashing the bank name (see GetIDFromString()). 
		/// You may use this ID with UnloadBank().
		/// \remarks
		/// - The initialization bank must be loaded first.
		/// - All SoundBanks subsequently loaded must come from the same Wwise project as the
		///   initialization bank. If you need to load SoundBanks from a different project, you
		///   must first unload ALL banks, including the initialization bank, then load the
		///   initialization bank from the other project, and finally load banks from that project.
		/// - Codecs and plug-ins must be registered before loading banks that use them.
		/// - Loading a bank referencing an unregistered plug-in or codec will result in a load bank success,
		/// but the plug-ins will not be used. More specifically, playing a sound that uses an unregistered effect plug-in 
		/// will result in audio playback without applying the said effect. If an unregistered source plug-in is used by an event's audio objects, 
		/// posting the event will fail.
		/// - Requesting to load a bank in a different memory pool than where the bank was previously loaded must be done only
		/// after receiving confirmation by the callback that the bank was completely unloaded or by usung synchrone versions
		/// of the UnloadBank function.
		/// \sa 
		/// - AK::SoundEngine::UnloadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - AK::MemoryMgr::CreatePool()
		/// - AkBankCallbackFunc
		/// - \ref soundengine_banks
		/// - \ref integrating_elements_plugins
		/// - \ref sdk_bank_training
		extern AKSOUNDENGINE_API AKRESULT LoadBank(
	        AkBankID			in_bankID,              ///< Bank ID of the bank to load
			AkBankCallbackFunc  in_pfnBankCallback,	    ///< Callback function
			void *              in_pCookie,				///< Callback cookie (reserved to user, passed to the callback function)
            AkMemPoolId         in_memPoolId			///< Memory pool ID (the pool is created if AK_DEFAULT_POOL_ID is passed)
	        );

		/// Load a bank asynchronously (by in-memory data).\n
		/// Use this overload when you want to manage I/O on your side. Load the bank file
		/// in a buffer and pass its address to the sound engine.
		/// In-memory loading is in-place: the memory must be valid until the bank is unloaded.
		/// A bank load request will be posted to the Bank Manager consumer thread.
		/// The function returns immediately.
		/// \return 
		/// AK_Success if the scheduling was successful, AK_Fail otherwise, or AK_InvalidParameter if memory alignment is not correct.
		/// Use a callback to be notified when completed, and get the status of the request.
		/// The bank ID, which is obtained by hashing the bank name (see GetIDFromString()). 
		/// You may use this ID with UnloadBank().
		/// \remarks
		/// - The initialization bank must be loaded first.
		/// - All SoundBanks subsequently loaded must come from the same Wwise project as the
		///   initialization bank. If you need to load SoundBanks from a different project, you
		///   must first unload ALL banks, including the initialization bank, then load the
		///   initialization bank from the other project, and finally load banks from that project.
		/// - Codecs and plug-ins must be registered before loading banks that use them.
		/// - Loading a bank referencing an unregistered plug-in or codec will result in a load bank success,
		/// but the plug-ins will not be used. More specifically, playing a sound that uses an unregistered effect plug-in 
		/// will result in audio playback without applying the said effect. If an unregistered source plug-in is used by an event's audio objects, 
		/// posting the event will fail.
		/// - The memory must be aligned on platform-specific AK_BANK_PLATFORM_DATA_ALIGNMENT bytes (see AkTypes.h).
		/// - Requesting to load a bank in a different memory pool than where the bank was previously loaded must be done only
		/// after receiving confirmation by the callback that the bank was completely unloaded or by usung synchrone versions
		/// of the UnloadBank function.
		/// \sa 
		/// - AK::SoundEngine::UnloadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - AkBankCallbackFunc
		/// - \ref soundengine_banks
		/// - \ref integrating_elements_plugins
		/// - \ref sdk_bank_training
		extern AKSOUNDENGINE_API AKRESULT LoadBank(
	        const void *		in_pInMemoryBankPtr,	///< Pointer to the in-memory bank to load
			AkUInt32			in_uInMemoryBankSize,	///< Size of the in-memory bank to load
			AkBankCallbackFunc  in_pfnBankCallback,	    ///< Callback function
			void *              in_pCookie,				///< Callback cookie
			AkBankID &          out_bankID				///< Returned bank ID
	        );

        /// Unload a bank synchronously (by unicode string).\n
		/// Refer to \ref soundengine_banks_general for a discussion on using strings and IDs.
		/// \return AK_Success if successful, AK_Fail otherwise. AK_Success is returned when the bank was not loaded.
		/// \remarks
		/// - If you provided a pool memory ID when loading this bank, it is returned as well. 
		/// Otherwise, the function returns AK_DEFAULT_POOL_ID.
		/// - The sound engine internally calls GetIDFromString(in_pszString) to retrieve the bank ID, 
		/// then it calls the synchronous version of UnloadBank() by ID.
		/// Therefore, in_pszString should be the real name of the SoundBank (with or without the "bnk" extension - it is trimmed internally),
		/// not the name of the file (if you changed it), nor the full path of the file. 
		/// - In order to force the memory deallocation of the bank, sounds that use media from this bank will be stopped. 
		/// This means that streamed sounds or generated sounds will not be stopped.
		/// \sa 
		/// - AK::SoundEngine::LoadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - \ref soundengine_banks
        extern AKSOUNDENGINE_API AKRESULT UnloadBank(
	        const wchar_t*      in_pszString,           ///< Name of the bank to unload (Unicode string)
	        AkMemPoolId *       out_pMemPoolId = NULL   ///< Returned memory pool ID used with LoadBank() (can pass NULL)
	        );

        /// Unload a bank synchronously (by ansi string).\n
		/// Refer to \ref soundengine_banks_general for a discussion on using strings and IDs.
		/// \return AK_Success if successful, AK_Fail otherwise. AK_Success is returned when the bank was not loaded.
		/// \remarks
		/// - If you provided a pool memory ID when loading this bank, it is returned as well. 
		/// Otherwise, the function returns AK_DEFAULT_POOL_ID.
		/// - The sound engine internally calls GetIDFromString(in_pszString) to retrieve the bank ID, 
		/// then it calls the synchronous version of UnloadBank() by ID.
		/// Therefore, in_pszString should be the real name of the SoundBank (with or without the "bnk" extension - it is trimmed internally),
		/// not the name of the file (if you changed it), nor the full path of the file. 
		/// - In order to force the memory deallocation of the bank, sounds that use media from this bank will be stopped. 
		/// This means that streamed sounds or generated sounds will not be stopped.
		/// \sa 
		/// - AK::SoundEngine::LoadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - \ref soundengine_banks
        extern AKSOUNDENGINE_API AKRESULT UnloadBank(
	        const char*         in_pszString,           ///< Name of the bank to unload (Ansi string)
	        AkMemPoolId *       out_pMemPoolId = NULL   ///< Returned memory pool ID used with LoadBank() (can pass NULL)
	        );

        /// Unload a bank synchronously (by ID).\n
		/// \return AK_Success if successful, AK_Fail otherwise. AK_Success is returned when the bank was not loaded.
		/// \remarks
		/// If you provided a pool memory ID when loading this bank, it is returned as well. 
		/// Otherwise, the function returns AK_DEFAULT_POOL_ID.
		/// - In order to force the memory deallocation of the bank, sounds that use media from this bank will be stopped. 
		/// This means that streamed sounds or generated sounds will not be stopped.
		/// \sa 
		/// - AK::SoundEngine::LoadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - \ref soundengine_banks
        extern AKSOUNDENGINE_API AKRESULT UnloadBank(
	        AkBankID            in_bankID,              ///< ID of the bank to unload
            AkMemPoolId *       out_pMemPoolId = NULL   ///< Returned memory pool ID used with LoadBank() (can pass NULL)
	        );

		/// Unload a bank asynchronously (by unicode string).\n
		/// Refer to \ref soundengine_banks_general for a discussion on using strings and IDs.
		/// \return AK_Success if scheduling successful (use a callback to be notified when completed)
		/// \remarks
		/// The sound engine internally calls GetIDFromString(in_pszString) to retrieve the bank ID, 
		/// then it calls the synchronous version of UnloadBank() by ID.
		/// Therefore, in_pszString should be the real name of the SoundBank (with or without the "bnk" extension - it is trimmed internally),
		/// not the name of the file (if you changed it), nor the full path of the file. 
		/// - In order to force the memory deallocation of the bank, sounds that use media from this bank will be stopped. 
		/// This means that streamed sounds or generated sounds will not be stopped.
		/// \sa 
		/// - AK::SoundEngine::LoadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - AkBankCallbackFunc
		/// - \ref soundengine_banks
		extern AKSOUNDENGINE_API AKRESULT UnloadBank(
	        const wchar_t*      in_pszString,           ///< Name of the bank to unload (Unicode string)
			AkBankCallbackFunc  in_pfnBankCallback,	    ///< Callback function
			void *              in_pCookie 				///< Callback cookie (reserved to user, passed to the callback function)
	        );

		/// Unload a bank asynchronously (by ansi string).\n
		/// Refer to \ref soundengine_banks_general for a discussion on using strings and IDs.
		/// \return AK_Success if scheduling successful (use a callback to be notified when completed)
		/// \remarks
		/// The sound engine internally calls GetIDFromString(in_pszString) to retrieve the bank ID, 
		/// then it calls the synchronous version of UnloadBank() by ID.
		/// Therefore, in_pszString should be the real name of the SoundBank (with or without the "bnk" extension - it is trimmed internally),
		/// not the name of the file (if you changed it), nor the full path of the file. 
		/// - In order to force the memory deallocation of the bank, sounds that use media from this bank will be stopped. 
		/// This means that streamed sounds or generated sounds will not be stopped.
		/// \sa 
		/// - AK::SoundEngine::LoadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - AkBankCallbackFunc
		/// - \ref soundengine_banks
		extern AKSOUNDENGINE_API AKRESULT UnloadBank(
	        const char*         in_pszString,           ///< Name of the bank to unload (Ansi string)
			AkBankCallbackFunc  in_pfnBankCallback,	    ///< Callback function
			void *              in_pCookie 				///< Callback cookie (reserved to user, passed to the callback function)
	        );

        /// Unload a bank asynchronously (by ID).\n
		/// Refer to \ref soundengine_banks_general for a discussion on using strings and IDs.
		/// \return AK_Success if scheduling successful (use a callback to be notified when completed)
		/// \remarks
		/// - In order to force the memory deallocation of the bank, sounds that use media from this bank will be stopped. 
		/// This means that streamed sounds or generated sounds will not be stopped.
		/// \sa 
		/// - AK::SoundEngine::LoadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - AkBankCallbackFunc
		/// - \ref soundengine_banks
		extern AKSOUNDENGINE_API AKRESULT UnloadBank(
	        AkBankID            in_bankID,				///< ID of the bank to unload
			AkBankCallbackFunc  in_pfnBankCallback,		///< Callback function
			void *              in_pCookie				///< Callback cookie (reserved to user, passed to the callback function)
	        );

		/// Cancel all event callbacks associated with a specific callback cookie specified while loading Banks of preparing events.\n
		/// \sa 
		/// - AK::SoundEngine::LoadBank()
		/// - AK::SoundEngine::PrepareEvent()
		/// - AK::SoundEngine::UnloadBank()
		/// - AK::SoundEngine::ClearBanks()
		/// - AkBankCallbackFunc
		extern AKSOUNDENGINE_API void CancelBankCallbackCookie( 
			void * in_pCookie 							///< Callback cookie to be cancelled
			);

		/// Preparation type.
		/// \sa
		/// - AK::SoundEngine::PrepareEvent()
		/// - AK::SoundEngine::PrepareGameSyncs()
		/// - AK::SoundEngine::PrepareBank()
		enum PreparationType
		{
			Preparation_Load,	///< PrepareEvent will load required information to play the specified event.
			Preparation_Unload	///< PrepareEvent will unload required information to play the specified event.
		};

		/// Parameter to be passed to AK::SoundEngine::PrepareBank().
		/// Use AkBankContent_All to load both the media and structural content.
		/// Use AkBankContent_StructureOnly to load only the structural content and then use the PrepareEvent() functions to load media on demand.
		/// \sa 
		/// - AK::SoundEngine::PrepareBank()
		/// - \ref soundengine_banks_preparingbanks
		enum AkBankContent
		{
			AkBankContent_StructureOnly,	///< Use AkBankContent_All to load both the media and structural content.
			AkBankContent_All				///< Use AkBankContent_StructureOnly to load only the structural content and then use the PrepareEvent() functions to load media on demand.
		};

		/// This function will load the bank, but instead of loading the media part in one memory block, it will load every media track individually.
		/// It will prevent in-memory duplication of media at the cost of some memory fragmentation.
		/// Calling this function specifying the flag AkBankContent_StructureOnly will load only the structural part (including events) of this bank, 
		/// allowing using PrepareEvent() to load media on demand.
		/// \sa 
		/// - \ref soundengine_banks_preparingbanks
		/// - AK::SoundEngine::LoadBank()
		/// - AK::SoundEngine::PreparationType 
		extern AKSOUNDENGINE_API AKRESULT PrepareBank(
			AK::SoundEngine::PreparationType	in_PreparationType,			///< Preparation type ( Preparation_Load or Preparation_Unload )
			const wchar_t*        in_pszString,								///< Name of the bank to Prepare/Unprepare. (Unicode string)
			AK::SoundEngine::AkBankContent	in_uFlags = AkBankContent_All	///< Media, structure or both (Events are included in structural)
			);

		/// This function will load the bank, but instead of loading the media part in one memory block, it will load every media track individually.
		/// It will prevent in-memory duplication of media at the cost of some memory fragmentation.
		/// Calling this function specifying the flag AkBankContent_StructureOnly will load only the structural part (including events) of this bank, 
		/// allowing using PrepareEvent() to load media on demand.
		/// \sa 
		/// - \ref soundengine_banks_preparingbanks
		/// - AK::SoundEngine::LoadBank()
		/// - AK::SoundEngine::PreparationType 
		extern AKSOUNDENGINE_API AKRESULT PrepareBank(
			AK::SoundEngine::PreparationType	in_PreparationType,			///< Preparation type ( Preparation_Load or Preparation_Unload )
			const char*           in_pszString,								///< Name of the bank to Prepare/Unprepare. (Ansi string)
			AK::SoundEngine::AkBankContent	in_uFlags = AkBankContent_All	///< Media, structure or both (Events are included in structural)
			);

		/// This function will load the bank, but instead of loading the media part in one block, it will load every media track individually.
		/// It will prevent in-memory duplication of media at the cost of some memory fragmentation.
		/// Calling this function specifying the flag AkBankContent_StructureOnly will load only the structural part (including events) of this bank, 
		/// allowing using PrepareEvent() to load media on demand.
		/// \sa 
		/// - \ref soundengine_banks_preparingbanks
		/// - AK::SoundEngine::LoadBank()
		/// - AK::SoundEngine::PreparationType
		extern AKSOUNDENGINE_API AKRESULT PrepareBank(
			AK::SoundEngine::PreparationType	in_PreparationType,			///< Preparation type ( Preparation_Load or Preparation_Unload )
			AkBankID            in_bankID,									///< ID of the bank to Prepare/Unprepare.
			AK::SoundEngine::AkBankContent	in_uFlags = AkBankContent_All	///< Media, structure or both (Events are included in structural)
			);

		/// This function will load the bank, but instead of loading the media part in one block, it will load every media track individually.
		/// It will prevent in-memory duplication of media at the cost of some memory fragmentation.
		/// Calling this function specifying the flag AkBankContent_StructureOnly will load only the structural part (including events) of this bank, 
		/// allowing using PrepareEvent() to load media on demand.
		/// \sa 
		/// - \ref soundengine_banks_preparingbanks
		/// - AK::SoundEngine::LoadBank()
		/// - AkBankCallbackFunc
		/// - AK::SoundEngine::PreparationType
		extern AKSOUNDENGINE_API AKRESULT PrepareBank(
			AK::SoundEngine::PreparationType	in_PreparationType,			///< Preparation type ( Preparation_Load or Preparation_Unload )
			const wchar_t*      in_pszString,								///< Name of the bank to Prepare/Unprepare. (Unicode string)
			AkBankCallbackFunc	in_pfnBankCallback,							///< Callback function
			void *              in_pCookie,									///< Callback cookie (reserved to user, passed to the callback function)
			AK::SoundEngine::AkBankContent	in_uFlags = AkBankContent_All	///< Media, structure or both (Events are included in structural)
			);

		/// This function will load the bank, but instead of loading the media part in one block, it will load every media track individually.
		/// It will prevent in-memory duplication of media at the cost of some memory fragmentation.
		/// Calling this function specifying the flag AkBankContent_StructureOnly will load only the structural part (including events) of this bank, 
		/// allowing using PrepareEvent() to load media on demand.
		/// \sa 
		/// - \ref soundengine_banks_preparingbanks
		/// - AK::SoundEngine::LoadBank()
		/// - AkBankCallbackFunc
		/// - AK::SoundEngine::PreparationType
		extern AKSOUNDENGINE_API AKRESULT PrepareBank(
			AK::SoundEngine::PreparationType	in_PreparationType,			///< Preparation type ( Preparation_Load or Preparation_Unload )
			const char*         in_pszString,								///< Name of the bank to Prepare/Unprepare. (Ansi string)
			AkBankCallbackFunc	in_pfnBankCallback,							///< Callback function
			void *              in_pCookie,									///< Callback cookie (reserved to user, passed to the callback function)
			AK::SoundEngine::AkBankContent	in_uFlags = AkBankContent_All	///< Media, structure or both (Events are included in structural)
			);

		/// This function will load the bank, but instead of loading the media part in one block, it will load every media track individually.
		/// It will prevent in-memory duplication of media at the cost of some memory fragmentation.
		/// Calling this function specifying the flag AkBankContent_StructureOnly will load only the structural part (including events) of this bank, 
		/// allowing using PrepareEvent() to load media on demand.
		/// \sa 
		/// - \ref soundengine_banks_preparingbanks
		/// - AK::SoundEngine::LoadBank()
		/// - AkBankCallbackFunc
		/// - AK::SoundEngine::PreparationType
		extern AKSOUNDENGINE_API AKRESULT PrepareBank(
			AK::SoundEngine::PreparationType		in_PreparationType,				///< Preparation type ( Preparation_Load or Preparation_Unload )
			AkBankID            in_bankID,						///< ID of the bank to Prepare/Unprepare.
			AkBankCallbackFunc	in_pfnBankCallback,				///< Callback function
			void *              in_pCookie,						///< Callback cookie (reserved to user, passed to the callback function)
			AK::SoundEngine::AkBankContent		in_uFlags = AkBankContent_All	///< Media, structure or both (Events are included in structural)
			);
		
		/// Clear all previously prepared events.\n
		/// \return
		/// - AK_Success if successful.
		///	- AK_Fail if the sound engine was not correctly initialized or if there is not enough memory to handle the command.
		/// \remarks
		/// The function ClearBanks() also clears all prepared events.
		/// \sa
		/// - AK::SoundEngine::PrepareEvent()
		/// - AK::SoundEngine::ClearBanks()
		extern AKSOUNDENGINE_API AKRESULT ClearPreparedEvents();

		/// Prepare or un-prepare events synchronously (by unicode string).\n
		/// The events are identified by strings, and converted to IDs internally
		/// (refer to \ref soundengine_banks_general for a discussion on using strings and IDs).
		/// The event definitions must already exist in the sound engine by having
		/// explicitly loaded the bank(s) that contain them (with LoadBank()).
		/// A request is posted to the Bank Manager consumer thread. It will resolve all 
		/// dependencies needed to successfully post the events specified, and load the 
		/// required banks, if applicable. 
		/// The function returns when the request is completely processed.
		/// \return 
		///	- AK_Success: Prepare/un-prepare successful.
		/// - AK_IDNotFound: At least one of the event/game sync identifiers passed to PrepareEvent() does not exist.
		/// - AK_InsufficientMemory: Insufficient memory to store bank data.
		/// - AK_BankReadError: I/O error.
		/// - AK_WrongBankVersion: Invalid bank version: make sure the version of Wwise that 
		/// you used to generate the SoundBanks matches that of the SDK you are currently using.
		/// - AK_InvalidFile: File specified could not be opened.
		/// - AK_InvalidParameter: Invalid parameter, invalid memory alignment.		
		/// - AK_Fail: Load or unload failed for any other reason.
		/// \remarks
		/// Whenever at least one event fails to be resolved, the actions performed for all 
		/// other events are cancelled.
		/// \remarks
		/// To learn more about how PrepareEvent() works (including multiple calls to
		/// prepare/unprepare the same events and how this works with LoadBank()), please
		/// refer to the following Knowledge Base articles:
		/// - http://www.audiokinetic.com/goto.php?target=kb&article=74
		/// - http://www.audiokinetic.com/goto.php?target=kb&article=75
		/// - http://www.audiokinetic.com/goto.php?target=kb&article=148
		/// \sa
		/// - AK::SoundEngine::PrepareEvent()
		/// - AK::SoundEngine::ClearPreparedEvents()
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::LoadBank()
		/// - \ref soundengine_banks
		/// - \ref sdk_bank_training
		extern AKSOUNDENGINE_API AKRESULT PrepareEvent( 
			PreparationType		in_PreparationType,		///< Preparation type ( Preparation_Load or Preparation_Unload )
			const wchar_t**		in_ppszString,			///< Array of event names (Unicode string)
			AkUInt32			in_uNumEvent			///< Number of events in the array
			);

		/// Prepare or un-prepare events synchronously (by ansi string).\n
		/// The events are identified by strings, and converted to IDs internally
		/// (refer to \ref soundengine_banks_general for a discussion on using strings and IDs).
		/// The event definitions must already exist in the sound engine by having
		/// explicitly loaded the bank(s) that contain them (with LoadBank()).
		/// A request is posted to the Bank Manager consumer thread. It will resolve all 
		/// dependencies needed to successfully post the events specified, and load the 
		/// required banks, if applicable. 
		/// The function returns when the request is completely processed.
		/// \return 
		///	- AK_Success: Prepare/un-prepare successful.
		/// - AK_IDNotFound: At least one of the event/game sync identifiers passed to PrepareEvent() does not exist.
		/// - AK_InsufficientMemory: Insufficient memory to store bank data.
		/// - AK_BankReadError: I/O error.
		/// - AK_WrongBankVersion: Invalid bank version: make sure the version of Wwise that 
		/// you used to generate the SoundBanks matches that of the SDK you are currently using.
		/// - AK_InvalidFile: File specified could not be opened.
		/// - AK_InvalidParameter: Invalid parameter, invalid memory alignment.		
		/// - AK_Fail: Load or unload failed for any other reason.
		/// \remarks
		/// Whenever at least one event fails to be resolved, the actions performed for all 
		/// other events are cancelled.
		/// \remarks
		/// To learn more about how PrepareEvent() works (including multiple calls to
		/// prepare/unprepare the same events and how this works with LoadBank()), please
		/// refer to the following Knowledge Base articles:
		/// - http://www.audiokinetic.com/goto.php?target=kb&article=74
		/// - http://www.audiokinetic.com/goto.php?target=kb&article=75
		/// - http://www.audiokinetic.com/goto.php?target=kb&article=148
		/// \sa
		/// - AK::SoundEngine::PrepareEvent()
		/// - AK::SoundEngine::ClearPreparedEvents()
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::LoadBank()
		/// - \ref soundengine_banks
		/// - \ref sdk_bank_training
		extern AKSOUNDENGINE_API AKRESULT PrepareEvent( 
			PreparationType		in_PreparationType,		///< Preparation type ( Preparation_Load or Preparation_Unload )
			const char**		in_ppszString,			///< Array of event names (Ansi string)
			AkUInt32			in_uNumEvent			///< Number of events in the array
			);

		/// Prepare or un-prepare events synchronously (by ID).
		/// The events are identified by their ID (refer to \ref soundengine_banks_general for a discussion on using strings and IDs).
		/// The event definitions must already exist in the sound engine by having
		/// explicitly loaded the bank(s) that contain them (with LoadBank()).
		/// A request is posted to the Bank Manager consumer thread. It will resolve all 
		/// dependencies needed to successfully post the events specified, and load the 
		/// required banks, if applicable. 
		/// The function returns when the request is completely processed.
		/// \return 
		///	- AK_Success: Prepare/un-prepare successful.
		/// - AK_IDNotFound: At least one of the event/game sync identifiers passed to PrepareEvent() does not exist.
		/// - AK_InsufficientMemory: Insufficient memory to store bank data.
		/// - AK_BankReadError: I/O error.
		/// - AK_WrongBankVersion: Invalid bank version: make sure the version of Wwise that 
		/// you used to generate the SoundBanks matches that of the SDK you are currently using.
		/// - AK_InvalidFile: File specified could not be opened.
		/// - AK_InvalidParameter: Invalid parameter, invalid memory alignment.		
		/// - AK_Fail: Load or unload failed for any other reason.
		/// \remarks
		/// Whenever at least one event fails to be resolved, the actions performed for all 
		/// other events are cancelled.
		/// \remarks
		/// To learn more about how PrepareEvent() works (including multiple calls to
		/// prepare/unprepare the same events and how this works with LoadBank()), please
		/// refer to the following Knowledge Base articles:
		/// - http://www.audiokinetic.com/goto.php?target=kb&article=74
		/// - http://www.audiokinetic.com/goto.php?target=kb&article=75
		/// - http://www.audiokinetic.com/goto.php?target=kb&article=148
		/// \sa
		/// - AK::SoundEngine::PrepareEvent()
		/// - AK::SoundEngine::ClearPreparedEvents()
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::LoadBank()
		/// - \ref soundengine_banks
		/// - \ref sdk_bank_training
		extern AKSOUNDENGINE_API AKRESULT PrepareEvent( 
			PreparationType		in_PreparationType,		///< Preparation type ( Preparation_Load or Preparation_Unload )
			AkUniqueID*			in_pEventID,			///< Array of event IDs
			AkUInt32			in_uNumEvent			///< Number of event IDs in the array
			);

		/// Prepare or un-prepare an event asynchronously (by unicode string).
		/// The events are identified by string (refer to \ref soundengine_banks_general for a discussion on using strings and IDs).
		/// The event definitions must already exist in the sound engine by having
		/// explicitly loaded the bank(s) that contain them (with LoadBank()).
		/// A request is posted to the Bank Manager consumer thread. It will resolve all 
		/// dependencies needed to successfully post the events specified, and load the 
		/// required banks, if applicable. 
		/// The function returns immediately. Use a callback to be notified when the request has finished being processed.
		/// \return AK_Success if scheduling is was successful, AK_Fail otherwise.
		/// \remarks
		/// Whenever at least one event fails to be resolved, the actions performed for all 
		/// other events are cancelled.
		/// \remarks
		/// To learn more about how PrepareEvent() works (including multiple calls to
		/// prepare/unprepare the same events and how this works with LoadBank()), please
		/// refer to the following Knowledge Base articles:
		/// - http://www.audiokinetic.com/goto.php?target=kb&article=74
		/// - http://www.audiokinetic.com/goto.php?target=kb&article=75
		/// - http://www.audiokinetic.com/goto.php?target=kb&article=148
		/// \sa
		/// - AK::SoundEngine::PrepareEvent()
		/// - AK::SoundEngine::ClearPreparedEvents()
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::LoadBank()
		/// - AkBankCallbackFunc
		/// - \ref soundengine_banks
		/// - \ref sdk_bank_training
		extern AKSOUNDENGINE_API AKRESULT PrepareEvent( 
			PreparationType		in_PreparationType,		///< Preparation type ( Preparation_Load or Preparation_Unload )
			const wchar_t**		in_ppszString,			///< Array of event names (Unicode string)
			AkUInt32			in_uNumEvent,			///< Number of events in the array
			AkBankCallbackFunc	in_pfnBankCallback,		///< Callback function
			void *              in_pCookie				///< Callback cookie (reserved to user, passed to the callback function)
			);

		/// Prepare or un-prepare an event asynchronously (by ansi string).
		/// The events are identified by string (refer to \ref soundengine_banks_general for a discussion on using strings and IDs).
		/// The event definitions must already exist in the sound engine by having
		/// explicitly loaded the bank(s) that contain them (with LoadBank()).
		/// A request is posted to the Bank Manager consumer thread. It will resolve all 
		/// dependencies needed to successfully post the events specified, and load the 
		/// required banks, if applicable. 
		/// The function returns immediately. Use a callback to be notified when the request has finished being processed.
		/// \return AK_Success if scheduling is was successful, AK_Fail otherwise.
		/// \remarks
		/// Whenever at least one event fails to be resolved, the actions performed for all 
		/// other events are cancelled.
		/// \remarks
		/// To learn more about how PrepareEvent() works (including multiple calls to
		/// prepare/unprepare the same events and how this works with LoadBank()), please
		/// refer to the following Knowledge Base articles:
		/// - http://www.audiokinetic.com/goto.php?target=kb&article=74
		/// - http://www.audiokinetic.com/goto.php?target=kb&article=75
		/// - http://www.audiokinetic.com/goto.php?target=kb&article=148
		/// \sa
		/// - AK::SoundEngine::PrepareEvent()
		/// - AK::SoundEngine::ClearPreparedEvents()
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::LoadBank()
		/// - AkBankCallbackFunc
		/// - \ref soundengine_banks
		/// - \ref sdk_bank_training
		extern AKSOUNDENGINE_API AKRESULT PrepareEvent( 
			PreparationType		in_PreparationType,		///< Preparation type ( Preparation_Load or Preparation_Unload )
			const char**		in_ppszString,			///< Array of event names (Ansi string)
			AkUInt32			in_uNumEvent,			///< Number of events in the array
			AkBankCallbackFunc	in_pfnBankCallback,		///< Callback function
			void *              in_pCookie				///< Callback cookie (reserved to user, passed to the callback function)
			);

		/// Prepare or un-prepare events asynchronously (by ID).\n
		/// The events are identified by their ID (refer to \ref soundengine_banks_general for a discussion on using strings and IDs).
		/// The event definitions must already exist in the sound engine by having
		/// explicitly loaded the bank(s) that contain them (with LoadBank()).
		/// A request is posted to the Bank Manager consumer thread. It will resolve all 
		/// dependencies needed to successfully post the events specified, and load the 
		/// required banks, if applicable. 
		/// The function returns immediately. Use a callback to be notified when the request has finished being processed.
		/// \return AK_Success if scheduling is was successful, AK_Fail otherwise.
		/// \remarks
		/// Whenever at least one event fails to be resolved, the actions performed for all 
		/// other events are cancelled.
		/// \remarks
		/// To learn more about how PrepareEvent() works (including multiple calls to
		/// prepare/unprepare the same events and how this works with LoadBank()), please
		/// refer to the following Knowledge Base articles:
		/// - http://www.audiokinetic.com/goto.php?target=kb&article=74
		/// - http://www.audiokinetic.com/goto.php?target=kb&article=75
		/// - http://www.audiokinetic.com/goto.php?target=kb&article=148
		/// \sa
		/// - AK::SoundEngine::PrepareEvent()
		/// - AK::SoundEngine::ClearPreparedEvents()
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::LoadBank()
		/// - AkBankCallbackFunc
		/// - \ref soundengine_banks
		/// - \ref sdk_bank_training
		extern AKSOUNDENGINE_API AKRESULT PrepareEvent( 
			PreparationType		in_PreparationType,		///< Preparation type ( Preparation_Load or Preparation_Unload )
			AkUniqueID*			in_pEventID,			///< Array of event IDs
			AkUInt32			in_uNumEvent,			///< Number of event IDs in the array
			AkBankCallbackFunc	in_pfnBankCallback,		///< Callback function
			void *              in_pCookie				///< Callback cookie (reserved to user, passed to the callback function)
			);

		/// Prepare or un-prepare game syncs synchronously (by unicode string).\n
		/// The group and game syncs are specified by string (refer to \ref soundengine_banks_general for a discussion on using strings and IDs).
		/// The game syncs definitions must already exist in the sound engine by having
		/// explicitly loaded the bank(s) that contain them (with LoadBank()).
		/// A request is posted to the Bank Manager consumer thread. It will resolve all 
		/// dependencies needed to successfully set this game sync group to one of the
		/// game sync values specified, and load the required banks, if applicable. 
		/// The function returns when the request has been completely processed. 
		/// \return 
		///	- AK_Success: Prepare/un-prepare successful.
		/// - AK_IDNotFound: At least one of the event/game sync identifiers passed to PrepareGameSyncs() does not exist.
		/// - AK_InsufficientMemory: Insufficient memory to store bank data.
		/// - AK_BankReadError: I/O error.
		/// - AK_WrongBankVersion: Invalid bank version: make sure the version of Wwise that 
		/// you used to generate the SoundBanks matches that of the SDK you are currently using.
		/// - AK_InvalidFile: File specified could not be opened.
		/// - AK_InvalidParameter: Invalid parameter, invalid memory alignment.		
		/// - AK_Fail: Load or unload failed for any other reason.
		/// \remarks
		/// You need to call PrepareGameSyncs() if the sound engine was initialized with AkInitSettings::bEnableGameSyncPreparation 
		/// set to true. When set to false, the sound engine automatically prepares all game syncs when preparing events,
		/// so you never need to call this function.
		/// \sa 
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::PrepareEvent()
		/// - AK::SoundEngine::LoadBank()
		/// - AkInitSettings
		/// - \ref soundengine_banks
		/// - \ref sdk_bank_training
		extern AKSOUNDENGINE_API AKRESULT PrepareGameSyncs(
			PreparationType	in_PreparationType,			///< Preparation type ( Preparation_Load or Preparation_Unload )
			AkGroupType		in_eGameSyncType,			///< The type of game sync.
			const wchar_t*	in_pszGroupName,			///< The state group Name or the Switch Group Name. (Unicode string)
			const wchar_t**	in_ppszGameSyncName,		///< The specific ID of the state to either support or not support. (Unicode string)
			AkUInt32		in_uNumGameSyncs			///< The number of game sync in the string array.
			);

		/// Prepare or un-prepare game syncs synchronously (by ansi string).\n
		/// The group and game syncs are specified by string (refer to \ref soundengine_banks_general for a discussion on using strings and IDs).
		/// The game syncs definitions must already exist in the sound engine by having
		/// explicitly loaded the bank(s) that contain them (with LoadBank()).
		/// A request is posted to the Bank Manager consumer thread. It will resolve all 
		/// dependencies needed to successfully set this game sync group to one of the
		/// game sync values specified, and load the required banks, if applicable. 
		/// The function returns when the request has been completely processed. 
		/// \return 
		///	- AK_Success: Prepare/un-prepare successful.
		/// - AK_IDNotFound: At least one of the event/game sync identifiers passed to PrepareGameSyncs() does not exist.
		/// - AK_InsufficientMemory: Insufficient memory to store bank data.
		/// - AK_BankReadError: I/O error.
		/// - AK_WrongBankVersion: Invalid bank version: make sure the version of Wwise that 
		/// you used to generate the SoundBanks matches that of the SDK you are currently using.
		/// - AK_InvalidFile: File specified could not be opened.
		/// - AK_InvalidParameter: Invalid parameter, invalid memory alignment.		
		/// - AK_Fail: Load or unload failed for any other reason.
		/// \remarks
		/// You need to call PrepareGameSyncs() if the sound engine was initialized with AkInitSettings::bEnableGameSyncPreparation 
		/// set to true. When set to false, the sound engine automatically prepares all game syncs when preparing events,
		/// so you never need to call this function.
		/// \sa 
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::PrepareEvent()
		/// - AK::SoundEngine::LoadBank()
		/// - AkInitSettings
		/// - \ref soundengine_banks
		/// - \ref sdk_bank_training
		extern AKSOUNDENGINE_API AKRESULT PrepareGameSyncs(
			PreparationType	in_PreparationType,			///< Preparation type ( Preparation_Load or Preparation_Unload )
			AkGroupType		in_eGameSyncType,			///< The type of game sync.
			const char*		in_pszGroupName,			///< The state group Name or the Switch Group Name. (Ansi string)
			const char**	in_ppszGameSyncName,		///< The specific ID of the state to either support or not support. (Ansi string)
			AkUInt32		in_uNumGameSyncs			///< The number of game sync in the string array.
			);

		/// Prepare or un-prepare game syncs synchronously (by ID).\n
		/// The group and game syncs are specified by ID (refer to \ref soundengine_banks_general for a discussion on using strings and IDs).
		/// The game syncs definitions must already exist in the sound engine by having
		/// explicitly loaded the bank(s) that contain them (with LoadBank()).
		/// A request is posted to the Bank Manager consumer thread. It will resolve all 
		/// dependencies needed to successfully set this game sync group to one of the
		/// game sync values specified, and load the required banks, if applicable. 
		/// The function returns when the request has been completely processed. 
		/// \return 
		///	- AK_Success: Prepare/un-prepare successful.
		/// - AK_IDNotFound: At least one of the event/game sync identifiers passed to PrepareGameSyncs() does not exist.
		/// - AK_InsufficientMemory: Insufficient memory to store bank data.
		/// - AK_BankReadError: I/O error.
		/// - AK_WrongBankVersion: Invalid bank version: make sure the version of Wwise that 
		/// you used to generate the SoundBanks matches that of the SDK you are currently using.
		/// - AK_InvalidFile: File specified could not be opened.
		/// - AK_InvalidParameter: Invalid parameter, invalid memory alignment.		
		/// - AK_Fail: Load or unload failed for any other reason.
		/// \remarks
		/// You need to call PrepareGameSyncs() if the sound engine was initialized with AkInitSettings::bEnableGameSyncPreparation 
		/// set to true. When set to false, the sound engine automatically prepares all game syncs when preparing events,
		/// so you never need to call this function.
		/// \sa 
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::PrepareEvent()
		/// - AK::SoundEngine::LoadBank()
		/// - AkInitSettings
		/// - \ref soundengine_banks
		/// - \ref sdk_bank_training
		extern AKSOUNDENGINE_API AKRESULT PrepareGameSyncs(
			PreparationType	in_PreparationType,			///< Preparation type ( Preparation_Load or Preparation_Unload )
			AkGroupType		in_eGameSyncType,			///< The type of game sync.
			AkUInt32		in_GroupID,					///< The state group ID or the Switch Group ID.
			AkUInt32*		in_paGameSyncID,			///< Array of ID of the gamesyncs to either support or not support.
			AkUInt32		in_uNumGameSyncs			///< The number of game sync ID in the array.
			);

		/// Prepare or un-prepare game syncs asynchronously (by unicode string).\n
		/// The group and game syncs are specified by string (refer to \ref soundengine_banks_general for a discussion on using strings and IDs).
		/// The game syncs definitions must already exist in the sound engine by having
		/// explicitly loaded the bank(s) that contain them (with LoadBank()).
		/// A request is posted to the Bank Manager consumer thread. It will resolve all 
		/// dependencies needed to successfully set this game sync group to one of the
		/// game sync values specified, and load the required banks, if applicable. 
		/// The function returns immediately. Use a callback to be notified when the request has finished being processed.
		/// \return AK_Success if scheduling is was successful, AK_Fail otherwise.
		/// \remarks
		/// You need to call PrepareGameSyncs() if the sound engine was initialized with AkInitSettings::bEnableGameSyncPreparation 
		/// set to true. When set to false, the sound engine automatically prepares all game syncs when preparing events,
		/// so you never need to call this function.
		/// \sa 
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::PrepareEvent()
		/// - AK::SoundEngine::LoadBank()
		/// - AkInitSettings
		/// - AkBankCallbackFunc
		/// - \ref soundengine_banks
		/// - \ref sdk_bank_training
		extern AKSOUNDENGINE_API AKRESULT PrepareGameSyncs(
			PreparationType		in_PreparationType,		///< Preparation type ( Preparation_Load or Preparation_Unload )
			AkGroupType			in_eGameSyncType,		///< The type of game sync.
			const wchar_t*		in_pszGroupName,		///< The state group Name or the Switch Group Name. (Unicode string)
			const wchar_t**		in_ppszGameSyncName,	///< The specific ID of the state to either support or not support. (Unicode string)
			AkUInt32			in_uNumGameSyncs,		///< The number of game sync in the string array.
			AkBankCallbackFunc	in_pfnBankCallback,		///< Callback function
			void *				in_pCookie				///< Callback cookie (reserved to user, passed to the callback function)
			);

		/// Prepare or un-prepare game syncs asynchronously (by ansi string).\n
		/// The group and game syncs are specified by string (refer to \ref soundengine_banks_general for a discussion on using strings and IDs).
		/// The game syncs definitions must already exist in the sound engine by having
		/// explicitly loaded the bank(s) that contain them (with LoadBank()).
		/// A request is posted to the Bank Manager consumer thread. It will resolve all 
		/// dependencies needed to successfully set this game sync group to one of the
		/// game sync values specified, and load the required banks, if applicable. 
		/// The function returns immediately. Use a callback to be notified when the request has finished being processed.
		/// \return AK_Success if scheduling is was successful, AK_Fail otherwise.
		/// \remarks
		/// You need to call PrepareGameSyncs() if the sound engine was initialized with AkInitSettings::bEnableGameSyncPreparation 
		/// set to true. When set to false, the sound engine automatically prepares all game syncs when preparing events,
		/// so you never need to call this function.
		/// \sa 
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::PrepareEvent()
		/// - AK::SoundEngine::LoadBank()
		/// - AkInitSettings
		/// - AkBankCallbackFunc
		/// - \ref soundengine_banks
		/// - \ref sdk_bank_training
		extern AKSOUNDENGINE_API AKRESULT PrepareGameSyncs(
			PreparationType		in_PreparationType,		///< Preparation type ( Preparation_Load or Preparation_Unload )
			AkGroupType			in_eGameSyncType,		///< The type of game sync.
			const char*			in_pszGroupName,		///< The state group Name or the Switch Group Name. (Ansi string)
			const char**		in_ppszGameSyncName,	///< The specific ID of the state to either support or not support. (Ansi string)
			AkUInt32			in_uNumGameSyncs,		///< The number of game sync in the string array.
			AkBankCallbackFunc	in_pfnBankCallback,		///< Callback function
			void *				in_pCookie				///< Callback cookie (reserved to user, passed to the callback function)
			);

		/// Prepare or un-prepare game syncs asynchronously (by ID).\n
		/// The group and game syncs are specified by ID (refer to \ref soundengine_banks_general for a discussion on using strings and IDs).
		/// The game syncs definitions must already exist in the sound engine by having
		/// explicitly loaded the bank(s) that contain them (with LoadBank()).
		/// A request is posted to the Bank Manager consumer thread. It will resolve all 
		/// dependencies needed to successfully set this game sync group to one of the
		/// game sync values specified, and load the required banks, if applicable. 
		/// The function returns immediately. Use a callback to be notified when the request has finished being processed.
		/// \return AK_Success if scheduling is was successful, AK_Fail otherwise.
		/// \remarks
		/// You need to call PrepareGameSyncs() if the sound engine was initialized with AkInitSettings::bEnableGameSyncPreparation 
		/// set to true. When set to false, the sound engine automatically prepares all game syncs when preparing events,
		/// so you never need to call this function.
		/// \sa 
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::PrepareEvent()
		/// - AK::SoundEngine::LoadBank()
		/// - AkInitSettings
		/// - AkBankCallbackFunc
		/// - \ref soundengine_banks
		/// - \ref sdk_bank_training
		extern AKSOUNDENGINE_API AKRESULT PrepareGameSyncs(
			PreparationType		in_PreparationType,		///< Preparation type ( Preparation_Load or Preparation_Unload )
			AkGroupType			in_eGameSyncType,		///< The type of game sync.
			AkUInt32			in_GroupID,				///< The state group ID or the Switch Group ID.
			AkUInt32*			in_paGameSyncID,		///< Array of ID of the gamesyncs to either support or not support.
			AkUInt32			in_uNumGameSyncs,		///< The number of game sync ID in the array.
			AkBankCallbackFunc	in_pfnBankCallback,		///< Callback function
			void *				in_pCookie				///< Callback cookie (reserved to user, passed to the callback function)
			);

	    //@}


		////////////////////////////////////////////////////////////////////////
		/// @name Listeners
		//@{

		/// Set a game object's active listeners.
		/// By default, all new game objects only have the listener 0 active.  Inactive listeners are not computed.
		/// \return Always returns AK_Success
		/// \sa 
		/// - \ref soundengine_listeners_multi_assignobjects
		extern AKSOUNDENGINE_API AKRESULT SetActiveListeners(
			AkGameObjectID in_GameObjectID,				///< Game object identifier
			AkUInt32 in_uListenerMask					///< Bitmask representing the active listeners (LSB = Listener 0, set to 1 means active)
			);

		/// Set a listener's position.
		/// \return Always returns AK_Success
		/// \sa 
		/// - \ref soundengine_listeners_settingpos
        extern AKSOUNDENGINE_API AKRESULT SetListenerPosition( 
			const AkListenerPosition & in_Position,		///< Position to set
			AkUInt32 in_uIndex = 0 						///< Listener index (0: first listener, 7: 8th listener)
		    );

		/// Set a listener's spatialization parameters. This allows you to define listener-specific 
		/// volume offsets for each audio channel.
		/// \return Always returns AK_Success
		/// \sa 
		/// - \ref soundengine_listeners_spatial
		extern AKSOUNDENGINE_API AKRESULT SetListenerSpatialization(
			AkUInt32 in_uIndex,							///< Listener index (0: first listener, 7: 8th listener)
			bool in_bSpatialized,						///< Spatialization toggle (True : enable spatialization, False : disable spatialization)
			AkSpeakerVolumes * in_pVolumeOffsets = NULL	///< Per-speaker volume offset, in dB (Only used if in_bSpatialized == false)
			);

		/// Set a listener's ability to listen to audio and motion events.
		/// By default, all listeners are enabled for audio and disabled for motion.  
		/// \aknote If your game doesn't use Motion, you should NOT need to use this function. 
		/// This function isn't a global "activate" switch on the listeners.  Use SetActiveListeners properly to
		/// control which listeners are used in your game. \endaknote
		/// \return Always returns AK_Success
		/// \sa 
		/// - \ref soundengine_listeners_multi_assignobjects
		/// - AK::SoundEngine::SetActiveListeners
		extern AKSOUNDENGINE_API AKRESULT SetListenerPipeline(
			AkUInt32 in_uIndex,						///< Listener index (0: first listener, 7: 8th listener)
			bool in_bAudio,							///< True=Listens to audio events (by default it is true)
			bool in_bMotion							///< True=Listens to motion events (by default it is false)
			);
	    //@}


		////////////////////////////////////////////////////////////////////////
		/// @name Game Syncs
		//@{

		/// Set the value of a real-time parameter control (by ID).
		/// With this function, you may set a game parameter value on global scope or on game object scope. 
		/// Game object scope supersedes global scope. Game parameter values set on global scope are applied to all 
		/// game objects that not yet registered, or already registered but not overriden with a value on game object scope.
		/// To set a game parameter value on global scope, pass AK_INVALID_GAME_OBJECT as the game object. 
		/// Note that busses ignore RTPCs when they are applied on game object scope. Thus, you may only change bus 
		/// or bus plugins properties by calling this function with AK_INVALID_GAME_OBJECT.
		/// Refer to \ref soundengine_rtpc_pergameobject, \ref soundengine_rtpc_buses and 
		/// \ref soundengine_rtpc_effects for more details on RTPC scope.
		/// \return Always AK_Success
		/// \sa 
		/// - \ref soundengine_rtpc
		/// - AK::SoundEngine::GetIDFromString()
        extern AKSOUNDENGINE_API AKRESULT SetRTPCValue( 
			AkRtpcID in_rtpcID, 									///< ID of the game parameter
			AkRtpcValue in_value, 									///< Value to set
			AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT ///< Associated game object ID
		    );

		/// Set the value of a real-time parameter control (by unicode string name).
		/// With this function, you may set a game parameter value on global scope or on game object scope. 
		/// Game object scope supersedes global scope. Game parameter values set on global scope are applied to all 
		/// game objects that not yet registered, or already registered but not overriden with a value on game object scope.
		/// To set a game parameter value on global scope, pass AK_INVALID_GAME_OBJECT as the game object. 
		/// Note that busses ignore RTPCs when they are applied on game object scope. Thus, you may only change bus 
		/// or bus plugins properties by calling this function with AK_INVALID_GAME_OBJECT.
		/// Refer to \ref soundengine_rtpc_pergameobject, \ref soundengine_rtpc_buses and 
		/// \ref soundengine_rtpc_effects for more details on RTPC scope.
		/// \return 
		/// - AK_Success if successful
		/// - AK_IDNotFound if in_pszRtpcName is NULL.
		/// \aknote Strings are case-sensitive. \endaknote
		/// \sa 
		/// - \ref soundengine_rtpc
        extern AKSOUNDENGINE_API AKRESULT SetRTPCValue( 
			const wchar_t* in_pszRtpcName,							///< Name of the game parameter (Unicode string)
			AkRtpcValue in_value, 									///< Value to set
			AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT ///< Associated game object ID
		    );

		/// Set the value of a real-time parameter control (by ansi string name).
		/// With this function, you may set a game parameter value on global scope or on game object scope. 
		/// Game object scope supersedes global scope. Game parameter values set on global scope are applied to all 
		/// game objects that not yet registered, or already registered but not overriden with a value on game object scope.
		/// To set a game parameter value on global scope, pass AK_INVALID_GAME_OBJECT as the game object. 
		/// Note that busses ignore RTPCs when they are applied on game object scope. Thus, you may only change bus 
		/// or bus plugins properties by calling this function with AK_INVALID_GAME_OBJECT.
		/// Refer to \ref soundengine_rtpc_pergameobject, \ref soundengine_rtpc_buses and 
		/// \ref soundengine_rtpc_effects for more details on RTPC scope.
		/// \return 
		/// - AK_Success if successful
		/// - AK_IDNotFound if in_pszRtpcName is NULL.
		/// \aknote Strings are case-sensitive. \endaknote
		/// \sa 
		/// - \ref soundengine_rtpc
        extern AKSOUNDENGINE_API AKRESULT SetRTPCValue( 
			const char* in_pszRtpcName,								///< Name of the game parameter (Ansi string)
			AkRtpcValue in_value, 									///< Value to set
			AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT ///< Associated game object ID
		    );

		/// Reset the value of the game parameter to its default value, as specified in the Wwise project.
		/// With this function, you may reset a game parameter to its default value on global scope or on game object scope. 
		/// Game object scope supersedes global scope. Game parameter values reset on global scope are applied to all 
		/// game objects that were not overriden with a value on game object scope.
		/// To reset a game parameter value on global scope, pass AK_INVALID_GAME_OBJECT as the game object. 
		/// Refer to \ref soundengine_rtpc_pergameobject, \ref soundengine_rtpc_buses and 
		/// \ref soundengine_rtpc_effects for more details on RTPC scope.
		/// \return Always AK_Success
		/// \sa 
		/// - \ref soundengine_rtpc
		/// - AK::SoundEngine::GetIDFromString()
		/// - AK::SoundEngine::SetRTPCValue()
		extern AKSOUNDENGINE_API AKRESULT ResetRTPCValue(
			AkRtpcID in_rtpcID, 									///< ID of the game parameter
			AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT	///< Associated game object ID
			);

		/// Reset the value of the game parameter to its default value, as specified in the Wwise project.
		/// With this function, you may reset a game parameter to its default value on global scope or on game object scope. 
		/// Game object scope supersedes global scope. Game parameter values reset on global scope are applied to all 
		/// game objects that were not overriden with a value on game object scope.
		/// To reset a game parameter value on global scope, pass AK_INVALID_GAME_OBJECT as the game object. 
		/// Refer to \ref soundengine_rtpc_pergameobject, \ref soundengine_rtpc_buses and 
		/// \ref soundengine_rtpc_effects for more details on RTPC scope.
		/// \return 
		/// - AK_Success if successful
		/// - AK_IDNotFound if in_pszParamName is NULL.
		/// \aknote Strings are case-sensitive. \endaknote
		/// \sa 
		/// - \ref soundengine_rtpc
		/// - AK::SoundEngine::SetRTPCValue()
		extern AKSOUNDENGINE_API AKRESULT ResetRTPCValue(
			const wchar_t* in_pszRtpcName,							///< Name of the game parameter (Unicode string)
			AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT	///< Associated game object ID
			);

		/// Reset the value of the game parameter to its default value, as specified in the Wwise project.
		/// With this function, you may reset a game parameter to its default value on global scope or on game object scope. 
		/// Game object scope supersedes global scope. Game parameter values reset on global scope are applied to all 
		/// game objects that were not overriden with a value on game object scope.
		/// To reset a game parameter value on global scope, pass AK_INVALID_GAME_OBJECT as the game object. 
		/// Refer to \ref soundengine_rtpc_pergameobject, \ref soundengine_rtpc_buses and 
		/// \ref soundengine_rtpc_effects for more details on RTPC scope.
		/// \return 
		/// - AK_Success if successful
		/// - AK_IDNotFound if in_pszParamName is NULL.
		/// \aknote Strings are case-sensitive. \endaknote
		/// \sa 
		/// - \ref soundengine_rtpc
		/// - AK::SoundEngine::SetRTPCValue()
		extern AKSOUNDENGINE_API AKRESULT ResetRTPCValue(
			const char* in_pszRtpcName,								///< Name of the game parameter  (Ansi string)
			AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT	///< Associated game object ID
			);

		/// Set the state of a switch group (by IDs).
		/// \return Always returns AK_Success
		/// \sa 
		/// - \ref soundengine_switch
		/// - AK::SoundEngine::GetIDFromString()
        extern AKSOUNDENGINE_API AKRESULT SetSwitch( 
			AkSwitchGroupID in_switchGroup, 			///< ID of the switch group
			AkSwitchStateID in_switchState, 			///< ID of the switch
			AkGameObjectID in_gameObjectID				///< Associated game object ID
		    );

		/// Set the state of a switch group (by unicode string names).
		/// \return 
		/// - AK_Success if successful
		/// - AK_IDNotFound if the switch or switch group name was not resolved to an existing ID\n
		/// Make sure that the banks were generated with the "include string" option.
		/// \aknote Strings are case-sensitive. \endaknote
		/// \sa 
		/// - \ref soundengine_switch
        extern AKSOUNDENGINE_API AKRESULT SetSwitch( 
			const wchar_t* in_pszSwitchGroup,			///< Name of the switch group (Unicode string)
			const wchar_t* in_pszSwitchState, 			///< Name of the switch (Unicode string)
			AkGameObjectID in_gameObjectID				///< Associated game object ID
			);

		/// Set the state of a switch group (by ansi string names).
		/// \return 
		/// - AK_Success if successful
		/// - AK_IDNotFound if the switch or switch group name was not resolved to an existing ID\n
		/// Make sure that the banks were generated with the "include string" option.
		/// \aknote Strings are case-sensitive. \endaknote
		/// \sa 
		/// - \ref soundengine_switch
        extern AKSOUNDENGINE_API AKRESULT SetSwitch( 
			const char* in_pszSwitchGroup,				///< Name of the switch group (Ansi string)
			const char* in_pszSwitchState, 				///< Name of the switch (Ansi string)
			AkGameObjectID in_gameObjectID				///< Associated game object ID
			);

		/// Post the specified trigger (by IDs).
		/// \return Always returns AK_Success
		/// \sa 
		/// - \ref soundengine_triggers
		/// - AK::SoundEngine::GetIDFromString()
		extern AKSOUNDENGINE_API AKRESULT PostTrigger( 
			AkTriggerID 	in_triggerID, 				///< ID of the trigger
			AkGameObjectID 	in_gameObjectID				///< Associated game object ID
		    );

		/// Post the specified trigger (by unicode string name).
		/// \return 
		/// - AK_Success if successful
		/// - AK_IDNotFound if the trigger name was not resolved to an existing ID\n
		/// Make sure that the banks were generated with the "include string" option.
		/// \aknote Strings are case-sensitive. \endaknote
		/// \sa 
		/// - \ref soundengine_triggers
        extern AKSOUNDENGINE_API AKRESULT PostTrigger( 
			const wchar_t* in_pszTrigger,				///< Name of the trigger (Unicode string)
			AkGameObjectID in_gameObjectID				///< Associated game object ID
			);

		/// Post the specified trigger (by ansi string name).
		/// \return 
		/// - AK_Success if successful
		/// - AK_IDNotFound if the trigger name was not resolved to an existing ID\n
		/// Make sure that the banks were generated with the "include string" option.
		/// \aknote Strings are case-sensitive. \endaknote
		/// \sa 
		/// - \ref soundengine_triggers
        extern AKSOUNDENGINE_API AKRESULT PostTrigger( 
			const char* in_pszTrigger,			 	    ///< Name of the trigger (Ansi string)
			AkGameObjectID in_gameObjectID				///< Associated game object ID
			);

		/// Set the state of a state group (by IDs).
		/// \return Always returns AK_Success
		/// \sa 
		/// - \ref soundengine_states
		/// - AK::SoundEngine::GetIDFromString()
        extern AKSOUNDENGINE_API AKRESULT SetState( 
			AkStateGroupID in_stateGroup, 				///< ID of the state group
			AkStateID in_state 							///< ID of the state
		    );

		/// Set the state of a state group (by unicode string names).
		/// \return 
		/// - AK_Success if successful
		/// - AK_IDNotFound if the state or state group name was not resolved to an existing ID\n
		/// Make sure that the banks were generated with the "include string" option.
		/// \aknote Strings are case-sensitive. \endaknote
		/// \sa 
		/// - \ref soundengine_states
		/// - AK::SoundEngine::GetIDFromString()
        extern AKSOUNDENGINE_API AKRESULT SetState( 
			const wchar_t* in_pszStateGroup,				///< Name of the state group (Unicode string)
			const wchar_t* in_pszState 						///< Name of the state (Unicode string)
			);

		/// Set the state of a state group (by ansi string names).
		/// \return 
		/// - AK_Success if successful
		/// - AK_IDNotFound if the state or state group name was not resolved to an existing ID\n
		/// Make sure that the banks were generated with the "include string" option.
		/// \aknote Strings are case-sensitive. \endaknote
		/// \sa 
		/// - \ref soundengine_states
		/// - AK::SoundEngine::GetIDFromString()
        extern AKSOUNDENGINE_API AKRESULT SetState( 
			const char* in_pszStateGroup,					///< Name of the state group (Ansi string)
			const char* in_pszState 						///< Name of the state (Ansi string)
			);

		//@}

		////////////////////////////////////////////////////////////////////////
		/// @name Environments
		//@{

		/// Set the environmental ratios to be used for the specified game object
		/// The array size cannot exceed AK_MAX_ENVIRONMENTS_PER_OBJ.
		/// To clear the game object's environments, in_uNumEnvValues must be 0.
		/// \aknote The actual maximum number of environments in which a game object can be is AK_MAX_ENVIRONMENTS_PER_OBJ. \endaknote
		/// \sa 
		/// - \ref soundengine_environments
		/// - \ref soundengine_environments_setting_environments
		/// - \ref soundengine_environments_id_vs_string
		/// - AK::SoundEngine::GetIDFromString()
		/// \return 
		/// - AK_Success if successful
		///	- AK_InvalidParameter if the array size exceeds AK_MAX_ENVIRONMENTS_PER_OBJ, or if a duplicated environment is found in the array
		extern AKSOUNDENGINE_API AKRESULT SetGameObjectEnvironmentsValues( 
			AkGameObjectID		in_gameObjectID,		///< Associated game object ID
			AkEnvironmentValue*	in_aEnvironmentValues,	///< Variable-size array of AkEnvironmentValue structures
														///< (it may be NULL if no environment must be set, and its size cannot exceed AK_MAX_ENVIRONMENTS_PER_OBJ)
			AkUInt32			in_uNumEnvValues		///< The number of environments at the pointer's address
														///< (it must be 0 if no environment is set, and can not exceed AK_MAX_ENVIRONMENTS_PER_OBJ)
			);

		/// Set the environmental dry level to be used for the specified game object
		/// The control value is a number ranging from 0.0f to 1.0f.
		/// 0.0f stands for 0% dry, while 1.0f stands for 100% dry.
		/// \aknote Reducing the dry level does not mean increasing the wet level. \endaknote
		/// \sa 
		/// - \ref soundengine_environments
		/// - \ref soundengine_environments_setting_dry_environment
		/// - \ref soundengine_environments_id_vs_string
		/// \return Always returns AK_Success
		extern AKSOUNDENGINE_API AKRESULT SetGameObjectDryLevelValue( 
			AkGameObjectID		in_gameObjectID,		///< Associated game object ID
			AkReal32			in_fControlValue		///< Dry level control value, ranging from 0.0f to 1.0f
														///< (0.0f stands for 0% dry, while 1.0f stands for 100% dry)
			);

		/// Set the same volume on all speaker for the specified environment.
		/// This function and SetEnvironmentVolumes aren't additive, they works on same data. 
		/// The volume is a number ranging from 0.0f to 1.0f.
		/// 0.0f stands for 0% of the environment volume, while 1.0f stands for 100% of the environment volume.
		/// \sa 
		/// - \ref soundengine_environments
		/// - \ref soundengine_environments_setting_environment_volume
		/// - AK::SoundEngine::GetIDFromString()
		/// \return Always returns AK_Success
		extern AKSOUNDENGINE_API AKRESULT SetEnvironmentVolume( 
			AkEnvID				in_FXParameterSetID,	///< Environment ID
			AkReal32			in_fVolume				///< Volume control value, ranging from 0.0f to 1.0f.
														///< (0.0f stands for 0% of the environment volume, 1.0f stands for 100% of the environment volume)
			);

		/// Set speakers volumes for the specified environment.
		/// This function and SetEnvironmentVolume aren't additive, they works on same data.
		/// This function do nothing on the Wii platform, use SetEnvironmentVolume instead.
		/// The volume is a number ranging from 0.0f to 1.0f.
		/// 0.0f stands for 0% of the environment volume, while 1.0f stands for 100% of the environment volume.
		/// \sa 
		/// - \ref soundengine_environments
		/// - \ref soundengine_environments_setting_environment_volume
		/// - AK::SoundEngine::GetIDFromString()
		/// \return Always returns AK_Success
		extern AKSOUNDENGINE_API AKRESULT SetEnvironmentVolumes( 
			const AkEnvID			in_FXParameterSetID,	///< Environment ID
			const AkSpeakerVolumes&	in_sVolumes				///< Speakers Volumes controls values, ranging from 0.0f to 1.0f.
															///< (0.0f stands for 0% of the environment volume, 1.0f stands for 100% of the environment volume)
			);
			
		/// Bypass an environment.
		/// The specified environment will not be computed when bypassed.
		/// This method is useful to disable environments that should not be heard, and therefore save resources.
		/// \sa 
		/// - \ref soundengine_environments
		/// - \ref soundengine_environments_bypassing_environments
		/// - \ref soundengine_environments_id_vs_string
		/// - AK::SoundEngine::GetIDFromString()
		/// \return Always returns AK_Success
		extern AKSOUNDENGINE_API AKRESULT BypassEnvironment(
			AkEnvID	in_FXParameterSetID,				///< Environment ID
			bool	in_bIsBypassed						///< True: bypass the specified environment
			);

		/// Set an effect shareset at the specified audio node and effect slot index.
		/// This adds a reference on the audio node to an existing ShareSet.
		/// \aknote This function has unspecified behavior when adding an effect to a currently playing
		/// bus which does not have any effects, or removing the last effect on a currently playing bus.
		/// \return Always returns AK_Success
		extern AKSOUNDENGINE_API AKRESULT SetEffect( 
			AkUniqueID in_audioNodeID,					///< Can be a member of the bus, actor-mixer or interactive music hierarchy
			AkUInt32 in_uFXIndex,						///< Effect slot index (0-3)
			AkUniqueID in_shareSetID					///< ShareSet ID; pass AK_INVALID_UNIQUE_ID to clear the effect slot
			);

		/// Set a game object's obstruction and occlusion levels.
		/// This method is used to affect how an object should be heard by a specific listener.
		/// \sa 
		/// - \ref soundengine_obsocc
		/// - \ref soundengine_environments
		/// \return Always returns AK_Success
		extern AKSOUNDENGINE_API AKRESULT SetObjectObstructionAndOcclusion(  
			AkGameObjectID in_ObjectID,			///< Associated game object ID
			AkUInt32 in_uListener,				///< Listener index (0: first listener, 7: 8th listener)
			AkReal32 in_fObstructionLevel,		///< ObstructionLevel: [0.0f..1.0f]
			AkReal32 in_fOcclusionLevel			///< OcclusionLevel: [0.0f..1.0f]
			);

		//@}
        
        ////////////////////////////////////////////////////////////////////////
		/// @name Audio output capture
		//@{
         

		/// Start recording the sound engine audio output. 
		/// StartOutputCapture outputs a wav file in the 5.1 configuration with the channels in this order:
		/// <ol>
		///		<li>Front Left</li>
		///		<li>Front Right</li>
		///		<li>Center</li>
		///		<li>LFE</li>
		///		<li>Surround Left</li>
		///		<li>Surround Right</li>
		/// </ol>
		/// This function is not thread-safe. 
		/// \return AK_Success if successful, AK_Fail if there was a problem starting the output capture.
		///			In optimized mode, this function returns AK_NotCompatible.
		/// \remark 
		///		- This function is provided as a utility tool only. It does nothing if it is 
		///			called in the optimized/release configuration and return AK_NotCompatible.
		///		- The sound engine opens a stream for writing using AK::IAkStreamMgr::CreateStd(). If you are using the
		///			default implementation of the Stream Manager, file opening is executed in your implementation of 
		///			the Low-Level IO interface AK::StreamMgr::IAkFileLocationResolver::Open(). The following 
		///			AkFileSystemFlags are passed: uCompanyID = AKCOMPANYID_AUDIOKINETIC and uCodecID = AKCODECID_PCM,
		///			and the AkOpenMode is AK_OpenModeWriteOvrwr. Refer to \ref streamingmanager_lowlevel_location for
		///			more details on managing the deployement of your Wwise generated data.
		/// \sa 
		/// - AK::SoundEngine::StopOutputCapture()
		/// - AK::StreamMgr::SetFileLocationResolver()
		/// - \ref streamingdevicemanager
		/// - \ref streamingmanager_lowlevel_location
		extern AKSOUNDENGINE_API AKRESULT StartOutputCapture( 
			const AkOSChar* in_CaptureFileName				///< Name of the output capture file
			);

		/// Stop recording the sound engine audio output. 
		/// This function is not thread-safe. 
		/// \return AK_Success if successful, AK_Fail if there was a problem stopping the output capture.
		///			In optimized mode, this function returns AK_NotCompatible.
		/// \remark This function is provided as a utility tool only. It does nothing if it is 
		///			called in the optimized/release configuration and return AK_NotCompatible.
		/// \sa 
		/// - AK::SoundEngine::StartOutputCapture()
		extern AKSOUNDENGINE_API AKRESULT StopOutputCapture();
			
		//@}
	}
}

#endif // _AK_SOUNDENGINE_H_
