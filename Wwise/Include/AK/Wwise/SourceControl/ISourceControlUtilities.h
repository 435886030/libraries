//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

/// \file
/// Wwise source control plug-in utilities interface, used to create custom dialogs, display the progress dialog, and get
/// the registry path needed to save the plug-in configuration.

#ifndef _AK_WWISE_ISOURCECONTROLUTILITIES_H
#define _AK_WWISE_ISOURCECONTROLUTILITIES_H

#include <AK/SoundEngine/Common/AkTypes.h>

#include "ISourceControlDialogBase.h"
#include "ISourceControlOperationProgress.h"

// Audiokinetic namespace
namespace AK
{
	// Audiokinetic Wwise namespace
	namespace Wwise
	{
		/// Wwise source control utilities interface. This interface is provided when the plug-in is initialized.
		/// With this interface, you can display a progress dialog, create custom dialogs, display message boxes, and
		/// save the plug-in configuration to the registry.
		class ISourceControlUtilities
		{
		public:
			/// Get a pointer to an AK::Wwise::ISourceControlOperationProgress interface, so you can display a simple progress dialog for the operation.
			/// \warning This function is not thread-safe.
			/// \return A pointer to an AK::Wwise::ISourceControlOperationProgress interface.
			virtual ISourceControlOperationProgress* GetProgressDialog() = 0;

			/// This function does the same thing as the standard ::MessageBox function, except that this one will
			/// be displayed with the Wwise UI look and feel.
			/// \warning This function is not thread-safe.
			/// \return The window results of the dialog
			virtual int MessageBox( 
				HWND in_hWnd,					///< The window handle of the dialog
				LPCWSTR in_pszText,				///< The text to be displayed in the message box
				LPCWSTR in_pszCaption,			///< The caption of the message box
				UINT in_uiType					///< The window message box type (e.g. MB_OK)
				) = 0;

			/// This function show a dialog with a edit field and allow the user enter input string
			/// \warning This function is not thread-safe.
			/// \return The window results of the dialog: IDOK or IDCANCEL
			virtual int PromptMessage( 
				HWND in_hWnd,					///< The window handle of the dialog
				LPCWSTR in_pszText,				///< The text to be displayed in the message box
				LPCWSTR in_pszCaption,			///< The caption of the message box
				LPWSTR out_pszInput,			///< The buffer to receive the user input
				UINT in_uiInputSize,			///< The size of the buffer to receive input
				bool in_bIsPassword				///< True to hide text; used for passwords
				) = 0;
	
			/// Show a browse for folder dialog.  
			/// \warning This function is not thread-safe.
			/// \return The resulting path is set in out_pszChoosenPath
			/// \return True if user clicked OK, false if user clicked Cancel
			virtual bool ShowBrowseForFolderDialog(
				LPCWSTR in_pszDialogTitle,			///< The dialog title
				LPWSTR out_pszChoosenPath,			///< The choosen path
				UINT in_uiChoosenPathSize,			///< The size of the buffer to receive path (out_pszChoosenPath)
				LPCWSTR in_pszRootPath = NULL		///< The root path for the browse for folder dialog
				) = 0;

			/// This function does the same thing as the CDialog::DoModal function.
			/// \warning This function is not thread-safe.
			/// \return The window results of the dialog (e.g. IDOK)
			virtual INT_PTR CreateModalCustomDialog( 
				ISourceControlDialogBase* in_pDialog	///< A pointer to a dialog class that implements 
														///< AK::Wwise::ISourceControlDialogBase functions.
				) = 0;

			/// Get the path to the registry for the current project. This path is to be used with
			/// the HKEY_CURRENT_USER registry key.
			/// \warning This function is not thread-safe.
			/// \return A string containing the registry path
			virtual LPCWSTR GetRegistryPath() = 0;

			/// Get the relative path of an audio file in its language directory.  
			/// \warning This function is not thread-safe.
			/// \return Nothing as return value.  The out_pszRelativePath will contain the path.
			virtual void GetSourceRelativePath( 
				LPCWSTR in_pszFullPath,			///< The full path of the audio source file
				LPWSTR out_pszRelativePath,		///< A pointer to the array that receives the path
				UINT in_uiRelativePathSize				///< The size of the array that receives the path
				) = 0;
		};							  
	}
}

#endif // _AK_WWISE_ISOURCECONTROLUTILITIES_H