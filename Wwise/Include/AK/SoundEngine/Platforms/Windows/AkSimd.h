//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkSimd.h

/// \file 
/// Data type definitions.

#ifndef _AK_SIMD_PLATFORM_H_
#define _AK_SIMD_PLATFORM_H_

#include <xmmintrin.h>
#include <AK/SoundEngine/Common/AkTypes.h>

// Platform specific defines for prefetching

#define AKSIMD_ARCHCACHELINESIZE	(64)				///< Assumed cache line width for architectures on this platform
#define AKSIMD_ARCHMAXPREFETCHSIZE	(512) 				///< Use this to control how much prefetching maximum is desirable (assuming 8-way cache)		
/// Cross-platform memory prefetch of effective address assuming non-temporal data
#define AKSIMD_PREFETCHMEMORY( __offset__, __add__ ) _mm_prefetch(((char *)(__add__))+(__offset__), _MM_HINT_NTA ) 

// Platform specfic defines for SIMD operations

#define AkReal32Vector									__m128										///< Cross-platform SIMD vector declaration.
#define AKSIMD_GETELEMENT( __vName, __num__ )			(__vName).m128_f32[(__num__)]				///< Retrieve scalar element from vector.
#define AKSIMD_LOAD1( __scalar__ )						_mm_load1_ps( &(__scalar__) )				///< Load and splat scalar into vector.
#define AKSIMD_LOADVEC( __addr__ )						_mm_load_ps( (AkReal32*)(__addr__) )					///< Load vector from 16-byte aligned address.
#define AKSIMD_LOADUNALIGNEDVEC( __addr__ )				_mm_loadu_ps( (AkReal32*)(__addr__) )					///< Load vector from unaligned address.
#define AKSIMD_STORE1( __addr__, __vName__ )			_mm_store_ss( (AkReal32*)(__addr__), (__vName__) )		///< Store first vector element to address.
#define AKSIMD_STOREVEC( __addr__, __vName__ )			_mm_store_ps( (AkReal32*)(__addr__), (__vName__) )		///< Store vector to 16-byte aligned address.
#define AKSIMD_STOREUNALIGNEDVEC( __addr__, __vName__ )	_mm_storeu_ps( (AkReal32*)(__addr__), (__vName__) )	///< Store vector to unaligned address.
#define AKSIMD_MUL( __a__, __b__ )						_mm_mul_ps( (__a__), (__b__) )				///< Vector multiply vector operation.
#define AKSIMD_ADD( __a__, __b__ )						_mm_add_ps( (__a__), (__b__) )				///< Vector add vector operation.
#define AKSIMD_SUB( __a__, __b__ )						_mm_sub_ps( (__a__), (__b__) )				///< Vector subtraction operation.
#define AKSIMD_MADD( __a__, __b__, __c__ )				_mm_add_ps( _mm_mul_ps( (__a__), (__b__) ), (__c__)) ///< Vector multiply-add operation.
#define AKSIMD_MAX( __a__, __b__ )						_mm_max_ps( (__a__), (__b__) )				///< Vector max operation.
#define AKSIMD_MIN( __a__, __b__ )						_mm_min_ps( (__a__), (__b__) )				///< Vector min operation.
#define AKSIMD_SQRTE( __a__ )							_mm_sqrt_ps( (__a__) )						///< Vector square root aproximation.
#define AKSIMD_LTEQ( __a__, __b__ )						_mm_cmple_ps( (__a__), (__b__) )			///< Vector <= operation.
#define AKSIMD_SETZERO()								_mm_setzero_ps( )							///< Vector set zero operation. 
#define AKSIMD_SHUFFLE_BADC( __a__ )					_mm_shuffle_ps( (__a__), (__a__), _MM_SHUFFLE(2,3,0,1));	///< Swap the 2 lower floats together and the 2 higher floats together.	
#define AKSIMD_SHUFFLE_CDAB( __a__ )					_mm_shuffle_ps( (__a__), (__a__), _MM_SHUFFLE(1,0,3,2));	///< Swap the 2 lower floats with the 2 higher floats.	

/// Faked in-place vector horizontal add. 
/// \akwarning 
/// Don't expect this to be very efficient. 
/// /endakwarning
static AkForceInline void AKSIMD_HORIZONTALADD(__m128 & vVec)
{   
	__m128 vHighLow = _mm_movehl_ps(vVec, vVec);
	vVec = _mm_add_ps(vVec, vHighLow);
	vHighLow = _mm_shuffle_ps(vVec, vVec, 0x55);
	vVec = _mm_add_ps(vVec, vHighLow);
} 

/// Cross-platform SIMD multiplication of 2 complex data elements with interleaved real and imaginary parts
static AkReal32Vector AKSIMD_COMPLEXMUL( const AkReal32Vector vCIn1, const AkReal32Vector vCIn2 )
{
	static const AkReal32Vector vSign = { 1.f, -1.f, 1.f, -1.f }; 

	AkReal32Vector vTmp1 = _mm_shuffle_ps( vCIn1, vCIn1, _MM_SHUFFLE(2,2,0,0)); 
	vTmp1 = AKSIMD_MUL( vTmp1, vCIn2 );
	AkReal32Vector vTmp2 = _mm_shuffle_ps( vCIn1, vCIn1, _MM_SHUFFLE(3,3,1,1)); 
	vTmp2 = AKSIMD_MUL( vTmp2, vSign );
	vTmp2 = AKSIMD_MUL( vTmp2, vCIn2 );
	vTmp2 = AKSIMD_SHUFFLE_BADC( vTmp2 ); 
	vTmp2 = AKSIMD_ADD( vTmp2, vTmp1 );
	return vTmp2;
}

#endif //_AK_SIMD_PLATFORM_H_

