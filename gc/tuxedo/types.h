#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdalign.h>

/*! @name Integer types
	@brief Traditional shorthand form of integer types defined in `stdint.h`.
	@{
*/

typedef uint8_t  u8;       //!<  8-bit unsigned integer.
typedef uint16_t u16;      //!< 16-bit unsigned integer.
typedef uint32_t u32;      //!< 32-bit unsigned integer.
typedef uint64_t u64;      //!< 64-bit unsigned integer.
typedef uintptr_t uptr;    //!< Pointer-sized unsigned integer.

typedef int8_t  s8;        //!<  8-bit signed integer.
typedef int16_t s16;       //!< 16-bit signed integer.
typedef int32_t s32;       //!< 32-bit signed integer.
typedef int64_t s64;       //!< 64-bit signed integer.
typedef intptr_t sptr;     //!< Pointer-sized signed integer.

typedef volatile u8  vu8;  //!<  8-bit volatile unsigned integer.
typedef volatile u16 vu16; //!< 16-bit volatile unsigned integer.
typedef volatile u32 vu32; //!< 32-bit volatile unsigned integer.
typedef volatile u64 vu64; //!< 64-bit volatile unsigned integer.
typedef volatile uptr vuptr; //!< Pointer-sized volatile unsigned integer.

typedef volatile s8  vs8;  //!<  8-bit volatile signed integer.
typedef volatile s16 vs16; //!< 16-bit volatile signed integer.
typedef volatile s32 vs32; //!< 32-bit volatile signed integer.
typedef volatile s64 vs64; //!< 64-bit volatile signed integer.
typedef volatile sptr vsptr; //!< Pointer-sized volatile signed integer.

//! @}

//! @brief Disables struct member alignment ("packs" a struct).
#define MK_PACKED     __attribute__((packed))
//! @brief Marks something as deprecated
#define MK_DEPRECATED __attribute__((deprecated))
//! @brief Marks a small, utility function whose contents will be inserted into the caller ("inlined").
#define MK_INLINE     __attribute__((always_inline)) static inline
//! @brief Marks a function as never being eligible for automatic inlining by the compiler
#define MK_NOINLINE   __attribute__((noinline))
//! @brief Marks a function as never returning to the caller
#define MK_NORETURN   __attribute__((noreturn))
//! @brief Marks a function as pure
#define MK_PURE       __attribute__((pure))
//! @brief Marks a function or global as being overridable by other object files
#define MK_WEAK       __attribute__((weak))
//! @brief Macro used to explicitly express that a given variable is unused
#define MK_DUMMY(_x)  (void)(_x)

#if defined(__cplusplus)
#define MK_EXTERN_C       extern "C"
#define MK_EXTERN_C_START MK_EXTERN_C {
#define MK_EXTERN_C_END   }
#else
#define MK_EXTERN_C
#define MK_EXTERN_C_START
#define MK_EXTERN_C_END
#endif

/*! @brief Similar to @ref MK_INLINE, but also marking the function as eligible
	for compile-time evaluation.
	@note When compiling as C++, this macro adds the `constexpr` specifier,
	allowing the defined function to work from constexpr contexts.
	In C, this macro is otherwise identical to @ref MK_INLINE.
*/
#if __cplusplus >= 201402L
#define MK_CONSTEXPR  MK_INLINE constexpr
#else
#define MK_CONSTEXPR  MK_INLINE
#endif

/*! @brief Overrides the alignment requirement of a structure
	@note This macro arises from differences in behavior between C and C++
	regarding the `alignas` specifier. In C++ alignas can be used for this
	purpose, while in C it cannot.
*/
#if __cplusplus >= 201103L
#define MK_STRUCT_ALIGN(_align) alignas(_align)
#else
#define MK_STRUCT_ALIGN(_align) __attribute__((aligned(_align)))
#endif

//! @brief Hints the compiler to optimize the code using the given expression, which is assumed to always be true.
#define MK_ASSUME(_expr) do { if (!(_expr)) __builtin_unreachable(); } while (0)
