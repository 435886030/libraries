//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

/// \file 
/// Memory allocation macros for Wwise sound engine plug-ins. 

#ifndef _IAKPLUGINMEMALLOC_H_
#define _IAKPLUGINMEMALLOC_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkMemoryMgr.h> // For AK_MEMDEBUG

namespace AK
{
	/// Interface to memory allocation
	/// \warning The functions in this interface are not thread-safe, unless stated otherwise.
	///
	/// \akcaution SDK users should never call these function directly, but use memory allocation macros instead. \endakcaution
	/// \sa 
	/// - \ref fx_memory_alloc
	class IAkPluginMemAlloc
	{
	protected:
		/// Virtual destructor on interface to avoid warnings.
		virtual ~IAkPluginMemAlloc(){}

	public:
		
#if defined (AK_MEMDEBUG)
	    /// Debug malloc.
		/// \sa 
		/// - \ref fx_memory_alloc
	    virtual void * dMalloc( 
            size_t	 in_uSize,		///< Allocation size
            const char*  in_pszFile,///< Allocation file name (for tracking purposes) (Ansi string)
		    AkUInt32 in_uLine		///< Allocation line number (for tracking purposes)
		    ) = 0;

#else
	    /// Release malloc.
		/// \sa 
		/// - \ref fx_memory_alloc
	    virtual void * Malloc( 
            size_t in_uSize		///< Allocation size
            ) = 0;
#endif
	    /// Free allocated memory.
		/// \sa 
		/// - \ref fx_memory_alloc
        virtual void Free(
            void * in_pMemAddress	///< Allocated memory start address
            ) = 0;
	};
}

// Memory allocation macros to be used by sound engine plug-ins.
#if defined (AK_MEMDEBUG)
/// Declare this macro in the public interface of your IAkPlugin class to be able 
/// to use memory allocation macros for plug-ins.
/// \sa
/// - \ref fx_memory_alloc
#define AK_USE_PLUGIN_ALLOCATOR() \
		private: \
            static void * operator new( size_t size ); \
        public: \
            static void * operator new( size_t size, AK::IAkPluginMemAlloc * in_pAllocator, char* ptcFile, AkUInt32 ulLine ) \
            { \
                return in_pAllocator->dMalloc( size, ptcFile, ulLine ); \
            } \
            static void   operator delete( void* pvMem, AK::IAkPluginMemAlloc * in_pAllocator, char* ptcFile, AkUInt32 ulLine ) \
            { \
	            in_pAllocator->Free( pvMem ); \
            } \
            static void   operator delete( void* pvMem, size_t size ) \
            { \
            }
#else
/// Declare this macro in the public interface of your plug-in class to be able 
/// to use memory allocation macros for plug-ins.
/// \sa
/// - \ref fx_memory_alloc
#define AK_USE_PLUGIN_ALLOCATOR() \
        public: \
            static void * operator new( size_t size, AK::IAkPluginMemAlloc * in_pAllocator ) \
            { \
                return in_pAllocator->Malloc( size ); \
            } \
            static void   operator delete( void* pvMem, AK::IAkPluginMemAlloc * in_pAllocator ) \
            { \
	            in_pAllocator->Free( pvMem ); \
            } \
            static void   operator delete( void* , size_t ) \
            { \
            }
#endif


#if defined (AK_MEMDEBUG)
	#define AK_PLUGIN_NEW(_allocator,_what)	            new((_allocator),__FILE__,__LINE__) _what
	#define AK_PLUGIN_ALLOC(_allocator,_size)           (_allocator)->dMalloc((_size),__FILE__,__LINE__)
#else
	/// Macro used to allocate objects.
	/// \param _allocator Memory allocator interface.
	/// \param _what Desired object type. 
	/// \return A pointer to the newly-allocated object.
	/// \aknote Use AK_PLUGIN_DELETE() for memory allocated with this macro. \endaknote
	/// \sa
	/// - \ref fx_memory_alloc
	/// - AK_PLUGIN_DELETE()
	#define AK_PLUGIN_NEW(_allocator,_what)	            new(_allocator) _what
	/// Macro used to allocate arrays of built-in types.
	/// \param _allocator Memory allocator interface.
	/// \param _size Requested size in bytes.
	/// \return A void pointer to the the allocated memory.
	/// \aknote Use AK_PLUGIN_FREE() for memory allocated with this macro. \endaknote
	/// \sa
	/// - \ref fx_memory_alloc
	/// - AK_PLUGIN_FREE()
	#define AK_PLUGIN_ALLOC(_allocator,_size)           (_allocator)->Malloc((_size))
#endif

/// Macro used to deallocate objects.
/// \param _allocator Memory allocator interface.
/// \param _what A pointer to the allocated object.
/// \sa
/// - \ref fx_memory_alloc
/// - AK_PLUGIN_NEW()
#define AK_PLUGIN_DELETE(_allocator,_what)      delete (_allocator,_what); (_allocator)->Free((_what))

/// Macro used to deallocate arrays of built-in types.
/// \param _allocator Memory allocator interface.
/// \param _pvmem A void pointer to the allocated memory.
/// \sa
/// - \ref fx_memory_alloc
/// - AK_PLUGIN_ALLOC()
#define AK_PLUGIN_FREE(_allocator,_pvmem)       (_allocator)->Free((_pvmem))

#endif // _IAKPLUGINMEMALLOC_H_
