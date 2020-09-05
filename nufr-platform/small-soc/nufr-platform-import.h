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

//! @file     nufr-platform-import.h
//! @authors  Bernie Woodland
//! @date     19Feb18
//!
//! @brief   NUFR settings which need to be imported from the startup/CPU/BSP
//!
//! @details There are certain NUFR settings which must be defined in the
//! @details startup code. This file contains allows these to be brought
//! @details into NUFR. The file works by having standard import macros,
//! @details prototypes, etc. that are filled out here.

#ifndef NUFR_PLATFORM_IMPORT_H
#define NUFR_PLATFORM_IMPORT_H

#include <stdint.h>

// fixme: where does this get set??
#define   ARM_CORTEX_M

//*************           ARM Cortex M0/3/4    ************
//*******
#ifdef    ARM_CORTEX_M

//!
//! @brief  ARM Cortex Mx imports
//!

// Type sized to ARM Cortex Mx register size
#define _IMPORT_REGISTER_TYPE      uint32_t
#define _IMPORT_STATUS_REG_TYPE    uint32_t

//    Interrupt priority levels
//
//  Assumes ARM configured in a 3-bit priority level in 8-bit word.
//    Bits 7:6     priority
//    bit  5       sub-priority
//
//  Subpriority determines which of 2 interrukpts
//  will execute first if both are of same priority and
//  are pending.
//
//  Levels appended with _SUB are of a lower sub-priority
//  than the same without _SUB.
//
#define _IMPORT_INT_PRI_0       0x00   // highest priority
#define _IMPORT_INT_PRI_0_SUB   0x20
#define _IMPORT_INT_PRI_1       0x40
#define _IMPORT_INT_PRI_1_SUB   0x60
#define _IMPORT_INT_PRI_2       0x80
#define _IMPORT_INT_PRI_2_SUB   0xA0
#define _IMPORT_INT_PRI_3       0xC0   // lowest priority
#define _IMPORT_INT_PRI_3_SUB   0xE0

// FIXME: this is __NVIC_PRIO_BITS in ST files.
// If we use 2 bits on an M0, int_lock value must be changed:
//    "MOV r12, #0x60\n\t"
// ..must become:
//    "MOV r12, #0x40\n\t"
// ..to conform to mask-off level specified in comments in 'bsp_prl_t'
#define _IMPORT_PRIORITY_MASK   0xE0   // bits 7:5 only

// Must turn on USE_PRIMASK in order to get this to build on M0, since
//   BASEPRI isn't supported on M0.
//#define USE_PRIMASK 1
__attribute__((always_inline)) inline _IMPORT_REGISTER_TYPE int_lock(void)
{
#ifdef USE_PRIMASK       // alternate method
    /*
     *  Caller is responsible for storing the returned PRIMASK 
     *
     *  1.) Get primary mask value transfered to R12
     *  2.) disable interrupts
     *  3.) store primary mask value from R12 in result
     *  4.) return primary mask value to caller.
     */
     // Skipping "__asm volatile" as volatile adds additional, unnecessary
     //   instructions. No bad side effects making it volatile.
     // Had problems with optimizer moving code that was inside of lock to
     //   outside of lock and vice-versa. This was happening in both
     //   "fast" and "size" builds. Establishing a sequence point with
     //   inline assembler solved this problem. Using "memory" keyword established
     //   sequence point, so did 'volatile', but skipping volatile.
     //   See articles about sequence points:
     //   https://stackoverflow.com/questions/22106843/gccs-reordering-of-read-write-instructions
     //   https://stackoverflow.com/questions/22106843/gccs-reordering-of-read-write-instructions/32536664#32536664
     //   https://gcc.gnu.org/onlinedocs/gcc/Volatiles.html
     // Adding "dsb" not necessary. Article:
     //   http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.faqs/ka14041.html
     //
     // **** 28Dec19 NOTE:
     // *** Was getting an assembler error on the M0 when trying to save old PRIMASK
     // *** value and when there were 2 locks in the same fcn. call.
     // *** NESTING LOCKS WILL NOT WORK WITH THIS!!
     // *** Also, was getting an error on M0 when attempting to use registers > R6
     // ****   (M0 has restrictions on high registers)

    _IMPORT_REGISTER_TYPE result = 0;
    __asm(          "CPSID I" ::: "memory" );

// Old code.
// This handles nesting.
// Code uses R6 instead of R12 b/c M0 can't use R12 b/c 
//     of upper register restrictions.
// This won't compile on an M0 b/c of bug in gcc where strb uses an
// offset or immediate operand which is > 0x19. This problem
// only happens when 2 int locks are used in the same fcn. call.
//
//    _IMPORT_REGISTER_TYPE result = 0;
//    __asm(          "MRS R6, PRIMASK\n\t"
//                    "CPSID I\n\t"
//                    "STRB R6, %[output]"
//           : [output] "=m" (result) :: "r6", "memory");

#else                    // preferred method
    // BASEPRI method. Selectively mask by priority, rather
    //   than turning all interrupts off. Only interrupts
    //   set to a priority of _IMPORT_INT_PRI_0 will not
    //   be masked off
    _IMPORT_REGISTER_TYPE result;
    __asm(          "MRS r12, BASEPRI\n\t"
                    "STRB r12, %[output]\n\t"
                    "MOV r12, #0x60\n\t"
                    "MSR BASEPRI, r12"

           : [output] "=m" (result) :: "r12", "memory");
#endif           

    
    return result;
}

__attribute__((always_inline)) inline void int_unlock(_IMPORT_REGISTER_TYPE status)
{
#ifdef USE_PRIMASK
    /*
     *  Caller is responsible for passing in a valid PRIMASK status
     * 
     *  1.) Load the status into R12 from status
     *  2.) Transfer status into PRIMASK
     */
    __asm(          "CPSIE  I" ::: "memory" );

    (void)status;        // quench warning

#else
    /*
     *  Caller is responsible for passing in a valid BASEPRI status
     * 
     *  1.) Load the status into R12 from status
     *  2.) Transfer status into BASEPRI
     */
    __asm(          "ldrb r12, %[input]\n\t"
                    "MSR BASEPRI, r12;\n\t"
           ::[input] "m" (status) : "r12", "memory");
#endif
}


#define _IMPORT_INTERRUPT_LOCK         int_lock
#define _IMPORT_INTERRUPT_UNLOCK(y)    int_unlock(y)


//  Clock rate (Hz)
//  For convenience only. This is the value used on QEMU.
//   Ignored for disco board (168MHz there)
#define _IMPORT_CPU_CLOCK_SPEED      8000000

//!
//! @brief   PendSV trigger  
//! @details http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0497a/Cihfaaha.html
//!
#define _IMPORT_PENDSV_ACTIVATE  *(volatile uint32_t *)(0xE000ED04) = 0x10000000

//!
//! @brief   Alternative Context Switching
//!
//! @details For CPUs which have no software interrupt capability.
//! @details Left empty for ARM Cortex M.
//!
#define _IMPORT_ALT_CONTEXT_SWITCH

//!
//! @brief   SysTick Preprocessing
//!
//! @details For CPUs which don't have interrupt priorities
//! @details Left empty for ARM Cortex M.
//!
#define _IMPORT_SYSTICK_PREPROCESSING

//!
//! @brief   SysTick Postprocessing
//!
//! @details For CPUs which don't have interrupt priorities
//! @details Left empty for ARM Cortex M.
//!
#define _IMPORT_SYSTICK_POSTPROCESSING

// For task launching
typedef struct
{
    _IMPORT_REGISTER_TYPE  *stack_base_ptr;
    _IMPORT_REGISTER_TYPE  **stack_ptr_ptr;
    unsigned   stack_length_in_bytes;
    void       (*entry_point_fcn_ptr)(unsigned);
    void       (*exit_point_fcn_ptr)(void);
    unsigned   entry_parameter;
} _IMPORT_stack_specifier_t;

//!
//! @brief  CPU-specific Stack Prepration
//!
#define _IMPORT_PREPARE_STACK       Prepare_Stack
//  Prototype for above define
void Prepare_Stack(_IMPORT_stack_specifier_t *ptr);


#else
    #error "No CPU type defined"
#endif

#endif  //NUFR_PLATFORM_IMPORT_H
