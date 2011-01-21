// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
// AkQueryParameters.h

/// \file 
/// The sound engine parameter query interface.


#ifndef _AK_QUERYPARAMS_H_
#define _AK_QUERYPARAMS_H_

#include <AK/SoundEngine/Common/AkSoundEngineExport.h>
#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/Tools/Common/AkArray.h>

/// Positioning information obtained from an object
struct AkPositioningInfo
{
	AkReal32			fCenterPct;			///< Center %
	AkPositioningType	positioningType;	///< Positioning Type (2D, 3D gamedef, 3D userdef)
	bool				bUpdateEachFrame;   ///< Update at each frame (valid only with game-defined)
	bool				bUseSpatialization; ///< Use spatialization
	bool				bUseAttenuation;	///< Use attenuation parameter set

	bool				bUseConeAttenuation; ///< Use the cone attenuation
	AkReal32			fInnerAngle;		///< Inner angle
	AkReal32			fOuterAngle;		///< Outer angle
	AkReal32			fConeMaxAttenuation; ///< Cone max attenuation
	AkLPFType			LPFCone;			///< Cone low pass filter value

	AkReal32			fMaxDistance;		///< Maximum distance
	AkReal32			fVolDryAtMaxDist;	///< Volume dry at maximum distance
	AkReal32			fVolWetAtMaxDist;	///< Volume wet at maximum distance (if any)
	AkLPFType			LPFValueAtMaxDist;  ///< Low pass filter value at max distance (if any)
};

/// Object information structure for QueryAudioObjectsIDs
struct AkObjectInfo
{
	AkUniqueID	objID;		///< Object ID
	AkUniqueID	parentID;	///< Object ID of the parent 
	AkInt32		iDepth;		///< Depth in tree
};

// Audiokinetic namespace
namespace AK
{
	// Audiokinetic sound engine namespace
	namespace SoundEngine
	{
		/// Query namespace
		/// \remarks The functions in this namespace are thread-safe, unless stated otherwise.
		///
		/// \warning The functions in this namespace might stall for several milliseconds before returning (as they
		/// cannot execute while the main sound engine thread is busy). They should therefore not be called from any 
		/// game critical thread, such as the main game loop.
		///
		/// \warning There might be a significant delay between a Sound Engine call (such as PostEvent) and
		/// the information being reflected in a Query (such as GetIsGameObjectActive). 

		namespace Query
		{
			////////////////////////////////////////////////////////////////////////
			/// @name Game Objects
			//@{

			/// Get the position of a game object.
			/// \return AK_Success if succeeded, or AK_IDNotFound if the game object was not registered
			/// \sa 
			/// - \ref soundengine_3dpositions
			extern AKSOUNDENGINE_API AKRESULT GetPosition( 
				AkGameObjectID in_GameObjectID,				///< Game object identifier
				AkSoundPosition& out_rPosition				///< Position to get
				);

			//@}

			////////////////////////////////////////////////////////////////////////
			/// @name Listeners
			//@{

			/// Get a game object's active listeners.
			/// \return AK_Success if succeeded, or AK_IDNotFound if the game object was not registered
			/// \sa 
			/// - \ref soundengine_listeners_multi_assignobjects
			extern AKSOUNDENGINE_API AKRESULT GetActiveListeners(
				AkGameObjectID in_GameObjectID,				///< Game object identifier
				AkUInt32& out_ruListenerMask				///< Bitmask representing the active listeners (LSB = Listener 0, set to 1 means active)
				);

			/// Get a listener's position.
			/// \return AK_Success if succeeded, or AK_InvalidParameter if the index is out of range
			/// \sa 
			/// - \ref soundengine_listeners_settingpos
			extern AKSOUNDENGINE_API AKRESULT GetListenerPosition( 
				AkUInt32 in_uIndex, 						///< Listener index (0: first listener, 7: 8th listener)
				AkListenerPosition& out_rPosition			///< Position set
				);

			/// Get a listener's spatialization parameters. 
			/// \return AK_Success if succeeded, or AK_InvalidParameter if the index is out of range
			/// \sa 
			/// - \ref soundengine_listeners_spatial
			extern AKSOUNDENGINE_API AKRESULT GetListenerSpatialization(
				AkUInt32 in_uIndex,							///< Listener index (0: first listener, 7: 8th listener)
				bool& out_rbSpatialized,					///< Spatialization state
				AkSpeakerVolumes& out_rVolumeOffsets		///< Per-speaker volume offset, in dB (Only used if in_bSpatialized == false)
				);

			//@}


			////////////////////////////////////////////////////////////////////////
			/// @name Game Syncs
			//@{

			/// Enum used to request a specific RTPC Value.
			/// Also used to inform the user of where the RTPC Value comes from.
			///
			/// For example, the user may request the GameObject specific value by specifying RTPCValue_GameObject
			/// and can receive the Global Value if there was no GameObject specific value, and even the 
			/// default value is there was no Global value either.
			/// \sa 
			/// - \ref GetRTPCValue
			enum RTPCValue_type
			{
				RTPCValue_Default,		///< The value is the Default RTPC.
				RTPCValue_Global,		///< The value is the Global RTPC.
				RTPCValue_GameObject,	///< The value is the game object specific RTPC.
				RTPCValue_Unavailable	///< The value is not available for the RTPC specified.
			};

			/// Get the value of a real-time parameter control (by ID).
			/// \return AK_Success if succeeded, AK_IDNotFound if the game object was not registered, or AK_Fail if the RTPC value could not be obtained
			/// \sa 
			/// - \ref soundengine_rtpc
			/// - \ref RTPCValue_type
			extern AKSOUNDENGINE_API AKRESULT GetRTPCValue( 
				AkRtpcID in_rtpcID, 				///< ID of the RTPC
				AkGameObjectID in_gameObjectID,		///< Associated game object ID
				AkRtpcValue& out_rValue, 			///< Value returned
				RTPCValue_type&	io_rValueType		///< In/Out value, the user must specify the requested type. The function will return in this variable the type of the returned value.
				);

			/// Get the value of a real-time parameter control (by unicode string name).
			/// \return AK_Success if succeeded, AK_IDNotFound if the game object was not registered or the rtpc name could not be found, or AK_Fail if the RTPC value could not be obtained
			/// \sa 
			/// - \ref soundengine_rtpc
			/// - \ref RTPCValue_type
			extern AKSOUNDENGINE_API AKRESULT GetRTPCValue( 
				const wchar_t* in_pszRtpcName,		///< Unicode string name of the RTPC
				AkGameObjectID in_gameObjectID,		///< Associated game object ID
				AkRtpcValue& out_rValue, 			///< Value returned
				RTPCValue_type&	io_rValueType		///< In/Out value, the user must specify the requested type. The function will return in this variable the type of the returned value.
				);

			/// Get the value of a real-time parameter control (by ansi string name).
			/// \return AK_Success if succeeded, AK_IDNotFound if the game object was not registered or the rtpc name could not be found, or AK_Fail if the RTPC value could not be obtained
			/// \sa 
			/// - \ref soundengine_rtpc
			/// - \ref RTPCValue_type
			extern AKSOUNDENGINE_API AKRESULT GetRTPCValue( 
				const char* in_pszRtpcName,			///< Ansi string name of the RTPC
				AkGameObjectID in_gameObjectID,		///< Associated game object ID
				AkRtpcValue& out_rValue, 			///< Value returned
				RTPCValue_type&	io_rValueType		///< In/Out value, the user must specify the requested type. The function will return in this variable the type of the returned value.
				);

			/// Get the state of a switch group (by IDs).
			/// \return AK_Success if succeeded, or AK_IDNotFound if the game object was not registered
			/// \sa 
			/// - \ref soundengine_switch
			extern AKSOUNDENGINE_API AKRESULT GetSwitch( 
				AkSwitchGroupID in_switchGroup, 			///< ID of the switch group
				AkGameObjectID  in_gameObjectID,			///< Associated game object ID
				AkSwitchStateID& out_rSwitchState 			///< ID of the switch
				);

			/// Get the state of a switch group (by unicode string name).
			/// \return AK_Success if succeeded, or AK_IDNotFound if the game object was not registered or the switch group name can not be found
			/// \sa 
			/// - \ref soundengine_switch
			extern AKSOUNDENGINE_API AKRESULT GetSwitch( 
				const wchar_t* in_pstrSwitchGroupName,			///< Unicode string name of the switch group
				AkGameObjectID in_GameObj,					///< Associated game object ID
				AkSwitchStateID& out_rSwitchState			///< ID of the switch
				);

			/// Get the state of a switch group (by ansi string name).
			/// \return AK_Success if succeeded, or AK_IDNotFound if the game object was not registered or the switch group name can not be found
			/// \sa 
			/// - \ref soundengine_switch
			extern AKSOUNDENGINE_API AKRESULT GetSwitch( 
				const char* in_pstrSwitchGroupName,			///< Ansi string name of the switch group
				AkGameObjectID in_GameObj,					///< Associated game object ID
				AkSwitchStateID& out_rSwitchState			///< ID of the switch
				);

			/// Get the state of a state group (by IDs).
			/// \return AK_Success if succeeded
			/// \sa 
			/// - \ref soundengine_states
			extern AKSOUNDENGINE_API AKRESULT GetState( 
				AkStateGroupID in_stateGroup, 				///< ID of the state group
				AkStateID& out_rState 						///< ID of the state
				);

			/// Get the state of a state group (by unicode string name).
			/// \return AK_Success if succeeded, or AK_IDNotFound if the state group name can not be found
			/// \sa 
			/// - \ref soundengine_states
			extern AKSOUNDENGINE_API AKRESULT GetState( 
				const wchar_t* in_pstrStateGroupName,			///< Unicode string name of the state group
				AkStateID& out_rState						///< ID of the state
				);

			/// Get the state of a state group (by ansi string name).
			/// \return AK_Success if succeeded, or AK_IDNotFound if the state group name can not be found
			/// \sa 
			/// - \ref soundengine_states
			extern AKSOUNDENGINE_API AKRESULT GetState( 
				const char* in_pstrStateGroupName,			    ///< Ansi string name of the state group
				AkStateID& out_rState						///< ID of the state
				);

			//@}

			////////////////////////////////////////////////////////////////////////
			/// @name Environments
			//@{

			/// Get the environmental ratios used by the specified game object.
			/// The array size cannot exceed AK_MAX_ENVIRONMENTS_PER_OBJ.
			/// To clear the game object's environments, in_uNumEnvValues must be 0.
			/// \aknote The actual maximum number of environments in which a game object can be is AK_MAX_ENVIRONMENTS_PER_OBJ. \endaknote
			/// \sa 
			/// - \ref soundengine_environments
			/// - \ref soundengine_environments_setting_environments
			/// - \ref soundengine_environments_id_vs_string
			/// \return AK_Success if succeeded, or AK_InvalidParameter if io_ruNumEnvValues is 0 or out_paEnvironmentValues is NULL, or AK_PartialSuccess if more environments exist than io_ruNumEnvValues
			/// AK_InvalidParameter
			extern AKSOUNDENGINE_API AKRESULT GetGameObjectEnvironmentsValues( 
				AkGameObjectID		in_gameObjectID,		///< Associated game object ID
				AkEnvironmentValue*	out_paEnvironmentValues,	///< Variable-size array of AkEnvironmentValue structures
																///< (it may be NULL if no environment must be set, and its size 
																///<�cannot exceed AK_MAX_ENVIRONMENTS_PER_OBJ)
				AkUInt32&			io_ruNumEnvValues		///< The number of environments at the pointer's address
															///< (it must be 0 if no environment is set, and can not exceed AK_MAX_ENVIRONMENTS_PER_OBJ)
				);

			/// Get the environmental dry level to be used for the specified game object
			/// The control value is a number ranging from 0.0f to 1.0f.
			/// 0.0f stands for 0% dry, while 1.0f stands for 100% dry.
			/// \aknote Reducing the dry level does not mean increasing the wet level. \endaknote
			/// \sa 
			/// - \ref soundengine_environments
			/// - \ref soundengine_environments_setting_dry_environment
			/// - \ref soundengine_environments_id_vs_string
			/// \return AK_Success if succeeded, or AK_IDNotFound if the game object was not registered
			extern AKSOUNDENGINE_API AKRESULT GetGameObjectDryLevelValue( 
				AkGameObjectID		in_gameObjectID,		///< Associated game object ID
				AkReal32&			out_rfControlValue		///< Dry level control value, ranging from 0.0f to 1.0f
															///< (0.0f stands for 0% dry, while 1.0f stands for 100% dry)
				);

			/// Get the volume for the specified environment.
			/// If environment volumes have been set with SetEnvironmentVolumes this function return the frontleft speaker value.
			/// The volume is a number ranging from 0.0f to 1.0f.
			/// 0.0f stands for 0% of the environment volume, while 1.0f stands for 100% of the environment volume.
			/// \sa 
			/// - \ref soundengine_environments
			/// - \ref soundengine_environments_setting_environment_volume
			/// \return AK_Success if succeeded
			extern AKSOUNDENGINE_API AKRESULT GetEnvironmentVolume( 
				AkEnvID				in_FXParameterSetID,	///< Environment ID
				AkReal32&			out_rfVolume			///< Volume control value, ranging from 0.0f to 1.0f.
															///< (0.0f stands for 0% of the environment volume, 1.0f stands for 100% of the environment volume)
				);
			
			/// Get the speakers volumes for the specified environment.
			/// On Wii platform this function return a filled structure with the value returned from GetEnvironmentVolume. On wii, use GetEnvironmentVolume and SetEnvrionmentVolume instead.
			/// The volumes are numbers ranging from 0.0f to 1.0f.
			/// 0.0f stands for 0% of the environment volume, while 1.0f stands for 100% of the environment volume.
			/// \sa 
			/// - \ref soundengine_environments
			/// - \ref soundengine_environments_setting_environment_volume
			/// \return AK_Success if succeeded
			extern AKSOUNDENGINE_API AKRESULT GetEnvironmentVolumes( 
				const AkEnvID		in_FXParameterSetID,	///< Environment ID
				AkSpeakerVolumes&	out_rsVolumes			///< Volume control value, ranging from 0.0f to 1.0f.
															///< (0.0f stands for 0% of the environment volume, 1.0f stands for 100% of the environment volume)
				);

			/// Get the Bypass value for an environment.
			/// \sa 
			/// - \ref soundengine_environments
			/// - \ref soundengine_environments_bypassing_environments
			/// - \ref soundengine_environments_id_vs_string
			/// \return AK_Success if succeeded
			extern AKSOUNDENGINE_API AKRESULT GetEnvironmentBypass(
				AkEnvID	in_FXParameterSetID,				///< Environment ID
				bool&	out_rbIsBypassed					///< True if bypass the specified environment
				);

			/// Get a game object's obstruction and occlusion levels.
			/// \sa 
			/// - \ref soundengine_obsocc
			/// - \ref soundengine_environments
			/// \return AK_Success if succeeded, AK_IDNotFound if the game object was not registered
			extern AKSOUNDENGINE_API AKRESULT GetObjectObstructionAndOcclusion(  
				AkGameObjectID in_ObjectID,			///< Associated game object ID
				AkUInt32 in_uListener,				///< Listener index (0: first listener, 7: 8th listener)
				AkReal32& out_rfObstructionLevel,		///< ObstructionLevel: [0.0f..1.0f]
				AkReal32& out_rfOcclusionLevel			///< OcclusionLevel: [0.0f..1.0f]
				);

			//@}

			/// Get the list of audio object IDs associated to an event.
			/// \aknote It is possible to call QueryAudioObjectIDs with io_ruNumItems = 0 to get the total size of the
			/// structure that should be allocated for out_aObjectInfos. \endaknote
			/// \return AK_Success if succeeded, AK_IDNotFound if the eventID cannot be found, AK_InvalidParameter if out_aObjectInfos is NULL while io_ruNumItems > 0
			/// or AK_UnknownObject if the event contains an unknown audio object, 
			/// or AK_PartialSuccess if io_ruNumItems was set to 0 to query the number of available items.
			extern AKSOUNDENGINE_API AKRESULT QueryAudioObjectIDs(
				AkUniqueID in_eventID,				///< Event ID
				AkUInt32& io_ruNumItems,			///< Number of items in array provided / Number of items filled in array
				AkObjectInfo* out_aObjectInfos		///< Array of AkObjectInfo items to fill
				);

			/// Get the list of audio object IDs associated to a unicode event name.
			/// \aknote It is possible to call QueryAudioObjectIDs with io_ruNumItems = 0 to get the total size of the
			/// structure that should be allocated for out_aObjectInfos. \endaknote
			/// \return AK_Success if succeeded, AK_IDNotFound if the event name cannot be found, AK_InvalidParameter if out_aObjectInfos is NULL while io_ruNumItems > 0
			/// or AK_UnknownObject if the event contains an unknown audio object, 
			/// or AK_PartialSuccess if io_ruNumItems was set to 0 to query the number of available items.
			extern AKSOUNDENGINE_API AKRESULT QueryAudioObjectIDs(
				const wchar_t* in_pszEventName,		///< Unicode event name
				AkUInt32& io_ruNumItems,			///< Number of items in array provided / Number of items filled in array
				AkObjectInfo* out_aObjectInfos		///< Array of AkObjectInfo items to fill
				);

			/// Get the list of audio object IDs associated to an ansi event name.
			/// \aknote It is possible to call QueryAudioObjectIDs with io_ruNumItems = 0 to get the total size of the
			/// structure that should be allocated for out_aObjectInfos. \endaknote
			/// \return AK_Success if succeeded, AK_IDNotFound if the event name cannot be found, AK_InvalidParameter if out_aObjectInfos is NULL while io_ruNumItems > 0
			/// or AK_UnknownObject if the event contains an unknown audio object, 
			/// or AK_PartialSuccess if io_ruNumItems was set to 0 to query the number of available items.
			extern AKSOUNDENGINE_API AKRESULT QueryAudioObjectIDs(
				const char* in_pszEventName,		///< Ansi event name
				AkUInt32& io_ruNumItems,			///< Number of items in array provided / Number of items filled in array
				AkObjectInfo* out_aObjectInfos		///< Array of AkObjectInfo items to fill
				);

			/// Get positioning information associated to an audio object.
			/// \return AK_Success if succeeded, AK_IDNotFound if the object ID cannot be found, AK_NotCompatible if the audio object cannot expose positioning
			extern AKSOUNDENGINE_API AKRESULT GetPositioningInfo( 
				AkUniqueID in_ObjectID,						///< Audio object ID
				AkPositioningInfo& out_rPositioningInfo		///< Positioning information structure to be filled
				);

			/// List passed to GetActiveGameObjects.
			/// After calling this function, the list will contain the list of all game objects that are currently active in the sound engine.
			/// Being active means that either a sound is playing or pending to be played using this game object.
			/// The caller is responsible for calling Term() on the list when the list is not required anymore
			/// \sa 
			/// - \ref GetActiveGameObjects
			typedef AkArray<AkGameObjectID, AkGameObjectID, ArrayPoolDefault, 32> AkGameObjectsList;

			/// Fill the provided list with all the game object IDs that are currently active in the sound engine.
			/// The function may be used to avoid updating game objects positions that are not required at the moment.
			/// After calling this function, the list will contain the list of all game objects that are currently active in the sound engine.
			/// Being active means that either a sound is playing or pending to be played using this game object.
			/// \sa 
			/// - \ref AkGameObjectsList
			extern AKSOUNDENGINE_API AKRESULT GetActiveGameObjects( 
				AkGameObjectsList& io_GameObjectList	///< returned list of active game objects.
				);

			/// Query if the specified game object is currently active.
			/// Being active means that either a sound is playing or pending to be played using this game object.
			extern AKSOUNDENGINE_API bool GetIsGameObjectActive( 
				AkGameObjectID in_GameObjId ///< Game object ID
				);

			/// Game object and max distance association.
			/// \sa 
			/// - \ref AkRadiusList
			struct GameObjDst
			{
				/// Default constructor
				GameObjDst()
					: m_gameObjID( AK_INVALID_GAME_OBJECT )
					, m_dst( -1.0f )
				{}

				/// Easy constructor
				GameObjDst( AkGameObjectID in_gameObjID, AkReal32 in_dst )
					: m_gameObjID( in_gameObjID )
					, m_dst( in_dst )
				{}

				AkGameObjectID	m_gameObjID;	///< Game object ID
				AkReal32		m_dst;			///< MaxDistance
			};

			/// List passed to GetMaxRadius.
			/// \sa 
			/// - \ref AK::SoundEngine::Query::GetMaxRadius
			typedef AkArray<GameObjDst, const GameObjDst&, ArrayPoolDefault, 32> AkRadiusList;

			/// Returns the maximum distance used in attenuations associated to all sounds currently playing.
			/// This may be used for example by the game to know if some processing need to be performed on the game side, that would not be required
			/// if the object is out of reach anyway.
			///
			/// Example usage:
			/// \code
			/// /*******************************************************/
			/// AkRadiusList RadLst; //creating the list( array ).
			/// // Do not reserve any size for the array, 
			/// // the system will reserve the correct size.
			///
			/// GetMaxRadius( RadLst );
			/// // Use the content of the list
			/// (...)
			///
			/// RadLst.Term();// the user is responsible to free the memory allocated
			/// /*******************************************************/
			/// \endcode
			///
			/// \aknote The returned value is NOT the distance from a listener to an object but
			/// the maximum attenuation distance of all sounds playing on this object. This is
			/// not related in any way to the curent 3D position of the object. \endaknote
			///
			/// \return 
			/// - AK_Success if succeeded
			/// - AK_InsuficientMemory if there was not enough memory
			///
			/// \aknote 
			/// The Scaling factor (if one was specified on the game object) is included in the return value.
			/// The Scaling factor is not updated once a sound starts playing since it 
			/// is computed only when the playback starts with the initial scaling factor of this game object. Scaling factor will 
			/// be re-computed for every playback instance, always using the scaling factor available at this time.
			/// \endaknote
			///
			/// \sa 
			/// - \ref AkRadiusList
			extern AKSOUNDENGINE_API AKRESULT GetMaxRadius(
				AkRadiusList & io_RadiusList	///< List that will be filled with AK::SoundEngine::Query::GameObjDst objects.
				);

			/// Returns the maximum distance used in attenuations associated to sounds playing using the specified game object.
			/// This may be used for example by the game to know if some processing need to be performed on the game side, that would not be required
			/// if the object is out of reach anyway.
			/// 
			/// \aknote The returned value is NOT the distance from a listener to an object but the maximum attenuation distance of all sounds playing on this object. \endaknote
			///
			/// \return
			/// - A negative number if the game object specified is not playing.
			/// - 0, if the game object was only associated to sounds playing using no distance attenuation ( like 2D sounds ).
			/// - A positive number represents the maximum of all the distance attenuations playing on this game object.
			///
			/// \aknote 
			/// The Scaling factor (if one was specified on the game object) is included in the return value.
			/// The Scaling factor is not updated once a sound starts playing since it 
			/// is computed only when the playback starts with the initial scaling factor of this game object. Scaling factor will 
			/// be re-computed for every playback instance, always using the scaling factor available at this time.
			/// \endaknote
			///
			/// \sa 
			/// - \ref AK::SoundEngine::SetAttenuationScalingFactor
			extern AKSOUNDENGINE_API AkReal32 GetMaxRadius(
				AkGameObjectID in_GameObjId ///< Game object ID
				);

		} //namespace Query
	} //namespace SoundEngine
} //namespace AK

#endif // _AK_QUERYPARAMS_H_
