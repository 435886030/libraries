//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#ifndef _AK_AKASSERT_H_
#define _AK_AKASSERT_H_

#if ! defined( AKASSERT )

	#include <AK/SoundEngine/Common/AkTypes.h> //For AK_Fail/Success
	#include <assert.h>

	#if ! defined ( VERIFY )
		#define VERIFY(x)	((void)(x))
	#endif

	#if defined( _DEBUG )

		#if defined( __SPU__ )

			#define AKASSERT(Condition) assert(Condition)

		#else // defined( __SPU__ )

			#ifndef AK_ASSERT_HOOK
			typedef void (*AkAssertHook)( 
									const char * in_pszExpression,	///< Expression
									const char * in_pszFileName,	///< File Name
									int in_lineNumber				///< Line Number
									);
			#define AK_ASSERT_HOOK
			#endif

			extern AkAssertHook g_pAssertHook;

			#if defined( RVL_OS )

					inline void _AkAssertHook(
									bool bcondition,
									const char * in_pszExpression,
									const char * in_pszFileName,
									int in_lineNumber
									)
					{
						if( !bcondition )
							g_pAssertHook( in_pszExpression, in_pszFileName, in_lineNumber);
					}

					#define AKASSERT(Condition) if ( g_pAssertHook )   \
													_AkAssertHook((bool)(Condition), #Condition, __FILE__, __LINE__); \
												else                                \
													assert(Condition)
			#elif defined( __APPLE__ )
			#include <CoreServices/CoreServices.h>
					inline void _MacAssert( const char * in_pFunc , const char * in_pFile , unsigned in_LineNum, const char * in_pCondition )
					{
						printf ("%s:%u:%s failed assertion `%s'\n", in_pFile, in_LineNum, in_pFunc, in_pCondition);
						Debugger();
					}

					#define _AkAssertHook(_Expression) ( (_Expression) || (g_pAssertHook( #_Expression, __FILE__, __LINE__), 0) )

					#define AKASSERT(Condition) if ( g_pAssertHook )   \
												_AkAssertHook(Condition);          \
					else                                \
												(__builtin_expect(!(Condition), 0) ? _MacAssert(__func__, __FILE__, __LINE__, #Condition) : (void)0)	

			#else // defined( RVL_OS )
				
				#define _AkAssertHook(_Expression) ( (_Expression) || (g_pAssertHook( #_Expression, __FILE__, __LINE__), 0) )

				#define AKASSERT(Condition) if ( g_pAssertHook )   \
												_AkAssertHook(Condition);          \
											else                                \
												assert(Condition)

			#endif // defined( RVL_OS )

		#endif // defined( __SPU__ )

		#define AKVERIFY AKASSERT

	#else // defined( _DEBUG )

		#define AKASSERT(Condition) ((void)0)
		#define AKVERIFY(x) (x)

	#endif // defined( _DEBUG )

	#define AKASSERT_RANGE(Value, Min, Max) (AKASSERT(((Value) >= (Min)) && ((Value) <= (Max))))

	#define AKASSERTANDRETURN( __Expression, __ErrorCode )\
		if (!(__Expression))\
		{\
			AKASSERT(__Expression);\
			return __ErrorCode;\
		}\

	#define AKASSERTPOINTERORFAIL( __Pointer ) AKASSERTANDRETURN( __Pointer != NULL, AK_Fail )
	#define AKASSERTSUCCESSORRETURN( __akr ) AKASSERTANDRETURN( __akr == AK_Success, __akr )

	#define AKASSERTPOINTERORRETURN( __Pointer ) \
		if ((__Pointer) == NULL)\
		{\
			AKASSERT((__Pointer) == NULL);\
			return ;\
		}\

#endif // ! defined( AKASSERT )

#endif //_AK_AKASSERT_H_

