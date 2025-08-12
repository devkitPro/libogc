#ifndef __GCTYPES_H__
#define __GCTYPES_H__

/*! \file gctypes.h
\brief Data type definitions

*/

#include "tuxedo/types.h"

#if defined(__gamecube__) && !defined(HW_DOL)
#define HW_DOL
#elif !defined(__gamecube__) && defined(HW_DOL)
#define __gamecube__
#endif

#if defined(__wii__) && !defined(HW_RVL)
#define HW_RVL
#elif !defined(__wii__) && defined(HW_RVL)
#define __wii__
#endif

#if defined(__gamecube__) == defined(__wii__)
#error "Invalid platform selected"
#endif

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

/*+----------------------------------------------------------------------------------------------+*/
// fixed point math typedefs
typedef s16 sfp16;					///< signed 8:8 fixed point
typedef s32 sfp32;					///< signed 20:8 fixed point
typedef u16 ufp16;					///< unsigned 8:8 fixed point
typedef u32 ufp32;					///< unsigned 24:8 fixed point
/*+----------------------------------------------------------------------------------------------+*/
typedef float f32;
typedef double f64;
/*+----------------------------------------------------------------------------------------------+*/
typedef volatile float vf32;
typedef volatile double vf64;
/*+----------------------------------------------------------------------------------------------+*/


/*+----------------------------------------------------------------------------------------------+*/
// alias type typedefs
#define FIXED s32					///< Alias type for sfp32
/*+----------------------------------------------------------------------------------------------+*/
#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN  3412
#endif /* LITTLE_ENDIAN */
/*+----------------------------------------------------------------------------------------------+*/
#ifndef BIG_ENDIAN
#define BIG_ENDIAN     1234
#endif /* BIG_ENDIAN */
/*+----------------------------------------------------------------------------------------------+*/
#ifndef BYTE_ORDER
#define BYTE_ORDER     BIG_ENDIAN
#endif /* BYTE_ORDER */
/*+----------------------------------------------------------------------------------------------+*/


//!	argv structure
/*!	\struct __argv

	structure used to set up argc/argv

*/
struct __argv {
	int argvMagic;		//!< argv magic number, set to 0x5f617267 ('_arg') if valid
	char *commandLine;	//!< base address of command line, set of null terminated strings
	int length;//!< total length of command line
	int argc;
	char **argv;
	char **endARGV;
};

//!	Default location for the system argv structure.
extern struct __argv *__system_argv;

// argv struct magic number
#define ARGV_MAGIC 0x5f617267

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif /* TYPES_H */


/* END OF FILE */
