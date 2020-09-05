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

/**
 *  @file       raging-contract.h
 *  @author     Christopher R. Martin
 *  @version    1.0.0
 *  @date       26 November 2016
 *  @brief      This file provides access to the design by contract macros.
 *
 *
 *  @details    Designed based on the following blog post:
 *  @details    http://www.barrgroup.com/Embedded-Systems/How-To/Design-by-Contract-for-Embedded-Software
 *
 */

#ifndef _RAGING_CONTRACT_H_
#define _RAGING_CONTRACT_H_

#include <stdint.h>

#include "nufr-platform-app-compile-switches.h"


#ifndef DOXYGEN_SHOULD_SKIP_THIS

/**
 *  @brief This function is used by the design by contract macros: REQUIRE, ENSURE, INVARIANT.
 *
 *  This function is called by the CONTRACT_ASSERT that is the base macro utilized by the rest
 *  of the design by contract macros.  This function will notify the developer of a failed
 *  precondition( REQUIRE ), postcondition( ENSURE ) or invariance( INVARIANT ).  This method
 *  is customizable, but does provide the filename and line number of the failed contract.
 *
 *  @param[in]  file    The filename of the file where the failed contract occurred.
 *  @param[in]  line    The line number of the failed contract.
 */
RAGING_EXTERN_C_START
void onContractFailure(uint8_t* file, uint32_t line);
RAGING_EXTERN_C_END

/**
 *  @brief This is the base macro used by the design by contract macros.
 *
 *  This macro provides a way to call the onContractFailure method that is used with
 *  the design by contract macros.  This is the base macro that actually does the
 *  test of the expression for truth.  The other macros are just aliases to this macro.
 *
 */
#define CONTRACT_ASSERT(_c_test_)   ( (_c_test_) ? (void)0 : onContractFailure((uint8_t*)__FILE__, __LINE__))

#endif

/**
 *  @brief This macros allows marking of a concept as a required precondition.
 *
 *  This macro provides a way to purposely mark an concept in code as a necessary
 *  precondition in the system.  If the expression passed to this macro is false, there will
 *  be a clear indication to the developer that the precondition has failed.
 *
 *  Three dimensions of these macros are provided. They are:
 *
 *    KERNEL_xxx         For use in the NUFR kernel, NUFR platform layers
 *    SL_xxx             For use in the NUFR Services Layer
 *    APP_xxx            For use in an application or any non-NUFR code
 *    UT_xxx             For use in the Unit Test/PC/offline environment
 *                       Not to be used in QEMU or target F/W
 *
 *    xxx_REQUIRE_xxx    Assertion is a required precondition
 *                       Condition must be true in order for subsequent logic
 *                       block to operate correctly.
 *    xxx_ENSURE_xxx     Assertion is a required postcondition
 *                       If previous logic block executed correctly, then
 *                       assertion will be true.
 *    xxx_INVARIANT_xxx  Assertion is an invariable condition
 *                       No contingent logic block; the condition is independent.
 *
 *    xxx_IL             For use in an interrupt lock (Critical Section).
 *    xxx_API            To validate the parameters of an API function call
 *
 *  @param  x   The expression that is a required precondition.
 *
 *  @code{.c}
 *  void a_method(uint8_t* ptrToRequiredObject)
 *  {
 *      APP_REQUIRE_API(NULL != ptrToRequiredObject);
 *      ...
 *  }
 *
 *  @endcode
 *
 */
#ifndef NUFR_ASSERT_LEVEL
    #define KERNEL_REQUIRE_IL(x)
    #define KERNEL_ENSURE_IL(x)
    #define KERNEL_INVARIANT_IL(x)

    #define KERNEL_REQUIRE(x)
    #define KERNEL_ENSURE(x)
    #define KERNEL_INVARIANT(x)

    #define KERNEL_REQUIRE_API(x)

    #define SL_REQUIRE_IL(x)
    #define SL_ENSURE_IL(x)
    #define SL_INVARIANT_IL(x)

    #define SL_REQUIRE(x)
    #define SL_ENSURE(x)
    #define SL_INVARIANT(x)

    #define SL_REQUIRE_API(x)

    #define APP_REQUIRE_IL(x)
    #define APP_ENSURE_IL(x)
    #define APP_INVARIANT_IL(x)

    #define APP_REQUIRE(x)
    #define APP_ENSURE(x)
    #define APP_INVARIANT(x)

    #define APP_REQUIRE_API(x)

    // Same as UNUSED(x), but
    //   this one suppresses warnings only when asserts are disabled
    #define UNUSED_BY_ASSERT(x)          ((void)(x))

#else
    #if NUFR_ASSERT_LEVEL == 9
        #define KERNEL_REQUIRE_IL(x)     CONTRACT_ASSERT(x)
        #define KERNEL_ENSURE_IL(x)      CONTRACT_ASSERT(x)
        #define KERNEL_INVARIANT_IL(x)   CONTRACT_ASSERT(x)
    #else
        #define KERNEL_REQUIRE_IL(x)
        #define KERNEL_ENSURE_IL(x)
        #define KERNEL_INVARIANT_IL(x)
    #endif

    #if NUFR_ASSERT_LEVEL >= 8
        #define KERNEL_REQUIRE(x)        CONTRACT_ASSERT(x)
        #define KERNEL_ENSURE(x)         CONTRACT_ASSERT(x)
        #define KERNEL_INVARIANT(x)      CONTRACT_ASSERT(x)
    #else
        #define KERNEL_REQUIRE(x)
        #define KERNEL_ENSURE(x)
        #define KERNEL_INVARIANT(x)
    #endif

    #if NUFR_ASSERT_LEVEL >= 7
        #define KERNEL_REQUIRE_API(x)    CONTRACT_ASSERT(x)
    #else
        #define KERNEL_REQUIRE_API(x)
    #endif

    #if NUFR_ASSERT_LEVEL >= 6
        #define SL_REQUIRE_IL(x)         CONTRACT_ASSERT(x)
        #define SL_ENSURE_IL(x)          CONTRACT_ASSERT(x)
        #define SL_INVARIANT_IL(x)       CONTRACT_ASSERT(x)
    #else
        #define SL_REQUIRE_IL(x)
        #define SL_ENSURE_IL(x)
        #define SL_INVARIANT_IL(x)
    #endif

    #if NUFR_ASSERT_LEVEL >= 5
        #define SL_REQUIRE(x)            CONTRACT_ASSERT(x)
        #define SL_ENSURE(x)             CONTRACT_ASSERT(x)
        #define SL_INVARIANT(x)          CONTRACT_ASSERT(x)
    #else
        #define SL_REQUIRE(x)
        #define SL_ENSURE(x)
        #define SL_INVARIANT(x)
    #endif

    #if NUFR_ASSERT_LEVEL >= 4
        #define SL_REQUIRE_API(x)        CONTRACT_ASSERT(x)
    #else
        #define SL_REQUIRE_API(x)
    #endif

    #if NUFR_ASSERT_LEVEL >= 3
        #define APP_REQUIRE_IL(x)        CONTRACT_ASSERT(x)
        #define APP_ENSURE_IL(x)         CONTRACT_ASSERT(x)
        #define APP_INVARIANT_IL(x)      CONTRACT_ASSERT(x)
    #else
        #define APP_REQUIRE_IL(x)
        #define APP_ENSURE_IL(x)
        #define APP_INVARIANT_IL(x)
    #endif

    #if NUFR_ASSERT_LEVEL >= 2
        #define APP_REQUIRE(x)           CONTRACT_ASSERT(x)
        #define APP_ENSURE(x)            CONTRACT_ASSERT(x)
        #define APP_INVARIANT(x)         CONTRACT_ASSERT(x)
    #else
        #define APP_REQUIRE(x)
        #define APP_ENSURE(x)
        #define APP_INVARIANT(x)
    #endif

    #if NUFR_ASSERT_LEVEL >= 1
        #define APP_REQUIRE_API(x)       CONTRACT_ASSERT(x)
        #define UNUSED_BY_ASSERT(x)
    #else
        #define APP_REQUIRE_API(x)
        #define UNUSED_BY_ASSERT(x)      ((void)(x))
    #endif

#endif


#define UT_REQUIRE(x)            CONTRACT_ASSERT(x)
#define UT_ENSURE(x)             CONTRACT_ASSERT(x)
#define UT_INVARIANT(x)          CONTRACT_ASSERT(x)

#endif   // _RAGING_CONTRACT_H_