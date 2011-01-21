//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#ifndef _AKMONITORERROR_H
#define _AKMONITORERROR_H

#include <AK/SoundEngine/Common/AkSoundEngineExport.h>
#include <AK/SoundEngine/Common/AkTypes.h>

namespace AK
{
    // Error monitoring.

	namespace Monitor
	{
		///  ErrorLevel
		enum ErrorLevel
		{
			ErrorLevel_Message	= (1<<0), // used as bitfield
			ErrorLevel_Error	= (1<<1),
			
			ErrorLevel_All = ErrorLevel_Message | ErrorLevel_Error
		};
		/// ErrorCode
		enum ErrorCode
		{
			ErrorCode_NoError = 0, // 0-based index into AK::Monitor::s_aszErrorCodes table 
			ErrorCode_FileNotFound, 
			ErrorCode_CannotOpenFile,
			ErrorCode_CannotStartStreamNoMemory,
			ErrorCode_IODevice,

			ErrorCode_PluginUnsupportedChannelConfiguration,
			ErrorCode_PluginMediaUnavailable,
			ErrorCode_PluginInitialisationFailed,
			ErrorCode_PluginProcessingFailed,
			ErrorCode_PluginExecutionInvalid,
			ErrorCode_PluginAllocationFailed,

			ErrorCode_VorbisRequireSeekTable,
			ErrorCode_VorbisRequireSeekTableVirtual,

			ErrorCode_VorbisDecodeError,
			
			ErrorCode_xWMACreateDecoderFailed,

			ErrorCode_InvalidAudioFileHeader,
			ErrorCode_FileTooSmall,

			ErrorCode_TransitionNotAccurateChannel,
			ErrorCode_TransitionNotAccurateStarvation,
			ErrorCode_NothingToPlay, 
			ErrorCode_PlayFailed,

			ErrorCode_StingerCouldNotBeScheduled,
			ErrorCode_TooLongSegmentLookAhead,
			ErrorCode_CannotScheduleMusicSwitch,
			ErrorCode_TooManySimultaneousMusicSegments,

			ErrorCode_CannotPlaySource_Create,
			ErrorCode_CannotPlaySource_VirtualOff,
			ErrorCode_CannotPlaySource_TimeSkip,
			ErrorCode_CannotPlaySource_InconsistentState,
			ErrorCode_MediaNotLoaded,
			ErrorCode_VoiceStarving,
			ErrorCode_StreamingSourceStarving,
			ErrorCode_XMADecoderSourceStarving,

			ErrorCode_PluginNotRegistered,
			ErrorCode_CodecNotRegistered,

			ErrorCode_EventIDNotFound,

			ErrorCode_InvalidGroupID,
			ErrorCode_SelectedChildNotAvailable,
			ErrorCode_SelectedNodeNotAvailable,
			ErrorCode_SelectedMediaNotAvailable,
			ErrorCode_NoValidSwitch,

			ErrorCode_SelectedNodeNotAvailablePlay,

			ErrorCode_FeedbackVoiceStarving,

			ErrorCode_BankLoadFailed,
			ErrorCode_BankUnloadFailed,
			ErrorCode_ErrorWhileLoadingBank,
			ErrorCode_InsufficientSpaceToLoadBank,
			
			ErrorCode_LowerEngineCommandListFull,

			ErrorCode_SeekNoMarker,
			ErrorCode_CannotSeekContinuous,
			ErrorCode_SeekAfterEof,

			ErrorCode_UnknownGameObjectEvent,
			ErrorCode_UnknownGameObject,

			ErrorCode_ExternalSourceNotResolved,
			ErrorCode_FileFormatMismatch,

			Num_ErrorCodes // THIS STAYS AT END OF ENUM
		};

		/// Function prototype of local output function pointer.
		typedef void( *LocalOutputFunc )(
			ErrorCode in_eErrorCode,	///< Error code number value
			const wchar_t* in_pszError,	///< Message or error string to be displayed
			ErrorLevel in_eErrorLevel,	///< Specifies whether it should be displayed as a message or an error
			AkPlayingID in_playingID,   ///< Related Playing ID if applicable, AK_INVALID_PLAYING_ID otherwise
			AkGameObjectID in_gameObjID ///< Related Game Object ID if applicable, AK_INVALID_GAME_OBJECT otherwise
			);

		extern const wchar_t* s_aszErrorCodes[ Num_ErrorCodes ];

		/// Post a monitoring message or error code. This will be displayed in the Wwise capture
		/// log.
		/// \return AK_Success if successful, AK_Fail if there was a problem posting the message.
		///			In optimized mode, this function returns AK_NotCompatible.
		/// \remark This function is provided as a tracking tool only. It does nothing if it is 
		///			called in the optimized/release configuration and return AK_NotCompatible.
		extern AKSOUNDENGINE_API AKRESULT PostCode( 
			ErrorCode in_eError,		///< Message or error code to be displayed
			ErrorLevel in_eErrorLevel	///< Specifies whether it should be displayed as a message or an error
			);

		/// Post a unicode monitoring message or error string. This will be displayed in the Wwise capture
		/// log.
		/// \return AK_Success if successful, AK_Fail if there was a problem posting the message.
		///			In optimized mode, this function returns AK_NotCompatible.
		/// \remark This function is provided as a tracking tool only. It does nothing if it is 
		///			called in the optimized/release configuration and return AK_NotCompatible.
		extern AKSOUNDENGINE_API AKRESULT PostString( 
			const wchar_t* in_pszError,	///< Message or error string to be displayed (Unicode string)
			ErrorLevel in_eErrorLevel	///< Specifies whether it should be displayed as a message or an error
			);

		/// Post an ansi monitoring message or error string. This will be displayed in the Wwise capture
		/// log.
		/// \return AK_Success if successful, AK_Fail if there was a problem posting the message.
		///			In optimized mode, this function returns AK_NotCompatible.
		/// \remark This function is provided as a tracking tool only. It does nothing if it is 
		///			called in the optimized/release configuration and return AK_NotCompatible.
		extern AKSOUNDENGINE_API AKRESULT PostString( 
			const char* in_pszError,	///< Message or error string to be displayed (Ansi string)
			ErrorLevel in_eErrorLevel	///< Specifies whether it should be displayed as a message or an error
			);

		/// Enable/Disable local output of monitoring messages or errors. Pass 0 to disable,
		/// or any combination of ErrorLevel_Message and ErrorLevel_Error to enable. 
		/// \return AK_Success.
		///			In optimized/release configuration, this function returns AK_NotCompatible.
		extern AKSOUNDENGINE_API AKRESULT SetLocalOutput(
			AkUInt32 in_uErrorLevel	= ErrorLevel_All, ///< ErrorLevel(s) to enable in output. Default parameters enable all.
			LocalOutputFunc in_pMonitorFunc = 0 	  ///< Handler for local output. If NULL, the standard platform debug output method is used.
			);

		/// Get the time stamp shown in the capture log along with monitoring messages.
		/// \return AK_Success.
		///			In optimized/release configuration, this function returns 0.
		extern AKSOUNDENGINE_API AkTimeMs GetTimeStamp();
	}
}

// Macros.
#ifndef AK_OPTIMIZED
    #define AK_MONITOR_ERROR( in_eErrorCode )\
	AK::Monitor::PostCode( in_eErrorCode, AK::Monitor::ErrorLevel_Error )
#else
    #define AK_MONITOR_ERROR( in_eErrorCode )
#endif

#ifdef AK_MONITOR_IMPLEMENT_ERRORCODES
namespace AK
{
	namespace Monitor
	{
		const wchar_t* s_aszErrorCodes[ Num_ErrorCodes ] =
		{
			L"No error", // ErrorCode_NoError
			L"File not found", // ErrorCode_FileNotFound,
			L"Cannot open file", // ErrorCode_CannotOpenFile,
			L"Not enough memory to start stream", // ErrorCode_CannotStartStreamNoMemory,
			L"IO device error", // ErrorCode_IODevice,

			L"Plug-in unsupported channel configuration", // ErrorCode_PluginUnsupportedChannelConfiguration,
			L"Plug-in media unavailable", // ErrorCode_PluginMediaUnavailable,
			L"Plug-in initialization failure", // ErrorCode_PluginInitialisationFailed,
			L"Plug-in execution failure", // ErrorCode_PluginProcessingFailed,
			L"Invalid plug-in execution mode", // ErrorCode_PluginExecutionInvalid
			L"Could not allocate effect", // ErrorCode_PluginAllocationFailed

			L"Seek table required to seek in Vorbis sources. Please update conversion settings.", // ErrorCode_VorbisRequireSeekTable,
			L"Seek table needed for Vorbis audio format with this virtual voice behavior. Please update conversion settings or virtual mode.", // ErrorCode_VorbisRequireSeekTableVirtual,

			L"Vorbis decoder failure", // ErrorCode_VorbisDecodeError,

			L"Failed creating xWMA decoder", // ErrorCode_xWMACreateDecoderFailed,

			L"Invalid file header", // ErrorCode_InvalidAudioFileHeader,
			L"File or loop region is too small to be played properly", // ErrorCode_FileTooSmall,

			L"Transition not sample-accurate due to mixed channel configurations", // ErrorCode_TransitionNotAccurateChannel,
			L"Transition not sample-accurate due to source starvation", // ErrorCode_TransitionNotAccurateStarvation,
			L"Nothing to play", // ErrorCode_NothingToPlay, 
			L"Play Failed", // ErrorCode_PlayFailed,	// Notification meaning the play asked was not done for an out of control reason
											// For example, if The Element has a missing source file.

			L"Stinger could not be scheduled or was dropped", // ErrorCode_StingerCouldNotBeScheduled,
			L"Segment look-ahead is longer than previous segment in sequence", // ErrorCode_TooLongSegmentLookAhead,
			L"Cannot schedule music switch transition in upcoming segments: using Exit Cue", // ErrorCode_CannotScheduleMusicSwitch,
			L"Cannot schedule music segments: Stopping music", // ErrorCode_TooManySimultaneousMusicSegments,

			L"Failed creating source", // ErrorCode_CannotPlaySource_Create,
			L"Virtual source failed becoming physical", // ErrorCode_CannotPlaySource_VirtualOff,
			L"Error while computing virtual source elapsed time", // ErrorCode_CannotPlaySource_TimeSkip,
			L"Inconsistent source status", // ErrorCode_CannotPlaySource_InconsistentState,
			L"Media was not loaded for this source",// ErrorCode_MediaNotLoaded,
			L"Voice Starvation", // ErrorCode_VoiceStarving,
			L"Source starvation", // ErrorCode_StreamingSourceStarving,
			L"XMA decoder starvation", // ErrorCode_XMADecoderSourceStarving,

			L"Plug-in not registered", // ErrorCode_PluginNotRegistered,
			L"Codec plug-in not registered", // ErrorCode_CodecNotRegistered,

			L"Event ID not found", // ErrorCode_EventIDNotFound,

			L"Invalid Group ID", // ErrorCode_InvalidGroupID,
			L"Selected Child Not Available", // ErrorCode_SelectedChildNotAvailable,
			L"Selected Node Not Available", // ErrorCode_SelectedNodeNotAvailable,
			L"Selected Media Not Available",// ErrorCode_SelectedMediaNotAvailable,
			L"No Valid Switch", // ErrorCode_NoValidSwitch,

			L"Selected node not available. Make sure the structure associated to the event is loaded or that the event has been prepared", // ErrorCode_SelectedNodeNotAvailablePlay,

			L"Motion voice starvation", // ErrorCode_FeedbackVoiceStarving,

			L"Bank Load Failed", // ErrorCode_BankLoadFailed,
			L"Bank Unload Failed", // ErrorCode_BankUnloadFailed,
			L"Error while loading bank", // ErrorCode_ErrorWhileLoadingBank,
			L"Insufficient Space to Load Bank", // ErrorCode_InsufficientSpaceToLoadBank,

			L"Lower engine command list is full", // ErrorCode_LowerEngineCommandListFull,

			L"No marker in file; seeking to specified location", // ErrorCode_SeekNoMarker
			L"Cannot in sound that is within a continuous container with special transitions", // ErrorCode_CannotSeekContinuous
			L"Seeking after end of file. Playback will stop", // ErrorCode_SeekAfterEof

			L"Unknown game object ID. Make sure the game object is registered before using it and do not use it once it was unregistered.", // ErrorCode_UnknownGameObject,
			L"Unknown game object ID. Make sure the game object is registered before using it and do not use it once it was unregistered.", // ErrorCode_UnknownGameObjectEvent

			L"External source missing from PostEvent call", // ErrorCode_ExternalSourceNotResolved
			L"Source file is of different format than expected", //ErrorCode_FileFormatMismatch
		};
	}
}
#endif // AK_MONITOR_IMPLEMENT_ERRORCODES

#endif // _AKMONITORERROR_H
