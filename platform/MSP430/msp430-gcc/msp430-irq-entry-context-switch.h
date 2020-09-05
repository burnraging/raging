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

//! @file     msp430-irq-entry-context-switch.h
//! @authors  Bernie Woodland
//! @date     27Oct18
//!
//! @brief     MSP430 IRQ entry point handler context switch inline fcns
//!
//!
//! @details   Rules for coding MSP430 Interrupt Handlers with NUFR API calls
//!
//! @details   Any IRQ handler which either makes NUFR API calls or
//! @details   which unlocks interrupts we'll call a "Stackable Handler".
//! @details   A Stackable Handler consists of an IRQ entry point handler
//! @details   and a C IRQ handler. The IRQ entry point handler hooks into
//! @details   the 0xFFxx vector address and gets invoked when the
//! @details   interrupt occurs. The C IRQ handler is called from the IRQ
//! @details   entry point handler.
//! @details   
//! @details   1) An IRQ entry point handler and a C IRQ handler must be used
//! @details   for any Stackable Handler.
//! @details   2) All Stackable Handler entry point handler must contain the
//! @details   following:
//! @details     (a) The function definition must have the gcc function attributes
//! @details         as 'interrupt' and 'naked'
//! @details     (b) The entry point handler must call msp430_irq_entry_pre() first
//! @details     (c) The entry point hander must invoke the C IRQ handler second
//! @details     (d) The entry point handler must call msp430_irq_entry_post() third
//! @details     (e) There cannot be any other content other than this in the
//! @details         entry point handler for Stackable Handlers.
//! @details   3) Interrupt handlers which are not Stackable Handlers should
//! @details   not follow these rules. The non-Stackable Handlers must keep
//! @details   interrupts locked throughout the duration of the handler.
//! @details   4) The Stackable Handler C IRQ Handler must contain the following:
//! @details     (a) It can be an ordinary C function or an assembly language
//! @details         function.
//! @details     (b) As a C function, any callee-saved registers which get
//! @details         used will be pushed onto the stack. Assembly language
//! @details         handler functions must do the same.
//! @details     (c) Must call msp430_irq_c_prelude() early in the call,
//! @details         before unlocking interrupts.
//! @details     (d) Must call msp430_irq_c_context_switch_conditional()
//! @details         last thing in the call.
//! @details     (e) msp430_irq_c_context_switch_conditional() return value
//! @details         will be passed to caller and become return value of 
//! @details         C IRQ Handler
//!
//! @details   Prototype example:
//! @details   
//! @details   __attribute__((interrupt(SOME_IRQ_NUMBER), naked)) void some_handler(void)
//! @details   {
//! @details       msp430_irq_entry_pre();
//! @details   
//! @details       c_handler();
//! @details   
//! @details       msp430_irq_entry_post();
//! @details   }
//! @details   
//! @details   uint16_t c_handler(void)
//! @details   {
//! @details       uint16_t                tm_ctl_reg;
//! @details       uint32_t                new_timeout;
//! @details       nsvc_timer_callin_return_t callin_rv;
//! @details       bool                    rv;
//! @details       _IMPORT_STATUS_REG_TYPE saved_sr;
//! @details   
//! @details       msp430_irq_c_prelude();
//! @details   
//! @details       saved_sr = _IMPORT_INTERRUPT_ENABLE();
//! @details   
//! @details       // Replace below with NUFR API of your choice
//! @details       nufr_bop_send_with_key_override(SOME_TASK_TID);
//! @details   
//! @details       rv = msp430_irq_c_context_switch_conditional(saved_sr);
//! @details   
//! @details       return rv;
//! @details   }
//!

#ifndef MSP430_IRQ_ENTRY_CONTEXT_SWITCH_H_
#define MSP430_IRQ_ENTRY_CONTEXT_SWITCH_H_

#include "raging-global.h"
#include "msp430-base.h"

//!
//! @name      msp430_irq_entry_pre
//!
//! @brief     Pre inline assembler code for all IRQ handlers
//! @brief     which make NUFR calls.
//!
//! @details   If an IRQ handler doesn't make a NUFR call, this
//! @details   must not be added.
//!
__attribute__((always_inline)) inline void msp430_irq_entry_pre(void)
{
    // The interrupt pushed PC, SR. Complete pushing of caller-saved
    // registers, since called C fcn. will clobber them.
#if   defined(CS_MSP430X_20BIT)
    // Must insert a 2-byte slot, so that if we do a context switch, we'll
    // have room to change PC, SR frame that ISR pushed into a NUFR
    // frame (CALLA-type frame)
    __asm volatile(
        "  suba     #2, SP\n\t"   // adjust for CALLA compatibility.
                                  // NOTE: 16-bit add on SR, since SR is < 64k
        "  pushm.a  #5, R15"      // push R15-R11, with R15 pushed first
        :);
#elif defined(CS_MSP430X_16BIT)
    __asm volatile(
        "  pushm.w  #5, R15"      // push R15-R11, with R15 pushed first
        :);
#else
    __asm volatile(
        "  push.w   R15\n\t"
        "  push.w   R14\n\t"
        "  push.w   R13\n\t"
        "  push.w   R12\n\t"
        "  push.w   R11"
        :);
#endif
}

//!
//! @name      msp430_irq_entry_pre
//!
//! @brief     Post inline assembler code for all IRQ handlers
//!
__attribute__((always_inline)) inline void msp430_irq_entry_post(void)
{
    extern unsigned            **msp_switchin_sp;
    extern unsigned            **msp_switchout_sp;

    // If return value == false, no context switch to be done;
    // goto 'no_switch:'
    __asm volatile goto(
        "  cmp.b   #0, R12\n\t"
        "  jeq     %l[no_switch]"
        :
        :
        :
        : no_switch);

#if   defined(CS_MSP430X_20BIT)
    // The IRQ stack frame of PC, SR must be converted to a CALLA-type
    // stack frame. See comments in 'Prepare_Stack.c'
    //  NOTE:
    //     Interrupts get re-enabled when SR is popped in
    //     'msp430asm_task_context_switch()'
    __asm volatile(
        "  mova    #0, R12\n\t"     // FIXME: Not sure if this instruction is needed.
                                    // Intent is to clear bits 19:16 in R12, in case mov.w
                                    // instruction which follows doesn't clear them.
        "  movx.w  22(SP), R12\n\t" // R12 bits 15:0 = [PC 19:6] + [SR 11:0] pushed by IRQ.
                                    // R12 bits 19:16 should be zeros
        "  movx.w  24(SP), R13\n\t" // R13 = PC bits 15:0 pushed by IRQ
                                    // R13 bits 19:16...not sure, and don't matter
        "  movx.w  R12, 20(SP)\n\t" //   20(SP) is pre-allocated slot for SR
                                    //   NOTE: pre-allocated slot will pick up bits 16:19 of PC,
                                    //         but these will be dropped when pushed back into SR
        "  swpbx.w R12\n\t"         // swap nibbles (15:8/7:0), while clearing 19:16
        "  rram.a  #4, R12\n\t"     // R12 = PC bits 19:16 only, MSB-justified
                                    // All values saved little endian,
                                    //    so offset 22 is PC LSB and offset 24 is PC MSB.
        "  bic.w   #0xFFF0, R12\n\t"  // Clear non-interesting bits (
        "  movx.w  R13, 22(SP)\n\t" // save PC bits 15:0 back to frame in CALLA format
        "  movx.w  R12, 24(SP)"     // save right-justified PC bits 16:19 back to frame in CALLA format
        :);
#endif

// Push callee-saved registers.
// Prepare for context switch
#if   defined(CS_MSP430X_20BIT)
    __asm volatile(
        "  pushm.a #7, R10\n\t"     // push R4-R10, with R10 pushed first
        "  mova    &msp_switchout_sp, R12\n\t" // R12: msp_switchout_sp
        "  movx.a  SP, @R12\n\t"    // *msp_switchout_sp = SP
        "  mova    &msp_switchin_sp, R12\n\t"  // R12: msp_switchin_sp
        "  mova    @R12, SP"        // SP = *msp_switchin_sp
        :);
#else
    #if defined(CS_MSP430X_16BIT)
        __asm volatile(
            "  pushm.w #7, R10"     // push R4-R10, with R10 pushed first
            :);
    #else
        __asm volatile(
            "  push.w  R10\n\t"
            "  push.w  R9\n\t"
            "  push.w  R8\n\t"
            "  push.w  R7\n\t"
            "  push.w  R6\n\t"
            "  push.w  R5\n\t"
            "  push.w  R4"
            :);
    #endif

    __asm volatile(
        "  mov.w  &msp_switchout_sp, R12\n\t"
        "  mov.w  SP, @R12\n\t"
        "  mov.w  &msp_switchin_sp, R12\n\t"
        "  mov.w  @R12, SP"
        :);
#endif

// Perform context switch
#if   defined(CS_MSP430X_20BIT)
    __asm volatile(
        "  popm.a  #12, R15\n\t"  // pop r4-R15. R4 is popped first.
        "  popx.w  SR\n\t"
        "  nop\n\t"               // suppress warning
        "  reta"                  // switch to switchin task
        :);
#elif defined(CS_MSP430X_16BIT)
    __asm volatile(
        "  popm.w  #12, R15\n\t"  // pop r4-R15
        "  popx.w  SR\n\t"
        "  nop\n\t"               // suppress warning
        "  ret"                   // switch to switchin task
        :);
#else
    __asm volatile(
        "  pop.w   R4\n\t"
        "  pop.w   R5\n\t"
        "  pop.w   R6\n\t"
        "  pop.w   R7\n\t"
        "  pop.w   R8\n\t"
        "  pop.w   R9\n\t"
        "  pop.w   R10\n\t"
        "  pop.w   R11\n\t"
        "  pop.w   R12\n\t"
        "  pop.w   R13\n\t"
        "  pop.w   R14\n\t"
        "  pop.w   R15\n\t"
        "  pop.w   SR\n\t"
        "  nop\n\t"               // suppress warning
        "  ret"                   // switch to switchin task
        :);
#endif
    // 'ret/a' instruction above: we never get here as a result

no_switch:

#if defined(CS_MSP430X_20BIT)
    __asm volatile(
        "  popm.a  #5, R15\n\t"   // pop R11-R15
        "  adda    #2, SP\n\t"
        "  reti"                  // reti will re-enable interrupts
        :);
#elif defined(CS_MSP430X_16BIT)
    __asm volatile(
        "  popm.w  #5, R15\n\t"   // pop R11-R15
        "  reti"
        :);
#else
    __asm volatile(
        "  pop.w   R11\n\t"
        "  pop.w   R12\n\t"
        "  pop.w   R13\n\t"
        "  pop.w   R14\n\t"
        "  pop.w   R15\n\t"
        "  reti"
        :);
#endif
}



#endif //MSP430_IRQ_ENTRY_CONTEXT_SWITCH_H_