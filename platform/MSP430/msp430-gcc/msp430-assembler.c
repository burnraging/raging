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

//! @file     msp430-assembler.c
//! @authors  Bernie Woodland
//! @date     20Sep18
//!
//! @brief    Assembly language routines for the MSP430
//!

#include "raging-utils.h"
#include "msp430-peripherals.h"
#include "msp430-assembler.h"
#include "msp430-base.h"
#include "nufr-platform-import.h"
#include "nufr-kernel-task.h"

// Can't use NUFR_SP_OFFSET_IN_TCB because of circular include problem
// NOTE: using 'unsigned' to switch between 2 and 4 for 16 and 20 bit modes
#define SP_OFFSET_IN_TCB    (3 * sizeof(unsigned))

//!
//! @name      msp430asm_get_sr
//!
//! @return    SR register value
//!
msp430_sr_reg_t msp430asm_get_sr(void)
{
    msp430_sr_reg_t _SR;

    __asm volatile(
#if   defined(CS_MSP430X_20BIT)
        "  movx.w  SR, %[status_reg]"
#else
        "  mov.w   SR, %[status_reg]"
#endif
        : [status_reg] "=r" (_SR)
        :
        : );

    return _SR;
}

//!
//! @name      msp430asm_set_sr
//!
//! @param[in] 'SR'--write value
//!
void msp430asm_set_sr(msp430_sr_reg_t _SR)
{
    __asm volatile(
#if   defined(CS_MSP430X_20BIT)
        "  movx.w  %[status_reg], SR\n\t"
#else
        "  mov.w   %[status_reg], SR\n\t"
#endif
        "  nop"                          // suppress warning
        :
        : [status_reg] "r" (_SR)
        : );
}

//!
//! @name      msp430asm_task_context_switch
//!
//! @brief     Task-to-task context switch logic
//!
//! @details   16-bit version. This compile-version requires that
//! @details    'msp430asm_task_context_switch()' get called from
//! @details   a CALL assembly instruction, not a CALLA.
//! @details   
//! @details   20-bit version. This compile-version requires that
//! @details    'msp430asm_task_context_switch()' get called from
//! @details   a CALLA assembly instruction, not a CALL.
//! @details   
//! @details   Kernel selects context switch by setting
//! @details   'msp430_pending_context_switch==true'.
//! @details   Sanity check that 'nufr_running != nufr_ready_list',
//! @details   as there may arise corner-cases where this isn't true.
//! @details   
//! @details   When called from IRQ handler, no context switch
//! @details   will occur, since 'msp430_irq_nest_level' will be > 0
//! @details   
//! @details   When BG Task is being switched in, the SR power mode
//! @details   bits get cleared before SR is restored. Assumption is
//! @details   that BG Task set LPM state, interrupt then woke up
//! @details   system, a task gets switched in, single/multiple tasks
//! @details   run, then BG Task resumes. When BG Task resumes, old SR
//! @details   with cleared power mode bits still stacked, must be cleared.
//! @details   
//! @details   MUST declare fcn. 'naked', to prevent compiler from
//! @details   pushing/popping registers upon entry/exit.
//! @details   No local variables (auto class) in this
//! @details   fcn., since it's declared naked.
//!
__attribute__((naked)) void msp430asm_task_context_switch(void)
{
    // PC was pushed by fcn. call. We keep that as a part of the task frame.
    // Save switchout task's registers
    __asm volatile(
#if   defined(CS_MSP430X_20BIT)
        "  pushx.w SR\n\t"               // this also saves interrupt lock state
#else
        "  push.w  SR\n\t"               // this also saves interrupt lock state
#endif
        "  bic.w #8, SR\n\t"             // lock interrupts
        "  nop"                          // suppress warning
        :);

    // Check first that it's ok to do context switch
    // if (!msp430_pending_context_switch || (0 != msp430_irq_nest_level) ||
    //     (nufr_running == nufr_ready_list))
    // {
    //     goto abort_switch;
    // }
    //
    // msp430_pending_context_switch = false;
    __asm volatile goto(
#if   defined(CS_MSP430X_20BIT)
        "  mova   #msp430_pending_context_switch, R11\n\t" // R11 = &msp430_pending_context_switch
        "  movx.b @R11, R12\n\t"         // R12 = msp430_pending_context_switch
        "  cmp.w  #0, R12\n\t"           // msp430_pending_context_switch==false?
        "  jeq    %l[abort_switch]\n\t"  // yes--goto abort_switch
        "  movx.w &msp430_irq_nest_level, R13\n\t"  // R13 = msp430_irq_nest_level
        "  cmp.w  #0, R13\n\t"           // msp430_irq_nest_level==0?
        "  jne    %l[abort_switch]\n\t"       // no--goto abort_switch
        "  mova   #nufr_running, R12\n\t"     // R12 = &nufr_running
        "  mova   #nufr_ready_list, R13\n\t"  // R13 = &nufr_ready_list
        "  mova   @R12, R14\n\t"         // R14 = nufr_running
        "  mova   @R13, R13\n\t"         // R13 = nufr_ready_list
        "  cmpa   R14, R13\n\t"
#else
        "  mov.w  #msp430_pending_context_switch, R11\n\t"
        "  mov.b  @R11, R12\n\t"
        "  cmp.w  #0, R12\n\t"
        "  jeq    %l[abort_switch]\n\t"
        "  mov.w  &msp430_irq_nest_level, R13\n\t"
        "  cmp.w  #0, R13\n\t"
        "  jne    %l[abort_switch]\n\t"
        "  mov.w  #nufr_running, R12\n\t"
        "  mov.w  #nufr_ready_list, R13\n\t"
        "  mov.w  @R12, R14\n\t"
        "  mov.w  @R13, R13\n\t"
        "  cmp.w  R14, R13\n\t"
#endif
        "  jeq    %l[abort_switch]\n\t"  // take branch to skip context switch
        "  mov.b  #0, @R11"              // msp430_pending_context_switch = 0
        :
        : 
        :
        : abort_switch);

    // R12: &nufr_running
    // R13: nufr_ready_list
    // R14: nufr_running

    __asm volatile(
#if   defined(CS_MSP430X_20BIT)
        "  pushm.a #12, R15\n\t"         // pushes R15-R4, R15 first
        "  adda    #12, R14\n\t"         // R14 = &(nufr_running->stack_ptr)
        "  mova    SP, @R14"             // nufr_running->stack_ptr = SP
#else
    #if defined(CS_MSP430X_16BIT)
        "  pushm.w #12, R15\n\t"         // pushes R15-R4, R15 first
    #else
        "  push.w  R15\n\t"
        "  push.w  R14\n\t"
        "  push.w  R13\n\t"
        "  push.w  R12\n\t"
        "  push.w  R11\n\t"
        "  push.w  R10\n\t"
        "  push.w  R9\n\t"
        "  push.w  R8\n\t"
        "  push.w  R7\n\t"
        "  push.w  R6\n\t"
        "  push.w  R5\n\t"
        "  push.w  R4\n\t"
    #endif
        "  add.w   #6,  R14\n\t"
        "  mov.w   SP,  @R14"
#endif
        :);

    // R12: &nufr_running
    // R13: nufr_ready_list
    // [not needed] R14 = &(nufr_running->stack_ptr)

    __asm volatile goto(
#if   defined(CS_MSP430X_20BIT)
        "  cmpa   #0, R13\n\t"           // NULL == nufr_ready_list?
        "  jne    %l[switchin_isnt_bg_task]\n\t" // no--take branch
        "  mova   #nufr_bg_sp, R13\n\t"  // R13 = &nufr_bg_sp[0]
        "  movx.a R13, @R12\n\t"         // nufr_running <== &nufr_bg_sp[0]
        "  adda   #12, R13\n\t"          // R13: &(nufr_bg_sp-> stack_ptr), which is &(nufr_bg_sp[3])
        "  mova   @R13, R14\n\t"         // R14: nufr_bg_sp->stack_ptr, which is nufr_bg_sp[3]

        // Switchin task is BG Task: BG task will likely have enabled a
        // LPM mode while running. Clear all power mode bits in SR before
        // SR is popped.
        // TBD: might have to cancel CPUOF, so not to clobber app
        //      settings, and deal with bit settings at app level.

        // Offset 48 bytes into switchin frame
        // 48 <== 12 reg's * 4 bytes each
        "  bicx.w #0x00F0, 48(R14)\n\t"  // 0x00F0 == SCG1+SCG0+OSCOFF+CPUOF

#else
        "  cmp.w  #0, R13\n\t"
        "  jne    %l[switchin_isnt_bg_task]\n\t"
        "  mov.w  #nufr_bg_sp, R13\n\t"
        "  mov.w  R13, @R12\n\t"
        "  add.w  #6, R13\n\t"
        "  mov.w  @R13, R14\n\t"

        // Offset 24 bytes to SR on stack
        // 24 <== 12 reg's * 2 bytes each
        "  bic.w  #0x00F0, 24(R14)\n\t"

#endif
        "  jmp    %l[after_setting_nufr_running]"
        :
        :
        :
        : switchin_isnt_bg_task, after_setting_nufr_running
        );

    // R12: &nufr_running
    // R13: &(nufr_bg_sp->stack_ptr)
    // R14: nufr_bg_sp->stack_ptr

switchin_isnt_bg_task:

    // R12: &nufr_running
    // R13: nufr_ready_list
    // [not needed] R14 = &(nufr_running->stack_ptr)

    __asm volatile(
#if   defined(CS_MSP430X_20BIT)
        "  movx.a R13, @R12\n\t"         // nufr_running <== nufr_ready_list
        "  adda   #12, R13\n\t"          // R13: &(nufr_ready_list->stack_ptr)
        "  mova   @R13, R14"             // R14: nufr_ready_list->stack_ptr
#else
        "  mov.w  R13, @R12\n\t"
        "  add.w  #6, R13\n\t"
        "  mov.w  @R13, R14"
#endif
        :);

    // R12: &nufr_running
    // R13: &(nufr_ready_list->stack_ptr)
    // R14: nufr_ready_list->stack_ptr

after_setting_nufr_running:

    // globals:
    // nufr_running->stack_ptr updated to SP of switchout task
    // nufr_running has been updated to either nufr_ready_list or nufr_bg_sp
    //
    // registers:
    // [not needed] R12: &nufr_running
    // [not needed] R13: &(nufr_ready_list/nufr_bg_sp -> stack_ptr)
    // R14: nufr_ready_list/nufr_bg_sp -> stack_ptr

    __asm volatile(
#if   defined(CS_MSP430X_20BIT)
        "  mova   R14, SP\n\t"           // SP <== switchin task SP:
                                         //        nufr_ready_list/nufr_bg_sp -> stack_ptr
        "  popm.a #12, R15\n\t"          // pop R15-R4, R4 first
        "  popx.w SR\n\t"
        "  nop\n\t"                      // suppress warning
        "  reta"
#else
        "  mov.w  R14, SP\n\t"

    #if defined(CS_MSP430X_16BIT)
        "  popm.w #12, R15\n\t"          // pop R15-R4, R4 first
    #else
        "  pop.w  R4\n\t"
        "  pop.w  R5\n\t"
        "  pop.w  R6\n\t"
        "  pop.w  R7\n\t"
        "  pop.w  R8\n\t"
        "  pop.w  R9\n\t"
        "  pop.w  R10\n\t"
        "  pop.w  R11\n\t"
        "  pop.w  R12\n\t"
        "  pop.w  R13\n\t"
        "  pop.w  R14\n\t"
        "  pop.w  R15\n\t"
    #endif

        "  pop.w  SR\n\t"
        "  nop\n\t"                      // suppress warning
        "  ret"
#endif
        :);

abort_switch:
    // Exit path if context switch not taken.
    // Uncommon from task level, always from ISR level.
    __asm volatile(
#if   defined(CS_MSP430X_20BIT)
        "  popx.w SR\n\t"               // restore interrupt lock setting
        "  nop\n\t"                     // suppress warning
        "  reta"
#else
        "  pop.w SR\n\t"                // restore interrupt lock setting
        "  nop\n\t"                     // suppress warning
        "  ret"
#endif
        :);
}