#ifndef __GCTYPES_H__
#define __GCTYPES_H__
/*+----------------------------------------------------------------------------------------------+*/
// typedef's borrowed from the HAM development kit for Gameboy Advance
// See www.ngine.de for information about this fantastic devkit!
/*+----------------------------------------------------------------------------------------------+*/

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

// bool is a standard type in cplusplus, but not in c.
#ifndef __cplusplus
typedef unsigned char bool;                     ///< For c++ compatibility
#endif
/*+----------------------------------------------------------------------------------------------+*/
typedef unsigned char u8;                       ///< 8bit unsigned integer
typedef unsigned short u16;                 ///< 16bit unsigned integer
typedef unsigned int u32;                       ///< 32bit unsigned integer
typedef unsigned long long u64;             ///< 64bit unsigned integer
/*+----------------------------------------------------------------------------------------------+*/
typedef signed char s8;                         ///< 8bit signed integer
typedef signed short s16;                   ///< 16bit signed integer
typedef signed int s32;                         ///< 32bit signed integer
typedef signed long long s64;               ///< 64bit signed integer
/*+----------------------------------------------------------------------------------------------+*/
typedef volatile unsigned char vu8;             ///< 8bit unsigned volatile't integer
typedef volatile unsigned short vu16;       ///< 16bit unsigned volatile't integer
typedef volatile unsigned int vu32;             ///< 32bit unsigned volatile't integer
typedef volatile unsigned long long vu64;   ///< 64bit unsigned volatile't integer
/*+----------------------------------------------------------------------------------------------+*/
typedef volatile signed char vs8;               ///< 8bit signed volatile't integer
typedef volatile signed short vs16;         ///< 16bit signed volatile't integer
typedef volatile signed int vs32;               ///< 32bit signed volatile't integer
typedef volatile signed long long vs64;     ///< 64bit signed volatile't integer
/*+----------------------------------------------------------------------------------------------+*/
// fixed point math typedefs
typedef s16 sfp16;                              ///< 1:7:8 fixed point
typedef s32 sfp32;                              ///< 1:19:8 fixed point
typedef u16 ufp16;                              ///< 8:8 fixed point
typedef u32 ufp32;                              ///< 24:8 fixed point
/*+----------------------------------------------------------------------------------------------+*/
typedef float f32;
typedef double f64;
/*+----------------------------------------------------------------------------------------------+*/
typedef volatile float vf32;
typedef volatile double vf64;
/*+----------------------------------------------------------------------------------------------+*/
typedef unsigned int BOOL;
/*+----------------------------------------------------------------------------------------------+*/
// alias type typedefs
#define FIXED s32                               ///< Alias type for sfp32
/*+----------------------------------------------------------------------------------------------+*/
// boolean defines
#ifndef boolean
#define boolean  u8
#endif
/*+----------------------------------------------------------------------------------------------+*/
#ifndef TRUE
#define TRUE 1                                  ///< True
#endif
/*+----------------------------------------------------------------------------------------------+*/
#ifndef FALSE
#define FALSE 0                                 ///< False
#endif
/*+----------------------------------------------------------------------------------------------+*/
#ifndef __cplusplus
#define true  TRUE                              ///< For c++ compatibility
#define false FALSE                             ///< For c++ compatibility
#endif
/*+----------------------------------------------------------------------------------------------+*/
#ifndef NULL
#define NULL			0                        ///< Pointer to 0
#endif
/*+----------------------------------------------------------------------------------------------+*/

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif /* TYPES_H */


/* END OF FILE */
