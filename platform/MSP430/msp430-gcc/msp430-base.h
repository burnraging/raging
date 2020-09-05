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

//! @file     msp430-base.h
//! @authors  Bernie Woodland
//! @date     26Sep18
//!
//! @brief    MSP430 top-level definitions
//!

#ifndef MSP430_BASE_H_
#define MSP430_BASE_H_

#include "raging-global.h"
#include <msp430.h>

// From GCC manual, MSP430 compile options:
// -mlarge Use large-model addressing (20-bit pointers, 32-bit size_t).
// -msmall Use small-model addressing (16-bit pointers, 16-bit size_t).
//
// Our compiler will pass in
//   -D LARGE_MODEL      for -mlarge
//   -D SMALL_MODEL      for -msmall
// From what I can see (https://forum.43oh.com/topic/5444-msp430-gcc-does-it-support-20-bit-addressing/),
//   when compiled as large, everything is 32-bits

#ifdef LARGE_MODEL
    #define CS_MSP430X_20BIT

#elif SMALL_MODEL
    // These flags in in430.h and msp430f5529.h. Also see:
    //      http://e2e.ti.com/support/microcontrollers/msp430/f/166/p/57974/206420
    #if defined(__MSP430_HAS_MSP430XV2_CPU__)  || defined(__MSP430_HAS_MSP430X_CPU__)
        #define CS_MSP430X_16BIT
    #else
        // Both switches off defaults to 16-bit non-extended instructions
    #endif

#else
    #error  "Make must explicitly set one of these compile switches: -mlarge, -msmall"
#endif



//#ifdef CS_MSP430X_20BIT
//    typedef uint32_t msp430_reg_t;
//
//
//#else
//    typedef uint16_t msp430_reg_t;
//#endif

// 'ptrdiff_t' will be 2 bytes or 4 bytes, depending on large or
//    small compilation model.
// Unfortunately, gcc makes sizeof(unsigned)==2 for both small
//    and large models
typedef ptrdiff_t msp430_reg_t;

// Status register is always 16 bits, even for large model
typedef uint16_t msp430_sr_reg_t;


#endif  // MSP430_BASE_H_