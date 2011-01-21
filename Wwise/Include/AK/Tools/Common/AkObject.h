//////////////////////////////////////////////////////////////////////
//
// AkObject.h
//
// Base class for object that use dynamic allocation.
// Overloads new and delete to call those of the memory manager.
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#ifndef _AK_OBJECT_H_
#define _AK_OBJECT_H_

#include <AK/SoundEngine/Common/AkMemoryMgr.h>

extern AKSOUNDENGINE_API AkMemPoolId g_DefaultPoolId;
extern AKSOUNDENGINE_API AkMemPoolId g_LEngineDefaultPoolId;

//-----------------------------------------------------------------------------
// Placement New definition. Use like this:
// AkPlacementNew( memorybuffer ) T(); // where T is your type constructor
//-----------------------------------------------------------------------------

/// Placement New definition. 
struct AkPlacementNewKey 
{ 
	/// ctor 
	AkPlacementNewKey(){} 
};

AkForceInline void * operator new( size_t /*size*/, void * memory, const AkPlacementNewKey & /*key*/ )
{
      return memory;
}

#define AkPlacementNew(_memory) ::new( _memory, AkPlacementNewKey() )

// Matching operator delete for AK placement new. This needs to be defined to avoid compiler warnings
// with projects built with exceptions enabled.
AkForceInline void operator delete( void *, void *, const AkPlacementNewKey & ) {}

//-----------------------------------------------------------------------------
// Macros
//-----------------------------------------------------------------------------
#if defined (AK_MEMDEBUG)
	#define AkNew(_pool,_what)				new((_pool),__FILE__,__LINE__) _what
	#define AkAlloc(_pool,_size)			(AK::MemoryMgr::dMalloc((_pool),_size,__FILE__,__LINE__))
	#define AkNew2(_ptr,_pool,_type,_what)	{ _ptr = (_type *) AK::MemoryMgr::dMalloc((_pool),sizeof(_type),__FILE__,__LINE__); if ( _ptr ) AkPlacementNew( _ptr ) _what; }
	#define AkMalign(_pool,_size,_align)	(AK::MemoryMgr::dMalign((_pool),_size,_align, __FILE__,__LINE__))
#else
	#define AkNew(_pool,_what)				new((_pool)) _what
	#define AkAlloc(_pool,_size)			(AK::MemoryMgr::Malloc((_pool),_size))
	#define AkNew2(_ptr,_pool,_type,_what)	{ _ptr = (_type *) AK::MemoryMgr::Malloc((_pool),sizeof(_type)); if ( _ptr ) AkPlacementNew( _ptr ) _what; }
	#define AkMalign(_pool,_size,_align)	(AK::MemoryMgr::Malign((_pool),_size,_align))
#endif

#define AkFree(_pool,_pvmem)				(AK::MemoryMgr::Free((_pool),(_pvmem)))
#define AkFalign(_pool,_pvmem)				(AK::MemoryMgr::Falign((_pool),(_pvmem)))
#define AkDelete2(_pool,_type,_what)	{ _what->~_type(); AK::MemoryMgr::Free( _pool, _what ); }

//-----------------------------------------------------------------------------
// Name: Class CAkObject
// Desc: Base allocator object.
//-----------------------------------------------------------------------------

#if defined (AK_WIN)
#pragma warning (disable : 4291)
#endif // defined(AK_WIN)
class CAkObject
{
public:

#if defined (AK_MEMDEBUG)
	static AkForceInline void * operator new(size_t size,AkMemPoolId in_PoolId,char* szFile,AkUInt32 ulLine)
	{
		return AK::MemoryMgr::dMalloc( in_PoolId,size,szFile,ulLine );
	}
#else
	/// Member new operator
    static AkForceInline void * operator new(size_t size,AkMemPoolId in_PoolId)
	{
		return AK::MemoryMgr::Malloc( in_PoolId, size );
	}
#endif

	/// Destructor
    virtual ~CAkObject( ) { }

private:
    static void * operator new(size_t size); // Illegal.
};

AkForceInline void AkDelete( AkMemPoolId in_PoolId, CAkObject * in_pObject )
{
	if ( in_pObject )
	{
		in_pObject->~CAkObject();
		AK::MemoryMgr::Free( in_PoolId, in_pObject );
	}
}

#endif // _AK_OBJECT_H_
