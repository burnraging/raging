/*
Copyright (c) 2018, Bernie Woodland
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

//! @file     raging-global.h
//! @authors  Bernie Woodland
//! @date     7Aug17

#ifndef _RAGING_GLOBAL_H_
#define _RAGING_GLOBAL_H_


// fixme: add cpu include file

// Is Visual C compiler (__INTEL_COMPILER check, since Intel Compiler
//   uses this switch also)?
#if _MSC_VER && !__INTEL_COMPILER
    #define VISUAL_C_COMPILER
#endif

// Compiling for C++
#ifdef __cplusplus
    #include <stddef.h>       // for 'ptrdiff_t'

    #include <stdint.h>
    //#include <stdbool.h>


    //#define STD_BOOL_BULL

    #define RAGING_EXTERN_C_START  extern "C" {
    #define RAGING_EXTERN_C_END               }

// C99 compliant?
#elif __STDC_VERSION__ >= 199901L
    #include <stddef.h>       // for 'ptrdiff_t'

    #include <stdint.h>
    #include <stdbool.h>

    // C or C++ compilation on gcc?
    #if defined(__GNUC__)
        // When gcc is optimized for codespace, it won't inline 'inline' fcns
        #define INLINE inline __attribute__((always_inline))

    // GCC uses these flags:
    // #ifdef __i386__
    // #ifdef __arm__
    // #ifdef __msp430__  (???)

    #else
        #define INLINE inline
    #endif

    #define RAGING_EXTERN_C_START
    #define RAGING_EXTERN_C_END

// ANSI C only
#else
    #include <stddef.h>       // for 'ptrdiff_t'

    // fixme...adjust for CPU type, etc
    typedef unsigned char       uint8_t;
    typedef unsigned short     uint16_t;
    typedef unsigned           uint32_t;
    typedef unsigned long long uint64_t;   // this might not work everywhere
    typedef          char        int8_t;
    typedef          short      int16_t;
    typedef          int        int32_t;
    typedef          long long  int64_t;   // this might not work everywhere

    #ifdef VISUAL_C_COMPILER
        #define STD_BOOL_BULL
    #else
        #include <stdbool.h>
    #endif

    // defined for Visual C compilers, other pre C99 compilers unknown
    #define INLINE __inline

    #define RAGING_EXTERN_C_START
    #define RAGING_EXTERN_C_END
#endif

    // std bool bull
#ifdef STD_BOOL_BULL
    #ifndef __cplusplus
        typedef unsigned char          bool;
    #endif

    #ifndef true
        #define true  1
    #endif
    #ifndef false
        #define false 0
    #endif
#endif

// Mandatory includes
#include <stddef.h>

// Byte and Bit Sizing

#define BIT_00             0x00000001
#define BIT_01             0x00000002
#define BIT_02             0x00000004
#define BIT_03             0x00000008
#define BIT_04             0x00000010
#define BIT_05             0x00000020
#define BIT_06             0x00000040
#define BIT_07             0x00000080
#define BIT_08             0x00000100
   //add more as needed...

#define BIT_MASK_NIBBLE          0x0F
#define BIT_MASK8                0xFF
#define BIT_MASK16             0xFFFF
#define BIT_MASK32         0xFFFFFFFF

#define BITS_PER_NIBBLE            4
#define BITS_PER_WORD8             8
#define BITS_PER_WORD16           16
#define BITS_PER_WORD32           32
#define BITS_PER_WORD64           64

#define BYTES_PER_WORD8            1
#define BYTES_PER_WORD16           2
#define BYTES_PER_WORD32           4
#define BYTES_PER_WORD64           8

#define NIBBLES_PER_WORD32         8
#define NIBBLES_PER_WORD64        16

#define BYTES_1K                1024
#define BYTES_1M     (BYTES_1K * BYTES_1K)

// Time  & Conversions

#define MILLISECS_PER_SEC       1000
#define SECS_PER_MINUTE           60
#define SECS_PER_HOUR           3600
#define SECS_PER_DAY           86400
#define SECS_PER_YEAR       31536000   // assumes a 365 day year
#define HOURS_PER_DAY             24
#define HOURS_PER_YEAR          8760   // assumes a 365 day year

// Basic Macros
#define ARRAY_SIZE(x)           ( sizeof(x)/sizeof(x[0]) )
#define OFFSETOF(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))
#define PTR_TO_UINT32(ptr)      ((uint32_t)(ptr))

  //fixme: might need to be tweaked for IAR
#define BITWISE_NOT8(x)          ((uint8_t)~((uint8_t)x))
#define BITWISE_NOT16(x)         ((uint16_t)~((uint16_t)x))
#define BITWISE_NOT32(x)         ((uint32_t)~((uint32_t)x))
#define BITWISE_NOT64(x)         ((uint64_t)~((uint64_t)x))

     // 'true' if any single bit in 'bits' is == 1
#define ANY_BITS_SET(var, bits)  ( ((var) & (bits)) != 0 )
     // 'true' if all bits in 'bits' are == 1
#define ARE_BITS_SET(var, bits)  ( ((var) & (bits)) == (bits) )
     // 'true' if all bits in 'bits' are == 0
#define ARE_BITS_CLR(var, bits)  ( ((var) & (bits)) == 0 )
     // 'true' if any single bit in 'bits' is == 1
#define ANY_BITS_CLR(var, bits)  ( ((var) & (bits)) != (bits) )

// If > 1 bits are set in 'x', returns a single bit set, always the LSB 
#define BIT_LSB_SET32(x)         ((uint32_t)(x) & (BITWISE_NOT32(x) + 1))

// Oh, what a pain these ptr sizes are! Only 'ptrdiff_t' works everywhere.
// On __x86_64__, they need to be 'long'.
//   (causes compiler error "cast ... loses precision")
// On msp430, they need to be
//   either 'unsigned' or 'long', depending on whether 16 or 20 bit
#define IS_ALIGNED16(x)              ( ((ptrdiff_t)(x) & 1) == 0 )
#define IS_ALIGNED32(x)              ( ((ptrdiff_t)(x) & 3) == 0 )
#define IS_ALIGNED64(x)              ( ((ptrdiff_t)(x) & 7) == 0 )
#define ALIGN16(x)                   ( ((ptrdiff_t)(x) | 1) - 1 )
#define ALIGN32(x)                   ( ((ptrdiff_t)(x) | 3) - 3 )
#define ALIGN64(x)                   ( ((ptrdiff_t)(x) | 7) - 7 )
#define ALIGNUP16(x)                 ( ((ptrdiff_t)(x) + (ptrdiff_t)1u) & ~((ptrdiff_t)1u) )
#define ALIGNUP32(x)                 ( ((ptrdiff_t)(x) + (ptrdiff_t)3u) & ~((ptrdiff_t)3u) )
#define ALIGNUP64(x)                 ( ((ptrdiff_t)(x) + (ptrdiff_t)7u) & ~((ptrdiff_t)7u) )

#define ROUND_DOWN(input, element_size)  ( (input) / (element_size) )
#define ROUND_UP(input, element_size)    ( ((input) + (element_size) - 1) / (element_size) )

// if 'x' hits 'upperBound', reset it to zero
#define WRAP(x, upperBound) ( (x) == (upperBound)? 0 : (x))

// Universal failure codes. Used when return value is a signed int and negative
// values are reserved for fail codes.
#define RFAIL_ERROR           (-1)    // General error
#define RFAIL_OVERRUN         (-2)    // Buffer being read/write overrun
#define RFAIL_UNSUPPORTED     (-3)    // Operation was valid, but we don't support it
#define RFAIL_NOT_FOUND       (-4)    // Not found condition


/**
 * 	@brief This allows marking of a variable as UNUSED preventing warnings.
 *
 *	This macro provides a way to purposely avoid compiler warnings / errors when
 *	not using a variable.  This method provides clear indication that the variable
 *	in question is not ready to be used, or currently not in use.
 *
 *	@params[in] x The unused variable to hide from the compiler warning / error check.
 *
 *	@code{.c}
 *	void a_method(uint32_t used_parameter, uint32_t expansion_parameter)
 *	{
 *	 	UNUSED(expansion_parameter);
 *		switch(used_parameter)
 *		{
 *			...
 *		}
 *	}
 *
 *	@endcode
 */
#ifndef UNUSED
 #define UNUSED(x) ((void)(x))
#endif

 /**
  *  @brief This allows setting a breakpoint in code.
  *
  *	 The BREAKPOINT macro allows a simple way to force an Arm BKPT instruction.
  *	 The BKPT instruction causes the processor to enter Debug state.  Debug tools
  *	 can use this to investigate system state when the instruction at a particular
  *	 address is reached.
  *
  *  @params[in] 'x' The breakpoint number in hexidecimal
  *
  *	 @code{.c}
  *
  *	 ErrorCode result = API_Call_To_Do_Something(msg);
  *	 if(SUCCESS == result)
  *  {
  * 		...
  *  }
  *	 else
  *	 {
  *		BREAKPOINT(0x01);
  *	 }
  *
  *  @endcode
  *
  */
#ifndef BREAKPOINT
  #define BREAKPOINT(x) (asm volatile ("bkpt"#x)) 
#endif

#endif 
