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

// Can't include these here--they cause a circular include problem
// due to macros included from 
//#include "msp430-context-switch-assist.h"
//#include <msp430.h>
#include "msp430-base.h"

extern bool     msp430_pending_context_switch;

__attribute__((naked)) void msp430asm_task_context_switch(void);

//!
//! @brief  MSP430 imports
//!

// Type sized to ARM Cortex Mx register size
#define _IMPORT_REGISTER_TYPE      msp430_reg_t
#define _IMPORT_STATUS_REG_TYPE    msp430_sr_reg_t

__attribute__((always_inline)) inline _IMPORT_STATUS_REG_TYPE int_lock(void)
{
    msp430_sr_reg_t result = 0;

    // Clear GIE bit in SR. This disables interrupts. Return old SR value
    // See notes about 'volatile' and "memory" in ARM version
    __asm(
        "mov.w  SR, R14\n\t"
        "bic.w  #8, SR\n\t"
        "nop\n\t"               // suppress warning
#if   defined(CS_MSP430X_20BIT)
        "movx.w R14, %[output]"
#else
        "mov.w  R14, %[output]"
#endif
        : [output] "=m" (result)
        :
        : "R14", "memory");


    return result;
}

__attribute__((always_inline)) inline void int_unlock(msp430_sr_reg_t status)
{

    // 'status' is saved SR: put it back in SR

    __asm(
#if   defined(CS_MSP430X_20BIT)
        "movx.w %[input], SR\n\t"
#else
        "mov.w  %[input], SR\n\t"
#endif
        "nop"                    // suppress warning
        :
        :[input] "m" (status)
        : "memory");
}

// Enable interrupts when interrupts are disabled. Used in an IRQ.
__attribute__((always_inline)) inline _IMPORT_STATUS_REG_TYPE int_enable(void)
{
    msp430_sr_reg_t result = 0;

    // Set GIE bit in SR. This enables interrupts. Return old SR value

    // Getting noise NOP warning from the assembler.
    // [ CC ] simple-main.c
    // /tmp/cct1JWAE.s: Assembler messages:
    // /tmp/cct1JWAE.s:41: Warning: a NOP might be needed here because of successive changes in interrupt state
    // /tmp/cc61nGxG.s: Assembler messages:
    // /tmp/cc61nGxG.s:41: Warning: a NOP might be needed here because of successive changes in interrupt state
    // [ CC ] msp430-assembler.c
    //
    // Reference:
    // http://e2e.ti.com/support/microcontrollers/msp430/f/166/t/182673
    // https://e2e.ti.com/support/development_tools/compiler/f/343/t/649173

    __asm(
        "mov.w  SR, R14\n\t"
        "bis.w  #8, SR\n\t"
        "nop\n\t"           // suppress warning
#if   defined(CS_MSP430X_20BIT)
        "movx.w R14, %[output]"
#else
        "mov.w  R14, %[output]"
#endif
        : [output] "=m" (result)
        :
        : "R14", "memory");

    return result;
}

// Restores from an 'int_enable' call above
__attribute__((always_inline)) inline void int_disable(msp430_sr_reg_t status)
{

    // 'status' is saved SR: put it back in SR

    __asm(
#if   defined(CS_MSP430X_20BIT)
        "movx.w %[input], SR\n\t"
#else
        "mov.w  %[input], SR\n\t"
#endif
        "nop"                        // suppress warning
        :
        :[input] "m" (status)
        : "memory");
}

#define _IMPORT_INTERRUPT_LOCK         int_lock
#define _IMPORT_INTERRUPT_UNLOCK(y)    int_unlock(y)
#define _IMPORT_INTERRUPT_ENABLE       int_enable
#define _IMPORT_INTERRUPT_DISABLE(y)   int_disable(y)


//  Clock rate (Hz)
//  For convenience only.
// (Not used currently: we're configured for tickless mode)
//#define _IMPORT_CPU_CLOCK_SPEED        10

//!
//! @brief   PendSV trigger  
//! @details http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0497a/Cihfaaha.html
//!
#define _IMPORT_PENDSV_ACTIVATE    msp430_pending_context_switch = true

//!
//! @brief   CPU-specific Stack Preparation  
//!
#define _IMPORT_PREPARE_STACK      Prepare_Stack

// For task launching
typedef struct
{
    uint32_t   *stack_base_ptr;
    _IMPORT_REGISTER_TYPE  **stack_ptr_ptr;
    unsigned   stack_length_in_bytes;
    void       (*entry_point_fcn_ptr)(unsigned);
    void       (*exit_point_fcn_ptr)(void);
    unsigned   entry_parameter;
} _IMPORT_stack_specifier_t;

void Prepare_Stack(_IMPORT_stack_specifier_t *ptr);

#endif  //NUFR_PLATFORM_IMPORT_H